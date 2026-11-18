// BAKERY — shared DSP building blocks (pure C++, no JUCE). Real-time safe: no allocation after prepare().
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace bk {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float dbToGain(float db) { return std::pow(10.f, db * 0.05f); }
inline float gainToDb(float g) { return 20.f * std::log10(std::max(g, 1e-9f)); }
inline float flushTiny(float x) { return std::abs(x) < 1e-18f ? 0.f : x; }
inline bool isSane(float x) { return std::isfinite(x) && std::abs(x) < 1e6f; }
/// Coefficient for an exponential decay that falls 60 dB in `seconds`.
inline float t60Coef(float seconds, float fs) { return std::exp(std::log(0.001f) / std::max(1e-5f, seconds * fs)); }
/// One-pole smoothing coefficient with time constant `seconds`.
inline float tauCoef(float seconds, float fs) { return std::exp(-1.f / std::max(1e-6f, seconds * fs)); }

struct Rng {
    uint64_t s = 0x9E3779B97F4A7C15ull;
    explicit Rng(uint64_t seed = 1) { setSeed(seed); }
    void setSeed(uint64_t seed) { s = seed * 0x9E3779B97F4A7C15ull + 0x632BE59BD9B4E019ull; if (!s) s = 1; next(); next(); }
    uint32_t next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 32); }
    float uni() { return (next() >> 8) * (1.f / 16777216.f); }          // [0,1)
    float bi() { return uni() * 2.f - 1.f; }                             // [-1,1)
    float range(float a, float b) { return a + (b - a) * uni(); }
    int below(int n) { return n <= 0 ? 0 : (int)(next() % (uint32_t)n); }
    bool chance(float p) { return uni() < p; }
};

/// White / pink / brown noise. colour 0 = white, 0.5 = pink, 1 = brown(ish red).
struct Noise {
    Rng rng;
    float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0, brown = 0;
    void seed(uint64_t s) { rng.setSeed(s); b0 = b1 = b2 = b3 = b4 = b5 = b6 = brown = 0; }
    float white() { return rng.bi(); }
    float tick(float colour) {
        float w = rng.bi();
        if (colour <= 0.001f) return w;
        // Paul Kellet's refined pink filter (-3 dB/oct)
        b0 = 0.99886f * b0 + w * 0.0555179f; b1 = 0.99332f * b1 + w * 0.0750759f;
        b2 = 0.96900f * b2 + w * 0.1538520f; b3 = 0.86650f * b3 + w * 0.3104856f;
        b4 = 0.55000f * b4 + w * 0.5329522f; b5 = -0.7616f * b5 - w * 0.0168980f;
        float pink = (b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f) * 0.11f; b6 = w * 0.115926f;
        if (colour <= 0.5f) return lerpf(w, pink, colour * 2.f);
        brown = flushTiny(brown * 0.985f + w * 0.12f);                    // leaky integrator (-6 dB/oct)
        return lerpf(pink, brown * 2.2f, (colour - 0.5f) * 2.f);
    }
};

/// Zavalishin/Cytomic TPT state-variable filter. Stable under fast modulation.
struct Svf {
    float g = 0, k = 2, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
    float lp = 0, bp = 0, hp = 0;
    void set(float hz, float res, float fs) {
        hz = clampf(hz, 5.f, fs * 0.49f);
        g = std::tan(kPi * hz / fs);
        k = 2.f - 1.96f * clampf(res, 0.f, 1.f);
        a1 = 1.f / (1.f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    void setQ(float hz, float q, float fs) { hz = clampf(hz, 5.f, fs * 0.49f); g = std::tan(kPi * hz / fs); k = 1.f / std::max(q, 0.05f); a1 = 1.f / (1.f + g * (g + k)); a2 = g * a1; a3 = g * a2; }
    void reset() { ic1 = ic2 = lp = bp = hp = 0; }
    inline void tick(float x) {
        float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = flushTiny(2.f * v1 - ic1); ic2 = flushTiny(2.f * v2 - ic2);
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
    /// mode 0 LP, 1 HP, 2 BP (unity peak), 3 Notch
    inline float process(float x, int mode) {
        tick(x);
        switch (mode) { case 1: return hp; case 2: return bp * k; case 3: return x - k * bp; default: return lp; }
    }
};

struct OnePole {
    float a = 0, z = 0;
    void set(float hz, float fs) { a = std::exp(-kTwoPi * clampf(hz, 1.f, fs * 0.49f) / fs); }
    inline float lp(float x) { z = flushTiny(x + a * (z - x)); return z; }
    inline float hp(float x) { return x - lp(x); }
    void reset() { z = 0; }
};

struct DcBlocker {
    float r = 0.999f, x1 = 0, y1 = 0;
    void prepare(float fs) { r = 1.f - kTwoPi * 8.f / fs; }
    inline float tick(float x) { float y = x - x1 + r * y1; x1 = x; y1 = flushTiny(y); return y; }
    void reset() { x1 = y1 = 0; }
};

/// Two-pole modal resonator: impulse of 1 rings as amp*sin at `hz` decaying 60 dB in t60 seconds.
struct Mode {
    float a1 = 0, a2 = 0, b = 0, y1 = 0, y2 = 0;
    void set(float hz, float t60, float fs, float amp = 1.f) {
        float w = kTwoPi * clampf(hz, 10.f, fs * 0.45f) / fs;
        float r = std::pow(0.001f, 1.f / std::max(1e-4f, t60 * fs));
        a1 = 2.f * r * std::cos(w); a2 = -r * r; b = amp * std::sin(w);
    }
    void reset() { y1 = y2 = 0; }
    inline float tick(float x) { float y = a1 * y1 + a2 * y2 + b * x; y2 = y1; y1 = flushTiny(y); return y; }
};

inline float polyBlep(float t, float dt) {
    if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}
/// Band-limited (polyBLEP) square oscillator.
struct BlepSquare {
    float ph = 0;
    inline float tick(float dt) {
        dt = clampf(dt, 0.f, 0.45f);
        float v = ph < 0.5f ? 1.f : -1.f;
        v += polyBlep(ph, dt);
        float p2 = ph + 0.5f; if (p2 >= 1.f) p2 -= 1.f;
        v -= polyBlep(p2, dt);
        ph += dt; if (ph >= 1.f) ph -= 1.f;
        return v;
    }
};
/// Band-limited saw.
struct BlepSaw {
    float ph = 0;
    inline float tick(float dt) { dt = clampf(dt, 0.f, 0.45f); float v = 2.f * ph - 1.f - polyBlep(ph, dt); ph += dt; if (ph >= 1.f) ph -= 1.f; return v; }
};

inline float sinCycle(float phase) { return std::sin(kTwoPi * phase); }
inline float triCycle(float phase) { phase -= std::floor(phase); return phase < 0.25f ? 4.f * phase : (phase < 0.75f ? 2.f - 4.f * phase : 4.f * phase - 4.f); }
inline float wrap01(float p) { return p - std::floor(p); }

// ---- Waveshapers (all bounded to about +-1.2) ----
inline float softClip(float x) { return std::tanh(x); }
inline float hardClip(float x) { return clampf(x, -1.f, 1.f); }
inline float foldShape(float x) { // triangle wavefolder: smooth, bounded
    float t = wrap01((x + 1.f) * 0.25f);
    return 1.f - 4.f * std::abs(t - 0.5f);
}
inline float asymShape(float x) { return std::tanh(x + 0.35f) - 0.33637f; }
inline float cubicSoft(float x) { x = clampf(x, -1.5f, 1.5f); return x - (4.f / 27.f) * x * x * x; }
/// mode: 0 soft (tanh), 1 hard clip, 2 fold, 3 asymmetric, 4 cubic
inline float shapeMode(float x, int mode) {
    switch (mode) { case 1: return hardClip(x); case 2: return foldShape(x); case 3: return asymShape(x); case 4: return cubicSoft(x) * 0.888f; default: return softClip(x); }
}

/// Windowed-sinc half-band FIR used for 2x oversampling (decimate and interpolate).
struct HalfBand {
    static constexpr int N = 47;
    static const std::array<float, N>& taps() {
        static const std::array<float, N> h = [] {
            std::array<float, N> t{}; const int M = N - 1; double sum = 0;
            for (int i = 0; i < N; ++i) {
                double n = i - M / 2.0, s = n == 0 ? 0.5 : std::sin(3.14159265358979 * 0.5 * n) / (3.14159265358979 * n);
                double w = 0.42 - 0.5 * std::cos(2 * 3.14159265358979 * i / M) + 0.08 * std::cos(4 * 3.14159265358979 * i / M);
                t[(size_t)i] = (float)(s * w); sum += s * w;
            }
            for (auto& v : t) v = (float)(v / sum);
            return t; }();
        return h;
    }
};
struct Decimator2 {
    std::array<float, HalfBand::N * 2> buf{}; int pos = 0;
    void reset() { buf.fill(0); pos = 0; }
    inline void push(float x) { buf[(size_t)pos] = x; buf[(size_t)pos + HalfBand::N] = x; if (++pos >= HalfBand::N) pos = 0; }
    /// push two oversampled samples, return one base-rate sample
    inline float process(float a, float b) {
        push(a); push(b);
        const auto& h = HalfBand::taps(); const float* p = &buf[(size_t)pos]; float acc = 0;
        for (int i = 0; i < HalfBand::N; ++i) acc += h[(size_t)i] * p[i];
        return acc;
    }
};
struct Interpolator2 {
    std::array<float, HalfBand::N * 2> buf{}; int pos = 0;
    void reset() { buf.fill(0); pos = 0; }
    inline float out() const { const auto& h = HalfBand::taps(); const float* p = &buf[(size_t)pos]; float acc = 0; for (int i = 0; i < HalfBand::N; ++i) acc += h[(size_t)i] * p[i]; return acc * 2.f; }
    inline void push(float x) { buf[(size_t)pos] = x; buf[(size_t)pos + HalfBand::N] = x; if (++pos >= HalfBand::N) pos = 0; }
    inline void process(float x, float& o1, float& o2) { push(x); o1 = out(); push(0.f); o2 = out(); }
};

/// ADSR with gate. Times: attack linear (s), decay/release are T60 exponential (s).
struct Adsr {
    enum Stage { Idle, Attack, Decay, Sustain, Release } stage = Idle;
    float v = 0, aInc = 1, dCoef = 0, rCoef = 0, sus = 0; int64_t gate = -1; // samples remaining in gate, -1 = held until release()
    void trigger(float att, float dec, float s, float rel, float fs, int64_t gateSamples) {
        aInc = att <= 0.00005f ? 1.f : 1.f / (att * fs);
        dCoef = t60Coef(dec, fs); rCoef = t60Coef(rel, fs); sus = clampf(s, 0, 1); gate = gateSamples;
        stage = Attack; if (aInc >= 1.f) { v = 1.f; stage = Decay; }
    }
    /// Drum behaviour: with Sustain at 0 the envelope is a one-shot (gate/note-off ignored).
    void release() { if (stage != Idle && sus > 0.f) stage = Release; }
    inline float tick() {
        if (gate > 0 && --gate == 0) release();
        switch (stage) {
            case Attack: v += aInc; if (v >= 1.f) { v = 1.f; stage = Decay; } break;
            case Decay: v = sus + (v - sus) * dCoef; if (v - sus < 1e-5f) { v = sus; stage = sus <= 0.f ? Idle : Sustain; } break;
            case Sustain: v = sus; break;
            case Release: v *= rCoef; if (v < 1e-5f) { v = 0; stage = Idle; } break;
            case Idle: v = 0; break;
        }
        return v;
    }
    bool active() const { return stage != Idle; }
};

/// Exponential decay envelope (one-shot).
struct Decay {
    float v = 0, c = 0;
    void trigger(float seconds, float fs, float start = 1.f) { v = start; c = t60Coef(seconds, fs); }
    inline float tick() { float o = v; v = flushTiny(v * c); return o; }
};

/// Fixed-capacity delay line (allocated in prepare only).
struct DelayLine {
    std::vector<float> d; int w = 0, mask = 0;
    void prepare(int maxSamples) { int n = 1; while (n < maxSamples + 4) n <<= 1; d.assign((size_t)n, 0.f); mask = n - 1; w = 0; }
    void reset() { std::fill(d.begin(), d.end(), 0.f); w = 0; }
    inline void push(float x) { if (d.empty()) return; d[(size_t)w] = x; w = (w + 1) & mask; }
    inline float tap(float delay) const {
        if (d.empty()) return 0.f;
        delay = clampf(delay, 1.f, (float)mask - 2.f);
        float rp = (float)w - delay; int i = (int)std::floor(rp); float f = rp - (float)i;
        float a = d[(size_t)(i & mask)], b = d[(size_t)((i + 1) & mask)];
        return a + (b - a) * f;
    }
    inline float tapInt(int delay) const { return d.empty() ? 0.f : d[(size_t)((w - delay) & mask)]; }
    int capacity() const { return mask - 2; }
};

/// Linear ramp for per-sample smoothing of a block-rate target.
struct Ramp {
    float cur = 0, inc = 0; bool init = false;
    void setTarget(float t, int n) { if (!init) { cur = t; init = true; } inc = n > 0 ? (t - cur) / (float)n : 0; if (n <= 0) cur = t; }
    inline float tick() { float o = cur; cur += inc; return o; }
};
} // namespace bk
