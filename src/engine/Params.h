// BAKERY — the complete, fixed parameter model (shared by engine, host wrapper, UI, presets, tests).
#pragma once
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "Dsp.h"

namespace bk {
constexpr int kChannels = 12;
constexpr int kFxParams = 7; // type, p1..p5, mix
constexpr int kMasterSlots = 4, kSendSlots = 2;

// ---------- Voice engines ----------
enum Engine { E_KICK, E_SNARE, E_CHAT, E_OHAT, E_CLAP, E_TOM, E_RIM, E_COWBELL, E_WOOD, E_METAL, E_CLICK, E_IMPACT, E_INDUSTRIAL, E_EXPERIMENTAL, E_COUNT };
inline const char* engineName(int e) {
    static const char* n[] = {"Kick", "Snare", "Closed Hat", "Open Hat", "Clap", "Tom", "Rimshot", "Cowbell", "Woodblock", "Metallic", "Click", "Impact", "Industrial", "Experimental"};
    return n[(e >= 0 && e < E_COUNT) ? e : 0];
}
inline const char* engineBlurb(int e) {
    static const char* n[] = {
        "Sine/triangle oscillator with a fast pitch drop, FM, AM, click and a clean sub. The heart of 808/909 kicks.",
        "Two tuned drum-head tones plus filtered noise (the 'wires'), with a separate snap transient.",
        "Six detuned square oscillators (like the 808) plus noise, through band-pass and high-pass filters. Short.",
        "Same metal bank as the closed hat but long, with sustain. A closed hat chokes it (choke group A).",
        "Several fast noise bursts in a row through a band-pass filter, then a short tail: that is a hand clap.",
        "Three membrane modes (1, 1.59, 2.14) with a pitch drop and a resonant shell.",
        "Two high-Q resonators pinged together and clipped, like the 808 rim.",
        "Two square oscillators (540 + 800 Hz on an 808) through a band-pass, with a two-stage decay.",
        "An impulse exciting two short, high-Q wooden modes.",
        "FM bell + ring modulator + inharmonic bar modes for bells, gongs and metal hits.",
        "Tiny filtered impulse and sine blip. Ticks, glitches and percussive clicks.",
        "Huge pitch-dropping boom, noise swell and diffusion: cinematic impacts.",
        "Noise and square through a metallic comb, wavefolder and bit reducer. Aggressive.",
        "Feedback FM, random pitch jumps, noise ring-mod and stutter gate. Unpredictable."};
    return n[e >= 0 && e < E_COUNT ? e : 0];
}

// ---------- Per-channel parameters ----------
enum ChanParam {
    V_ENGINE, V_FREQ, V_TUNE, V_SWEEP, V_SWEEPT, V_ATT, V_DEC, V_SUS, V_REL, V_PUNCH, V_CLICK, V_BODY, V_SUB,
    V_NOISE, V_NCOLOR, V_NDEC, V_HARM, V_SNAP, V_FMAMT, V_FMRATIO, V_RING, V_DRIVE, V_SAT, V_DIST, V_DMODE,
    V_FTYPE, V_CUT, V_RES, V_TONE, V_WAVE, V_MOD, V_WIDTH, V_VSENS, V_LEVEL, V_CHOKE, V_NOTE,
    S_LEVEL, S_SYNTH, S_START, S_END, S_PITCH, S_REV, S_LOOP, S_STRETCH, S_CUT, S_ATT, S_DEC,
    M_VOL, M_PAN, M_MUTE, M_SOLO, M_PHASE, M_OUT, M_SENDA, M_SENDB,
    F1_TYPE, F1_P1, F1_P2, F1_P3, F1_P4, F1_P5, F1_MIX,
    F2_TYPE, F2_P1, F2_P2, F2_P3, F2_P4, F2_P5, F2_MIX,
    kChanParams
};
enum GlobalParam { G_MVOL, G_CEIL, G_SWING, G_HUMAN, G_SEQ, G_RUN, G_RATE, G_PATTERN, G_MIDICH, G_TEMPO, G_RETA, G_RETB,
    G_RNDMODE, G_RNDINT, G_MACRO1, G_MACRO2, G_MACRO3, G_MACRO4, G_MACRO5, G_MACRO6, G_MACRO7, G_MACRO8,
    G_KEY, G_SCALE, G_KEEPKEY, G_SNAPMODE, kGlobalParams };
constexpr int kGlobalBase = kChannels * kChanParams;
constexpr int kMasterBase = kGlobalBase + kGlobalParams;
constexpr int kSendBase = kMasterBase + kMasterSlots * kFxParams;
constexpr int kNumParams = kSendBase + kSendSlots * kFxParams;
inline int cp(int ch, int p) { return ch * kChanParams + p; }
inline int gp(int p) { return kGlobalBase + p; }
inline int mp(int slot, int k) { return kMasterBase + slot * kFxParams + k; }
inline int sp(int slot, int k) { return kSendBase + slot * kFxParams + k; }

inline const char* channelName(int c) {
    static const char* n[] = {"Kick", "Snare", "Closed Hat", "Open Hat", "Perc 1", "Perc 2", "Perc 3", "Perc 4", "Perc 5", "Perc 6", "Perc 7", "Perc 8"};
    return n[c >= 0 && c < kChannels ? c : 0];
}
inline int defaultEngine(int c) { static const int e[] = {E_KICK, E_SNARE, E_CHAT, E_OHAT, E_CLAP, E_TOM, E_RIM, E_COWBELL, E_WOOD, E_METAL, E_IMPACT, E_EXPERIMENTAL}; return e[c]; }
inline int defaultNote(int c) { static const int n[] = {36, 38, 42, 46, 39, 45, 37, 56, 76, 53, 49, 75}; return n[c]; }

// ---------- Effects ----------
enum FxType { FX_OFF, FX_DIST, FX_COMP, FX_FILTER, FX_TRANS, FX_DELAY, FX_REVERB, FX_CRUSH, FX_WIDE, FX_RING, FX_PITCH, FX_DEGRADE, FX_COUNT };
inline const std::vector<std::string>& fxNames() { static const std::vector<std::string> n{"Off", "Distortion", "Compressor", "Filter", "Transient", "Delay", "Reverb", "Bitcrush", "Widener", "Ring Mod", "Pitch Shift", "Degrade"}; return n; }
struct FxMeta { const char* label[5]; float def[5]; float mix; };
inline const FxMeta& fxMeta(int t) {
    static const FxMeta m[FX_COUNT] = {
        {{"", "", "", "", ""}, {0, 0, 0, 0, 0}, 1},
        {{"Drive", "Mode", "Tone", "Bias", "Output"}, {0.35f, 0, 0.8f, 0, 0.75f}, 1},
        {{"Threshold", "Ratio", "Attack", "Release", "Makeup"}, {0.6f, 0.2f, 0.3f, 0.3f, 0.1f}, 1},
        {{"Cutoff", "Resonance", "Mode", "Drive", "Env Mod"}, {0.7f, 0.2f, 0, 0, 0.5f}, 1},
        {{"Attack", "Sustain", "Speed", "Output", "Clip"}, {0.75f, 0.5f, 0.4f, 0.5f, 0}, 1},
        {{"Time", "Feedback", "Filter", "Spread", "Sync"}, {0.5f, 0.35f, 0.7f, 0.5f, 0.6f}, 0.3f},
        {{"Size", "Decay", "Damping", "Diffusion", "Predelay"}, {0.55f, 0.65f, 0.4f, 0.7f, 0.1f}, 0.25f},
        {{"Bits", "Rate", "Smooth", "", ""}, {0.4f, 0.3f, 0.2f, 0, 0}, 1},
        {{"Width", "Haas", "Mono Bass", "", ""}, {0.75f, 0, 0.3f, 0, 0}, 1},
        {{"Freq", "Shape", "LFO Rate", "LFO Depth", ""}, {0.4f, 0, 0.2f, 0, 0}, 0.5f},
        {{"Semitones", "Window", "Fine", "", ""}, {0.75f, 0.4f, 0.5f, 0, 0}, 0.5f},
        {{"Hiss", "Crackle", "Wow", "Tone", "Dropouts"}, {0.25f, 0.2f, 0.2f, 0.7f, 0}, 1}};
    return m[t >= 0 && t < FX_COUNT ? t : 0];
}
inline const char* fxModeName(int i) { static const char* n[] = {"Saturate", "Soft clip", "Hard clip", "Fold", "Waveshape"}; return n[i < 0 ? 0 : (i > 4 ? 4 : i)]; }
inline const char* filterModeName(int i) { static const char* n[] = {"Low-pass", "High-pass", "Band-pass", "Notch"}; return n[i < 0 ? 0 : (i > 3 ? 3 : i)]; }
inline const char* syncName(int i) { static const char* n[] = {"Free", "1/32", "1/16T", "1/16", "1/8T", "1/16D", "1/8", "1/4T", "1/8D", "1/4", "1/2"}; return n[i < 0 ? 0 : (i > 10 ? 10 : i)]; }
inline double syncBeats(int i) { static const double b[] = {0, 0.125, 1.0 / 6, 0.25, 1.0 / 3, 0.375, 0.5, 2.0 / 3, 0.75, 1.0, 2.0}; return b[i < 0 ? 0 : (i > 10 ? 10 : i)]; }
inline int stepsOf(float v, int n) { return (int)clampf(std::floor(v * n), 0.f, (float)(n - 1)); }
inline float expMap(float v, float lo, float hi) { return lo * std::pow(hi / lo, clampf(v, 0, 1)); }

/// Map an FX slot's normalised 0..1 knob to the real value the DSP uses.
inline float fxValue(int t, int i, float v) {
    switch (t) {
        case FX_DIST: { const float r[] = {v * 36.f, (float)stepsOf(v, 5), expMap(v, 800, 20000), v, -24.f + 30.f * v}; return r[i]; }
        case FX_COMP: { const float r[] = {-60.f + 60.f * v, 1.f + 19.f * v * v, expMap(v, 0.1f, 100.f), expMap(v, 10.f, 1200.f), 24.f * v}; return r[i]; }
        case FX_FILTER: { const float r[] = {expMap(v, 20.f, 20000.f), v, (float)stepsOf(v, 4), v, v * 2.f - 1.f}; return r[i]; }
        case FX_TRANS: { const float r[] = {v * 2.f - 1.f, v * 2.f - 1.f, v, -12.f + 24.f * v, v}; return r[i]; }
        case FX_DELAY: { const float r[] = {expMap(v, 1.f, 1000.f), 0.95f * v, expMap(v, 400.f, 20000.f), v, (float)stepsOf(v, 11)}; return r[i]; }
        case FX_REVERB: { const float r[] = {v, v, v, v, 100.f * v}; return r[i]; }
        case FX_CRUSH: { const float r[] = {16.f - 15.f * v, expMap(1.f - v, 400.f, 48000.f), v, 0, 0}; return r[i]; }
        case FX_WIDE: { const float r[] = {2.f * v, 20.f * v, v < 0.02f ? 0.f : expMap(v, 40.f, 400.f), 0, 0}; return r[i]; }
        case FX_RING: { const float r[] = {expMap(v, 20.f, 5000.f), v, expMap(v, 0.05f, 20.f), v, 0}; return r[i]; }
        case FX_PITCH: { const float r[] = {std::round(-24.f + 48.f * v), expMap(v, 10.f, 120.f), -100.f + 200.f * v, 0, 0}; return r[i]; }
        case FX_DEGRADE: { const float r[] = {v, v, v, expMap(v, 1500.f, 20000.f), v}; return r[i]; }
        default: return v;
    }
}
inline std::string fxText(int t, int i, float v) {
    char b[48]; float x = fxValue(t, i, v);
    auto hz = [&](float f) { if (f >= 1000) std::snprintf(b, sizeof b, "%.1f kHz", f / 1000); else std::snprintf(b, sizeof b, "%.0f Hz", f); };
    switch (t) {
        case FX_DIST: if (i == 1) return fxModeName((int)x); if (i == 2) hz(x); else if (i == 3) std::snprintf(b, sizeof b, "%.0f %%", x * 100); else std::snprintf(b, sizeof b, "%.1f dB", x); break;
        case FX_COMP: if (i == 1) std::snprintf(b, sizeof b, "%.1f:1", x); else if (i == 2 || i == 3) std::snprintf(b, sizeof b, "%.1f ms", x); else std::snprintf(b, sizeof b, "%.1f dB", x); break;
        case FX_FILTER: if (i == 0) hz(x); else if (i == 2) return filterModeName((int)x); else std::snprintf(b, sizeof b, "%.0f %%", x * 100); break;
        case FX_TRANS: if (i == 3) std::snprintf(b, sizeof b, "%.1f dB", x); else std::snprintf(b, sizeof b, "%+.0f %%", x * 100); break;
        case FX_DELAY: if (i == 0) std::snprintf(b, sizeof b, "%.0f ms", x); else if (i == 2) hz(x); else if (i == 4) return syncName((int)x); else std::snprintf(b, sizeof b, "%.0f %%", x * 100); break;
        case FX_REVERB: if (i == 4) std::snprintf(b, sizeof b, "%.0f ms", x); else std::snprintf(b, sizeof b, "%.0f %%", x * 100); break;
        case FX_CRUSH: if (i == 0) std::snprintf(b, sizeof b, "%.1f bit", x); else if (i == 1) hz(x); else std::snprintf(b, sizeof b, "%.0f %%", x * 100); break;
        case FX_WIDE: if (i == 0) std::snprintf(b, sizeof b, "%.0f %%", x * 100); else if (i == 1) std::snprintf(b, sizeof b, "%.1f ms", x); else if (x <= 0) return "Off"; else hz(x); break;
        case FX_RING: if (i == 0) hz(x); else if (i == 2) std::snprintf(b, sizeof b, "%.2f Hz", x); else std::snprintf(b, sizeof b, "%.0f %%", x * 100); break;
        case FX_PITCH: if (i == 0) std::snprintf(b, sizeof b, "%+.0f st", x); else if (i == 1) std::snprintf(b, sizeof b, "%.0f ms", x); else std::snprintf(b, sizeof b, "%+.0f ct", x); break;
        case FX_DEGRADE: if (i == 3) hz(x); else std::snprintf(b, sizeof b, "%.0f %%", x * 100); break;
        default: std::snprintf(b, sizeof b, "%.2f", v);
    }
    return b;
}

// ---------- Parameter registry ----------
struct ParamDef {
    std::string id, name; float lo, hi, def, centre; // centre > 0: log-like skew so that centre sits mid-knob
    bool integer = false, toggle = false; std::vector<std::string> choices; std::string unit;
    int channel = -1, key = -1; // channel/key for per-channel params, key = ChanParam
};
struct VoiceKey { const char* id; const char* name; float lo, hi, def, centre; const char* unit; int kind; }; // kind 0 float, 1 int, 2 toggle, 3 choice
inline const VoiceKey& chanKey(int k) {
    static const VoiceKey keys[kChanParams] = {
        {"engine", "Engine", 0, E_COUNT - 1, 0, 0, "", 3}, {"freq", "Pitch", 20, 12000, 100, 300, "Hz", 0}, {"tune", "Tune", -24, 24, 0, 0, "st", 0},
        {"sweep", "Pitch Sweep", 0, 72, 0, 0, "st", 0}, {"sweept", "Sweep Decay", 1, 1000, 50, 60, "ms", 0}, {"att", "Attack", 0, 200, 0, 15, "ms", 0},
        {"dec", "Decay", 5, 6000, 300, 400, "ms", 0}, {"sus", "Sustain", 0, 1, 0, 0, "", 0}, {"rel", "Release", 5, 6000, 100, 300, "ms", 0},
        {"punch", "Punch", 0, 1, 0, 0, "", 0}, {"click", "Click", 0, 1, 0, 0, "", 0}, {"body", "Body", 0, 1, 1, 0, "", 0}, {"sub", "Sub", 0, 1, 0, 0, "", 0},
        {"noise", "Noise", 0, 1, 0, 0, "", 0}, {"ncolor", "Noise Colour", 0, 1, 0, 0, "", 0}, {"ndec", "Noise Decay", 2, 4000, 100, 200, "ms", 0},
        {"harm", "Harmonics", 0, 1, 0.5f, 0, "", 0}, {"snap", "Snap", 0, 1, 0, 0, "", 0}, {"fmamt", "FM Amount", 0, 1, 0, 0, "", 0},
        {"fmratio", "FM Ratio", 0.25f, 16, 2, 2, "x", 0}, {"ring", "Ring/AM", 0, 1, 0, 0, "", 0}, {"drive", "Drive", 0, 1, 0, 0, "", 0},
        {"sat", "Saturation", 0, 1, 0, 0, "", 0}, {"dist", "Distortion", 0, 1, 0, 0, "", 0}, {"dmode", "Dist Mode", 0, 4, 0, 0, "", 3},
        {"ftype", "Filter Type", 0, 3, 0, 0, "", 3}, {"cut", "Cutoff", 20, 20000, 20000, 1500, "Hz", 0}, {"res", "Resonance", 0, 1, 0, 0, "", 0},
        {"tone", "Tone", -1, 1, 0, 0, "", 0}, {"wave", "Wave", 0, 1, 0, 0, "", 0}, {"mod", "Modulation", 0, 1, 0, 0, "", 0}, {"width", "Width", 0, 1, 0, 0, "", 0},
        {"vsens", "Velocity Sens", 0, 1, 0.7f, 0, "", 0}, {"level", "Voice Level", -48, 6, -6, 0, "dB", 0}, {"choke", "Choke Group", 0, 4, 0, 0, "", 3},
        {"note", "MIDI Note", 0, 127, 36, 0, "", 1},
        {"slevel", "Sample Level", 0, 1, 0.8f, 0, "", 0}, {"synth", "Synth Level", 0, 1, 1, 0, "", 0}, {"sstart", "Sample Start", 0, 1, 0, 0, "", 0},
        {"send", "Sample End", 0, 1, 1, 0, "", 0}, {"spitch", "Sample Pitch", -24, 24, 0, 0, "st", 0}, {"srev", "Sample Reverse", 0, 1, 0, 0, "", 2},
        {"sloop", "Sample Loop", 0, 1, 0, 0, "", 2}, {"sstretch", "Sample Stretch", 0.5f, 2, 1, 1, "x", 0}, {"scut", "Sample Cutoff", 20, 20000, 20000, 1500, "Hz", 0},
        {"satt", "Sample Attack", 0, 500, 0, 30, "ms", 0}, {"sdec", "Sample Decay", 10, 10000, 10000, 600, "ms", 0},
        {"vol", "Volume", -60, 6, 0, -12, "dB", 0}, {"pan", "Pan", -1, 1, 0, 0, "", 0}, {"mute", "Mute", 0, 1, 0, 0, "", 2}, {"solo", "Solo", 0, 1, 0, 0, "", 2},
        {"phase", "Phase Invert", 0, 1, 0, 0, "", 2}, {"out", "Output", 0, kChannels, 0, 0, "", 3}, {"senda", "Send A", 0, 1, 0, 0, "", 0}, {"sendb", "Send B", 0, 1, 0, 0, "", 0},
        {"fx1type", "FX1 Type", 0, FX_COUNT - 1, 0, 0, "", 3}, {"fx1p1", "FX1 P1", 0, 1, 0.5f, 0, "", 0}, {"fx1p2", "FX1 P2", 0, 1, 0.5f, 0, "", 0}, {"fx1p3", "FX1 P3", 0, 1, 0.5f, 0, "", 0},
        {"fx1p4", "FX1 P4", 0, 1, 0.5f, 0, "", 0}, {"fx1p5", "FX1 P5", 0, 1, 0.5f, 0, "", 0}, {"fx1mix", "FX1 Mix", 0, 1, 1, 0, "", 0},
        {"fx2type", "FX2 Type", 0, FX_COUNT - 1, 0, 0, "", 3}, {"fx2p1", "FX2 P1", 0, 1, 0.5f, 0, "", 0}, {"fx2p2", "FX2 P2", 0, 1, 0.5f, 0, "", 0}, {"fx2p3", "FX2 P3", 0, 1, 0.5f, 0, "", 0},
        {"fx2p4", "FX2 P4", 0, 1, 0.5f, 0, "", 0}, {"fx2p5", "FX2 P5", 0, 1, 0.5f, 0, "", 0}, {"fx2mix", "FX2 Mix", 0, 1, 1, 0, "", 0}};
    return keys[k];
}
inline int chanKeyIndex(const std::string& id) { for (int k = 0; k < kChanParams; ++k) if (id == chanKey(k).id) return k; return -1; }

/// Engine default sound: overrides applied on top of the base defaults when an engine is selected.
struct KV { int k; float v; };
inline std::vector<KV> engineDefaults(int e) {
    switch (e) {
        case E_KICK: return {{V_FREQ, 50}, {V_SWEEP, 30}, {V_SWEEPT, 35}, {V_DEC, 650}, {V_CLICK, 0.25f}, {V_PUNCH, 0.4f}, {V_SUB, 0.3f}, {V_SAT, 0.2f}, {V_FMRATIO, 1.5f}, {V_NDEC, 15}, {V_TONE, 0}};
        case E_SNARE: return {{V_FREQ, 185}, {V_SWEEP, 7}, {V_SWEEPT, 20}, {V_DEC, 180}, {V_NOISE, 0.75f}, {V_NDEC, 220}, {V_NCOLOR, 0.15f}, {V_FTYPE, 1}, {V_CUT, 1200}, {V_RES, 0.15f}, {V_SNAP, 0.5f}, {V_BODY, 0.6f}, {V_HARM, 0.4f}, {V_WIDTH, 0.3f}};
        case E_CHAT: return {{V_FREQ, 330}, {V_DEC, 55}, {V_NOISE, 0.35f}, {V_NDEC, 45}, {V_CUT, 7000}, {V_RES, 0.2f}, {V_TONE, 0.3f}, {V_HARM, 0.2f}, {V_CHOKE, 1}, {V_WIDTH, 0.25f}};
        case E_OHAT: return {{V_FREQ, 330}, {V_DEC, 520}, {V_SUS, 0}, {V_REL, 300}, {V_NOISE, 0.35f}, {V_NDEC, 420}, {V_CUT, 6500}, {V_RES, 0.2f}, {V_TONE, 0.3f}, {V_HARM, 0.2f}, {V_CHOKE, 1}, {V_WIDTH, 0.35f}};
        case E_CLAP: return {{V_FREQ, 1150}, {V_RES, 0.35f}, {V_HARM, 0.4f}, {V_SWEEPT, 11}, {V_DEC, 260}, {V_NOISE, 1}, {V_CLICK, 0.8f}, {V_BODY, 0.6f}, {V_MOD, 0.3f}, {V_WIDTH, 0.4f}};
        case E_TOM: return {{V_FREQ, 110}, {V_SWEEP, 9}, {V_SWEEPT, 60}, {V_DEC, 420}, {V_HARM, 0.35f}, {V_NOISE, 0.15f}, {V_NDEC, 30}, {V_CLICK, 0.2f}, {V_RES, 0.3f}};
        case E_RIM: return {{V_FREQ, 455}, {V_HARM, 0.555f}, {V_DEC, 45}, {V_CLICK, 0.6f}, {V_NOISE, 0.15f}, {V_NDEC, 10}, {V_DRIVE, 0.35f}, {V_TONE, 0.2f}};
        case E_COWBELL: return {{V_FREQ, 540}, {V_HARM, 0.4624f}, {V_DEC, 330}, {V_PUNCH, 0.6f}, {V_CUT, 2600}, {V_RES, 0.3f}};
        case E_WOOD: return {{V_FREQ, 1250}, {V_HARM, 0.3f}, {V_DEC, 70}, {V_CLICK, 0.5f}, {V_RES, 0.5f}};
        case E_METAL: return {{V_FREQ, 420}, {V_FMAMT, 0.4f}, {V_FMRATIO, 3.41f}, {V_RING, 0.3f}, {V_HARM, 0.5f}, {V_MOD, 0.5f}, {V_DEC, 1400}, {V_WIDTH, 0.4f}};
        case E_CLICK: return {{V_FREQ, 3200}, {V_DEC, 12}, {V_NOISE, 0.5f}, {V_NDEC, 6}, {V_SWEEP, 12}, {V_SWEEPT, 4}, {V_RES, 0.4f}};
        case E_IMPACT: return {{V_FREQ, 38}, {V_SWEEP, 36}, {V_SWEEPT, 90}, {V_DEC, 2800}, {V_SUB, 0.6f}, {V_NOISE, 0.5f}, {V_NDEC, 1600}, {V_NCOLOR, 0.6f}, {V_CUT, 3500}, {V_MOD, 0.6f}, {V_SAT, 0.3f}, {V_WIDTH, 0.6f}};
        case E_INDUSTRIAL: return {{V_FREQ, 90}, {V_SWEEP, 18}, {V_SWEEPT, 40}, {V_DEC, 380}, {V_NOISE, 0.6f}, {V_NDEC, 250}, {V_HARM, 0.4f}, {V_RES, 0.7f}, {V_DRIVE, 0.5f}, {V_DIST, 0.7f}, {V_DMODE, 2}, {V_MOD, 0.3f}};
        case E_EXPERIMENTAL: return {{V_FREQ, 220}, {V_SWEEP, 12}, {V_SWEEPT, 45}, {V_DEC, 600}, {V_FMAMT, 0.5f}, {V_RING, 0.3f}, {V_NOISE, 0.2f}, {V_NDEC, 300}, {V_MOD, 0.4f}, {V_WIDTH, 0.5f}};
        default: return {};
    }
}

inline const std::vector<ParamDef>& registry() {
    static const std::vector<ParamDef> defs = [] {
        std::vector<ParamDef> d; d.reserve(kNumParams);
        std::vector<std::string> engines; for (int e = 0; e < E_COUNT; ++e) engines.push_back(engineName(e));
        std::vector<std::string> outs{"Main"}; for (int o = 2; o <= kChannels + 1; ++o) outs.push_back("Out " + std::to_string(o));
        for (int c = 0; c < kChannels; ++c) {
            auto ed = engineDefaults(defaultEngine(c));
            for (int k = 0; k < kChanParams; ++k) {
                const auto& vk = chanKey(k); ParamDef p;
                p.id = "c" + std::to_string(c + 1) + "_" + vk.id; p.name = std::to_string(c + 1) + " " + channelName(c) + " " + vk.name;
                p.lo = vk.lo; p.hi = vk.hi; p.def = vk.def; p.centre = vk.centre; p.unit = vk.unit; p.integer = vk.kind == 1; p.toggle = vk.kind == 2; p.channel = c; p.key = k;
                for (auto kv : ed) if (kv.k == k) p.def = kv.v;
                if (k == V_ENGINE) { p.choices = engines; p.def = (float)defaultEngine(c); }
                if (k == V_DMODE) p.choices = {"Soft", "Hard", "Fold", "Asym", "Cubic"};
                if (k == V_FTYPE) p.choices = {"Low-pass", "High-pass", "Band-pass", "Notch"};
                if (k == V_CHOKE) p.choices = {"None", "A", "B", "C", "D"};
                if (k == V_NOTE) p.def = (float)defaultNote(c);
                if (k == M_OUT) p.choices = outs;
                if (k == F1_TYPE || k == F2_TYPE) p.choices = fxNames();
                if (k == M_VOL) p.def = c == 0 ? 0.f : (c < 4 ? -2.f : -4.f);
                d.push_back(p);
            }
        }
        auto add = [&](std::string id, std::string name, float lo, float hi, float def, float centre = 0, std::string unit = "", int kind = 0, std::vector<std::string> ch = {}) {
            ParamDef p; p.id = id; p.name = name; p.lo = lo; p.hi = hi; p.def = def; p.centre = centre; p.unit = unit; p.integer = kind == 1; p.toggle = kind == 2; p.choices = ch; d.push_back(p);
        };
        add("g_mvol", "Master Volume", -60, 6, 0, -12, "dB");
        add("g_ceil", "Limiter Ceiling", -12, 0, -0.3f, 0, "dB");
        add("g_swing", "Swing", 50, 75, 50, 0, "%");
        add("g_human", "Humanize", 0, 1, 0);
        add("g_seq", "Sequencer", 0, 2, 1, 0, "", 3, {"Off", "Follow host", "Host + internal"});
        add("g_run", "Internal Play", 0, 1, 0, 0, "", 2);
        add("g_rate", "Step Rate", 0, 3, 1, 0, "", 3, {"1/8", "1/16", "1/16T", "1/32"});
        add("g_pattern", "Pattern", 1, 16, 1, 0, "", 1);
        add("g_midich", "MIDI Channel", 0, 16, 0, 0, "", 3, {"Omni", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16"});
        add("g_tempo", "Internal Tempo", 40, 240, 120, 0, "BPM");
        add("g_reta", "Return A", 0, 1, 0.8f);
        add("g_retb", "Return B", 0, 1, 0.8f);
        add("g_rndmode", "Random Mode", 0, 6, 0, 0, "", 3, {"Classic", "Analog", "Experimental", "Industrial", "Minimal", "Complex", "Ambient"});
        add("g_rndint", "Random Intensity", 0, 1, 0.5f);
        for (int m = 1; m <= 8; ++m) add("g_macro" + std::to_string(m), "Macro " + std::to_string(m), 0, 1, 0.5f);
        add("g_key", "Key", 0, 11, 0, 0, "", 3, {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"});
        add("g_scale", "Scale", 0, 4, 0, 0, "", 3, {"Major", "Minor", "Chromatic", "Maj Pent", "Min Pent"});
        add("g_keepkey", "Keep in Key", 0, 1, 0, 0, "", 2);
        add("g_snapmode", "Snap To", 0, 1, 0, 0, "", 3, {"Nearest", "Root"});
        for (int s = 0; s < kMasterSlots; ++s) {
            std::string pre = "m" + std::to_string(s + 1), nm = "Master FX" + std::to_string(s + 1);
            int defType = s == 0 ? FX_COMP : 0;
            add(pre + "type", nm + " Type", 0, FX_COUNT - 1, (float)defType, 0, "", 3, fxNames());
            for (int i = 0; i < 5; ++i) add(pre + "p" + std::to_string(i + 1), nm + " P" + std::to_string(i + 1), 0, 1, defType ? fxMeta(defType).def[i] : 0.5f);
            add(pre + "mix", nm + " Mix", 0, 1, 1);
        }
        for (int s = 0; s < kSendSlots; ++s) {
            std::string pre = s == 0 ? "sa" : "sb", nm = s == 0 ? "Send A FX" : "Send B FX"; int t = s == 0 ? FX_REVERB : FX_DELAY;
            add(pre + "type", nm + " Type", 0, FX_COUNT - 1, (float)t, 0, "", 3, fxNames());
            for (int i = 0; i < 5; ++i) add(pre + "p" + std::to_string(i + 1), nm + " P" + std::to_string(i + 1), 0, 1, fxMeta(t).def[i]);
            add(pre + "mix", nm + " Mix", 0, 1, 1);
        }
        // Gentle master glue compressor defaults
        auto& m1 = d[(size_t)mp(0, 1)]; m1.def = 0.75f; d[(size_t)mp(0, 2)].def = 0.12f; d[(size_t)mp(0, 3)].def = 0.35f; d[(size_t)mp(0, 4)].def = 0.35f; d[(size_t)mp(0, 5)].def = 0.04f;
        return d; }();
    return defs;
}
inline int paramIndex(const std::string& id) {
    static const auto map = [] { std::vector<std::pair<std::string, int>> m; int i = 0; for (auto& p : registry()) m.push_back({p.id, i++}); std::sort(m.begin(), m.end()); return m; }();
    auto it = std::lower_bound(map.begin(), map.end(), std::make_pair(id, -1));
    return (it != map.end() && it->first == id) ? it->second : -1;
}
inline void loadDefaults(float* v) { const auto& r = registry(); for (int i = 0; i < kNumParams; ++i) v[i] = r[(size_t)i].def; }
inline float clampToDef(int i, float x) { const auto& p = registry()[(size_t)i]; x = clampf(x, p.lo, p.hi); if (p.integer || p.toggle || !p.choices.empty()) x = std::round(x); return x; }

/// Reset a channel's sound (voice + sample params) to an engine's defaults, keeping note, mixer and FX.
inline void applyEngineDefaults(float* v, int ch, int engine) {
    for (int k = V_FREQ; k <= S_DEC; ++k) { if (k == V_NOTE) continue; v[cp(ch, k)] = chanKey(k).def; }
    for (auto kv : engineDefaults(engine)) v[cp(ch, kv.k)] = kv.v;
    v[cp(ch, V_ENGINE)] = (float)engine;
}
} // namespace bk
