// BAKERY — note display and Keep-in-key, using the settled body pitch (Pitch × Tune),
// never the start of a pitch sweep. Scientific pitch: A4 = 440 Hz, C4 = MIDI 60.
#pragma once
#include <cmath>
#include <cstdio>
#include <string>
#include "Params.h"

namespace bk {

enum KeyScale { SC_MAJOR, SC_MINOR, SC_CHROMATIC, SC_MAJPENT, SC_MINPENT, SC_COUNT };
inline const char* scaleName(int s) {
    static const char* n[] = {"Major", "Minor", "Chromatic", "Maj Pent", "Min Pent"};
    return n[(s >= 0 && s < SC_COUNT) ? s : 0];
}
inline const char* pitchClassName(int pc) {
    static const char* n[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return n[((pc % 12) + 12) % 12];
}

inline float midiFromHz(float hz) {
    hz = std::max(hz, 1e-6f);
    return 69.f + 12.f * std::log2(hz / 440.f);
}
inline float hzFromMidi(float midi) { return 440.f * std::exp2((midi - 69.f) / 12.f); }

struct NoteCents {
    int midi = 69;     // nearest MIDI note
    int pc = 9;        // pitch class 0=C .. 11=B
    int octave = 4;    // scientific: MIDI 60 = C4
    float cents = 0;   // signed leftover after rounding to nearest note, typically [-50, 50]
    float hz = 440;
    bool valid = false;
};

inline NoteCents noteCentsFromHz(float hz) {
    NoteCents n;
    if (!(hz > 0) || !std::isfinite(hz)) return n;
    n.hz = hz;
    n.valid = true;
    float m = midiFromHz(hz);
    n.midi = (int)std::lround(m);
    n.cents = (m - (float)n.midi) * 100.f;
    // Keep cents in a tidy range if rounding sat on a boundary.
    if (n.cents >= 50.f) { n.midi += 1; n.cents -= 100.f; }
    if (n.cents < -50.f) { n.midi -= 1; n.cents += 100.f; }
    n.pc = ((n.midi % 12) + 12) % 12;
    n.octave = (int)std::floor(n.midi / 12.0) - 1;
    return n;
}

inline std::string formatNoteCents(const NoteCents& n) {
    if (!n.valid) return "—";
    int c = (int)std::lround(n.cents);
    if (c == 0) return std::string(pitchClassName(n.pc)) + std::to_string(n.octave);
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%s%d %+d cents", pitchClassName(n.pc), n.octave, c);
    return buf;
}

// Settled body pitch: Pitch (Hz) shifted by Tune (semitones). Sweep is ignored.
inline float settledHz(float freq, float tuneSt) {
    float f = freq * std::exp2(tuneSt / 12.f);
    return (std::isfinite(f) && f > 0) ? f : freq;
}

inline bool engineShowsKey(int engine) {
    return engine == E_KICK || engine == E_TOM || engine == E_IMPACT
        || engine == E_COWBELL || engine == E_WOOD || engine == E_METAL;
}
inline bool engineSnapsKey(int engine) {
    return engine == E_KICK || engine == E_TOM || engine == E_IMPACT;
}

// Scale degrees as pitch-class offsets from the root (0–11).
inline const int* scaleDegrees(int scale, int& count) {
    static const int major[] = {0, 2, 4, 5, 7, 9, 11};
    static const int minor[] = {0, 2, 3, 5, 7, 8, 10};
    static const int chrom[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    static const int majp[] = {0, 2, 4, 7, 9};
    static const int minp[] = {0, 3, 5, 7, 10};
    switch (scale) {
        case SC_MINOR: count = 7; return minor;
        case SC_CHROMATIC: count = 12; return chrom;
        case SC_MAJPENT: count = 5; return majp;
        case SC_MINPENT: count = 5; return minp;
        default: count = 7; return major;
    }
}

// Snap a continuous MIDI number to the nearest scale note (or to the root, in any octave).
inline float snapMidi(float midi, int keyPc, int scale, bool toRoot) {
    keyPc = ((keyPc % 12) + 12) % 12;
    if (toRoot) {
        // Nearest MIDI whose pitch class is the key root.
        float base = midi - (float)keyPc;
        float oct = std::round(base / 12.f);
        return (float)keyPc + 12.f * oct;
    }
    int n = 0; const int* deg = scaleDegrees(scale, n);
    float best = midi, bestDist = 1e9f;
    int centre = (int)std::floor(midi / 12.f);
    for (int o = centre - 2; o <= centre + 2; ++o) {
        for (int i = 0; i < n; ++i) {
            float cand = (float)(o * 12 + keyPc + deg[i]);
            float d = std::abs(cand - midi);
            if (d < bestDist) { bestDist = d; best = cand; }
        }
    }
    return best;
}

inline float snapHz(float hz, int keyPc, int scale, bool toRoot) {
    return hzFromMidi(snapMidi(midiFromHz(hz), keyPc, scale, toRoot));
}

// Adjust V_FREQ so settled pitch (freq × 2^(tune/12)) equals the snapped note. Tune is left alone.
// Returns true if freq changed. Does not touch unpitched engines, or any engine when keep is off.
inline bool snapChannelToKey(float* params, int ch, int keyPc, int scale, bool toRoot, bool keepOn) {
    if (!keepOn || !params) return false;
    int eng = (int)std::lround(params[cp(ch, V_ENGINE)]);
    if (!engineSnapsKey(eng)) return false;
    float freq = params[cp(ch, V_FREQ)];
    float tune = params[cp(ch, V_TUNE)];
    float body = settledHz(freq, tune);
    float target = snapHz(body, keyPc, scale, toRoot);
    float newFreq = target / std::exp2(tune / 12.f);
    newFreq = clampToDef(cp(ch, V_FREQ), newFreq);
    if (std::abs(newFreq - freq) < 1e-4f * std::max(1.f, std::abs(freq))) return false;
    params[cp(ch, V_FREQ)] = newFreq;
    return true;
}

inline int applyKeepInKey(float* params) {
    if (!params) return 0;
    bool keep = params[gp(G_KEEPKEY)] > 0.5f;
    int keyPc = (int)std::lround(params[gp(G_KEY)]);
    int scale = (int)std::lround(params[gp(G_SCALE)]);
    bool toRoot = params[gp(G_SNAPMODE)] > 0.5f;
    int n = 0;
    for (int c = 0; c < kChannels; ++c) n += snapChannelToKey(params, c, keyPc, scale, toRoot, keep);
    return n;
}

} // namespace bk
