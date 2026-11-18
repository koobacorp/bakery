// BAKERY — random sound / kit / pattern generator. Seven modes, intensity, per-sound and per-parameter locks.
// Level safety: every generated sound is rendered offline and its Voice Level set so a full hit peaks at a safe target.
#pragma once
#include <set>
#include "Factory.h"

namespace bk {
enum RandomMode { R_CLASSIC, R_ANALOG, R_EXPERIMENTAL, R_INDUSTRIAL, R_MINIMAL, R_COMPLEX, R_AMBIENT, R_COUNT };
inline const char* randomModeName(int m) { static const char* n[] = {"Classic", "Analog", "Experimental", "Industrial", "Minimal", "Complex", "Ambient"}; return n[m >= 0 && m < R_COUNT ? m : 0]; }

struct Locks {
    bool sound[kChannels]{};   // keep this channel's sound
    bool track[kChannels]{};   // keep this sequencer track's steps
    std::set<int> params;      // global parameter indices to keep
    bool isLocked(int i) const { return params.count(i) > 0; }
};

struct FreqRange { float lo, hi; };
inline FreqRange plausibleFreq(int e) {
    switch (e) {
        case E_KICK: return {32, 80}; case E_SNARE: return {130, 340}; case E_CHAT: case E_OHAT: return {220, 650}; case E_CLAP: return {700, 2400};
        case E_TOM: return {60, 380}; case E_RIM: return {300, 900}; case E_COWBELL: return {380, 900}; case E_WOOD: return {500, 3200};
        case E_METAL: return {100, 1200}; case E_CLICK: return {500, 8000}; case E_IMPACT: return {25, 70}; case E_INDUSTRIAL: return {40, 400}; default: return {60, 1200};
    }
}
inline float maxDecayMs(int e, int mode) {
    float m = 1.f; if (mode == R_AMBIENT) m = 2.f; if (mode == R_MINIMAL) m = 0.7f;
    switch (e) {
        case E_KICK: return 2600 * m; case E_SNARE: return 450 * m; case E_CHAT: return 160 * m; case E_OHAT: return 1200 * m; case E_CLAP: return 800 * m;
        case E_TOM: return 1200 * m; case E_RIM: return 120 * m; case E_COWBELL: return 700 * m; case E_WOOD: return 200 * m; case E_METAL: return 4000 * m;
        case E_CLICK: return 120 * m; case E_IMPACT: return 5000 * m; default: return 1500 * m;
    }
}
// normalised position of a value on the parameter's range, honoring the skew used by the host knob
inline float toNorm(int i, float x) { const auto& p = registry()[(size_t)i]; if (p.centre > 0 && p.lo > 0) return std::log(x / p.lo) / std::log(p.hi / p.lo); return (x - p.lo) / (p.hi - p.lo); }
inline float fromNorm(int i, float n) { const auto& p = registry()[(size_t)i]; n = clampf(n, 0, 1); if (p.centre > 0 && p.lo > 0) return p.lo * std::pow(p.hi / p.lo, n); return p.lo + n * (p.hi - p.lo); }

/// Generate one random sound for channel `ch` (engine kept unless `engine` >= 0). Returns false if locked.
inline bool randomSound(float* v, int ch, int mode, float intensity, Rng& rng, const Locks& locks, int engine = -1, float kitDrive = -1.f) {
    if (locks.sound[ch]) return false;
    int e = engine >= 0 ? engine : (int)v[cp(ch, V_ENGINE)];
    float keep[kChanParams]; for (int k = 0; k < kChanParams; ++k) keep[k] = v[cp(ch, k)];
    // start from a factory sound of this engine (style seed) or the engine default
    std::vector<const SoundPreset*> pool; for (auto& s : factorySounds()) if (s.engine == e) pool.push_back(&s);
    if (!pool.empty() && rng.uni() < 0.85f) applySound(v, ch, *pool[(size_t)rng.below((int)pool.size())], false); else applyEngineDefaults(v, ch, e);
    float spread = 0.08f + 0.42f * clampf(intensity, 0, 1); if (mode == R_CLASSIC || mode == R_MINIMAL) spread *= 0.6f; if (mode == R_EXPERIMENTAL || mode == R_COMPLEX) spread *= 1.3f;
    static const int tweak[] = {V_FREQ, V_TUNE, V_SWEEP, V_SWEEPT, V_DEC, V_PUNCH, V_CLICK, V_BODY, V_SUB, V_NOISE, V_NCOLOR, V_NDEC, V_HARM, V_SNAP, V_FMAMT, V_FMRATIO, V_RING, V_DRIVE, V_SAT, V_DIST, V_CUT, V_RES, V_TONE, V_WAVE, V_MOD, V_WIDTH};
    for (int k : tweak) { int i = cp(ch, k); if (k == V_TUNE) { v[i] = 0; continue; } v[i] = fromNorm(i, toNorm(i, v[i]) + spread * (rng.bi() + rng.bi()) * 0.5f); }
    auto set = [&](int k, float x) { v[cp(ch, k)] = clampToDef(cp(ch, k), x); };
    auto get = [&](int k) { return v[cp(ch, k)]; };
    switch (mode) {
        case R_CLASSIC: set(V_RING, get(V_RING) * 0.3f); set(V_FMAMT, get(V_FMAMT) * 0.4f); set(V_DIST, get(V_DIST) * 0.3f); break;
        case R_ANALOG: set(V_SAT, std::max(get(V_SAT), rng.range(0.15f, 0.5f))); set(V_RING, 0); set(V_FMAMT, get(V_FMAMT) * 0.2f); set(V_DIST, get(V_DIST) * 0.2f); set(V_NCOLOR, std::max(get(V_NCOLOR), 0.15f)); set(V_TONE, get(V_TONE) - 0.2f); break;
        case R_EXPERIMENTAL: set(V_FMAMT, rng.range(0.2f, 1.f) * intensity + get(V_FMAMT) * (1 - intensity)); set(V_FMRATIO, rng.range(0.5f, 9.f)); set(V_RING, rng.range(0.f, 0.8f) * intensity); set(V_MOD, rng.uni()); set(V_DMODE, (float)rng.below(5)); set(V_SWEEP, get(V_SWEEP) + rng.range(0, 24) * intensity); break;
        case R_INDUSTRIAL: set(V_DRIVE, rng.range(0.45f, 1.f)); set(V_DIST, rng.range(0.4f, 0.9f)); set(V_DMODE, (float)(rng.chance(0.5f) ? 2 : 1)); set(V_SAT, rng.range(0.3f, 0.7f)); set(V_NOISE, std::max(get(V_NOISE), rng.range(0.2f, 0.7f))); set(V_TONE, rng.range(0.f, 0.5f)); break;
        case R_MINIMAL: set(V_DEC, get(V_DEC) * 0.6f); set(V_NDEC, get(V_NDEC) * 0.6f); set(V_DRIVE, get(V_DRIVE) * 0.3f); set(V_DIST, 0); set(V_RING, 0); set(V_NOISE, get(V_NOISE) * 0.7f); break;
        case R_COMPLEX: set(V_FMAMT, get(V_FMAMT) + rng.range(0, 0.5f) * intensity); set(V_RING, get(V_RING) + rng.range(0, 0.4f) * intensity); set(V_MOD, rng.uni() * intensity); set(V_DMODE, (float)rng.below(5)); break;
        case R_AMBIENT: set(V_DEC, get(V_DEC) * rng.range(1.3f, 2.2f)); set(V_NDEC, get(V_NDEC) * 1.8f); set(V_ATT, rng.range(0, 30.f) * intensity); set(V_DRIVE, 0); set(V_DIST, 0); set(V_SAT, get(V_SAT) * 0.3f); set(V_TONE, -rng.range(0.1f, 0.6f)); set(V_NCOLOR, rng.range(0.4f, 0.9f)); set(V_WIDTH, rng.range(0.4f, 1.f)); set(V_CLICK, get(V_CLICK) * 0.3f); break;
        default: break;
    }
    if (kitDrive >= 0 && mode != R_AMBIENT) set(V_SAT, lerpf(get(V_SAT), kitDrive, 0.5f));
    auto fr = plausibleFreq(e); set(V_FREQ, clampf(get(V_FREQ), fr.lo, fr.hi));
    set(V_DEC, std::min(get(V_DEC), maxDecayMs(e, mode))); set(V_NDEC, std::min(get(V_NDEC), maxDecayMs(e, mode) * 1.2f));
    if (e == E_KICK) { set(V_BODY, std::max(get(V_BODY), 0.5f)); set(V_SWEEP, clampf(get(V_SWEEP), 6.f, 48.f)); }
    if (e == E_SNARE || e == E_CLAP || e == E_CHAT || e == E_OHAT) set(V_NOISE, std::max(get(V_NOISE), e == E_CLAP ? 0.7f : 0.15f));
    if (e == E_CHAT || e == E_OHAT) { set(V_CUT, clampf(get(V_CUT), 3000.f, 12000.f)); set(V_CHOKE, 1); set(V_BODY, std::max(get(V_BODY), 0.3f)); }
    set(V_SUS, 0); set(V_ATT, std::min(get(V_ATT), mode == R_AMBIENT ? 40.f : 5.f)); set(V_RES, std::min(get(V_RES), 0.85f));
    // restore locked parameters, the MIDI note, choke for non-hats, and the sample layer
    for (int k = 0; k < kChanParams; ++k) {
        bool sampleOrMix = k >= S_LEVEL; bool lock = locks.isLocked(cp(ch, k));
        if (lock || sampleOrMix || k == V_NOTE || k == V_VSENS || (k == V_CHOKE && e != E_CHAT && e != E_OHAT)) v[cp(ch, k)] = keep[k];
    }
    v[cp(ch, V_ENGINE)] = (float)e;
    float pk = normaliseChannel(v, ch);
    if (pk < 1e-4f || !std::isfinite(pk)) { // never leave a silent / broken sound: fall back to the engine default
        for (int k = 0; k < kChanParams; ++k) if (k < S_LEVEL && k != V_NOTE) v[cp(ch, k)] = registry()[(size_t)cp(ch, k)].def;
        applyEngineDefaults(v, ch, e); normaliseChannel(v, ch);
    }
    return true;
}

/// Coherent kit: one root pitch and one drive character shared by every voice; tonal voices tuned to the root.
inline void randomKit(float* v, int mode, float intensity, Rng& rng, const Locks& locks) {
    float root = rng.range(40.f, 62.f), drive = mode == R_INDUSTRIAL ? rng.range(0.4f, 0.8f) : (mode == R_ANALOG ? rng.range(0.15f, 0.4f) : rng.range(0.f, 0.25f));
    static const int scale[] = {0, 3, 5, 7, 10, 12, 15, 19};
    for (int c = 0; c < kChannels; ++c) {
        if (locks.sound[c]) continue;
        int e = (int)v[cp(c, V_ENGINE)];
        if (c >= 4) {
            static const int perc[] = {E_CLAP, E_TOM, E_RIM, E_COWBELL, E_WOOD, E_METAL, E_CLICK, E_IMPACT, E_INDUSTRIAL, E_EXPERIMENTAL};
            if (mode == R_EXPERIMENTAL || mode == R_COMPLEX || rng.chance(0.25f * intensity)) e = perc[rng.below(10)];
            if (mode == R_INDUSTRIAL && rng.chance(0.4f)) e = rng.chance(0.5f) ? E_INDUSTRIAL : E_METAL;
            if (mode == R_AMBIENT && rng.chance(0.4f)) e = rng.chance(0.5f) ? E_METAL : E_IMPACT;
        } else e = c == 0 ? E_KICK : (c == 1 ? E_SNARE : (c == 2 ? E_CHAT : E_OHAT));
        randomSound(v, c, mode, intensity, rng, locks, e, drive);
        if (locks.isLocked(cp(c, V_FREQ))) continue;
        float f = v[cp(c, V_FREQ)]; int deg = scale[rng.below(8)];
        switch (e) {
            case E_KICK: f = root; break;
            case E_TOM: f = root * std::exp2((24 + deg) / 12.f); break;
            case E_SNARE: f = root * std::exp2((36 + scale[rng.below(5)]) / 12.f); break;
            case E_COWBELL: case E_METAL: case E_WOOD: case E_RIM: { auto fr = plausibleFreq(e); float x = root * std::exp2(deg / 12.f); while (x < fr.lo) x *= 2; while (x > fr.hi) x *= 0.5f; f = x; break; }
            default: break;
        }
        auto fr = plausibleFreq(e); v[cp(c, V_FREQ)] = clampf(f, fr.lo, fr.hi);
        normaliseChannel(v, c);
    }
}

inline void euclid(std::vector<bool>& out, int hits, int steps, int rot) {
    out.assign((size_t)steps, false); if (hits <= 0) return;
    for (int i = 0; i < steps; ++i) out[(size_t)((i + rot) % steps)] = ((i * hits) % steps) < hits;
}
/// Generate a full pattern in `slot`. Locked tracks keep their steps.
inline void randomPattern(PatternBank& bank, int slot, int mode, float intensity, Rng& rng, const Locks& locks) {
    int len = 16;
    if (mode == R_AMBIENT) len = 32; if (mode == R_COMPLEX) { const int l[] = {16, 24, 28, 32, 20}; len = l[rng.below(5)]; }
    if (mode == R_EXPERIMENTAL) { const int l[] = {16, 32, 48, 64, 24}; len = l[rng.below(5)]; }
    if (mode == R_CLASSIC && rng.chance(0.3f)) len = 32;
    bank.setLength(slot, len); bank.names[slot] = std::string("Random ") + randomModeName(mode);
    float dens = 0.3f + 0.7f * intensity;
    for (int t = 0; t < kChannels; ++t) {
        if (locks.track[t]) continue;
        bank.clearTrack(slot, t);
        auto hit = [&](int s, int vel, int prob = 100, int acc = 0, int rat = 1) { if (s < 0 || s >= len) return; Step st; st.on = 1; st.vel = (uint8_t)vel; st.prob = (uint8_t)prob; st.accent = (uint8_t)acc; st.ratchet = (uint8_t)rat; bank.set(slot, t, s, st); };
        std::vector<bool> e;
        switch (mode) {
            case R_CLASSIC: case R_ANALOG: case R_INDUSTRIAL: case R_MINIMAL: {
                bool minimal = mode == R_MINIMAL, ind = mode == R_INDUSTRIAL;
                for (int s = 0; s < len; ++s) {
                    int b = s % 16;
                    if (t == 0) { bool four = b % 4 == 0; bool synco = !minimal && rng.chance(0.12f * dens) && b % 2 == 0; if (ind && b % 4 == 3 && rng.chance(0.5f)) hit(s, 90); if (four || synco) hit(s, four ? (ind ? 120 : 110) : 85, 100, ind && four); }
                    if (t == 1 && (b == 4 || b == 12)) hit(s, 110);
                    if (t == 1 && !minimal && rng.chance(0.08f * dens) && b % 2 == 1) hit(s, 45);
                    if (t == 2) { bool on = minimal ? (b % 4 == 2 || rng.chance(0.15f)) : (dens > 0.6f ? true : b % 2 == 0); if (on) hit(s, b % 2 ? 60 : 95, minimal ? 70 : 100, 0, (!minimal && rng.chance(0.05f * dens)) ? 2 : 1); }
                    if (t == 3 && b % 4 == 2 && rng.chance(minimal ? 0.3f : 0.5f)) hit(s, 85);
                    if (t == 4 && (b == 4 || b == 12) && rng.chance(0.5f)) hit(s, 100);
                    if (t >= 5 && rng.chance((minimal ? 0.04f : 0.09f) * dens)) hit(s, (int)rng.range(60, 110), rng.chance(0.4f) ? 60 : 100);
                }
                break; }
            case R_COMPLEX: case R_EXPERIMENTAL: {
                int k = std::max(1, (int)std::round(len * rng.range(0.1f, 0.5f) * dens)); if (t == 0) k = std::max(3, len / 5); if (t == 2) k = (int)(len * rng.range(0.4f, 0.8f));
                euclid(e, k, len, rng.below(len));
                for (int s = 0; s < len; ++s) if (e[(size_t)s]) hit(s, (int)rng.range(55, 120), rng.chance(0.3f) ? (int)rng.range(40, 90) : 100, rng.chance(0.15f), rng.chance(mode == R_EXPERIMENTAL ? 0.15f : 0.07f) ? 2 + rng.below(3) : 1);
                if (t >= 6 && rng.chance(0.4f)) bank.clearTrack(slot, t);
                break; }
            case R_AMBIENT: {
                if (t == 0) for (int s = 0; s < len; s += 16) hit(s, 90);
                if (t == 2) for (int s = 0; s < len; s += 2) if (rng.chance(0.5f * dens)) hit(s, (int)rng.range(30, 70), (int)rng.range(30, 80));
                if (t >= 4 && rng.chance(0.6f)) { int n = 1 + rng.below(3); for (int i = 0; i < n; ++i) hit(rng.below(len), (int)rng.range(40, 90), (int)rng.range(30, 90)); }
                if (t == 1 && rng.chance(0.5f)) hit(len / 2 + 4, 70, 60);
                break; }
            default: break;
        }
    }
}

/// Variation: keep the groove, mutate a little (ghost notes, moved perc hits, hat ratchets, probabilities).
inline void patternVariation(PatternBank& bank, int from, int to, float intensity, Rng& rng, const Locks& locks) {
    bank.copy(from, to); int len = bank.length(to); bank.names[to] = bank.names[from].empty() ? "Variation" : bank.names[from] + " var";
    int changes = 1 + (int)(intensity * 8);
    for (int i = 0; i < changes; ++i) {
        int t = 1 + rng.below(kChannels - 1); if (locks.track[t]) continue; int s = rng.below(len); Step st = bank.get(to, t, s);
        switch (rng.below(4)) {
            case 0: if (!st.on) { st.on = 1; st.vel = (uint8_t)rng.range(35, 70); } break;
            case 1: if (st.on && t >= 4) { bank.set(to, t, s, Step{}); s = rng.below(len); st.on = 1; } break;
            case 2: if (st.on && t == 2) st.ratchet = (uint8_t)(2 + rng.below(2)); break;
            case 3: if (st.on) st.prob = (uint8_t)rng.range(40, 90); break;
        }
        bank.set(to, t, s, st);
    }
    if (len >= 16 && rng.chance(0.5f) && !locks.track[1]) { Step st; st.on = 1; st.vel = 80; st.ratchet = 2; bank.set(to, 1, len - 1, st); } // fill at the end
}
} // namespace bk
