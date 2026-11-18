#pragma once
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include "Random.h"
#include "Machine.h"

static int g_pass = 0, g_fail = 0;
#define CHECK(x) do { if (x) ++g_pass; else { ++g_fail; std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); } } while (0)
#define CHECKM(x, ...) do { if (x) ++g_pass; else { ++g_fail; std::printf("FAIL %s:%d: %s | ", __FILE__, __LINE__, #x); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

namespace tu {
inline float peak(const std::vector<float>& x, size_t a = 0, size_t b = SIZE_MAX) { float p = 0; b = std::min(b, x.size()); for (size_t i = a; i < b; ++i) p = std::max(p, std::abs(x[i])); return p; }
inline double rms(const std::vector<float>& x, size_t a = 0, size_t b = SIZE_MAX) { double e = 0; b = std::min(b, x.size()); if (b <= a) return 0; for (size_t i = a; i < b; ++i) e += (double)x[i] * x[i]; return std::sqrt(e / (double)(b - a)); }
inline bool finite(const std::vector<float>& x) { for (float v : x) if (!std::isfinite(v) || std::fpclassify(v) == FP_SUBNORMAL) return false; return true; }
/// Goertzel magnitude (normalised amplitude) of frequency f in x[a..b)
inline double tone(const std::vector<float>& x, double f, double sr, size_t a = 0, size_t b = SIZE_MAX) {
    b = std::min(b, x.size()); double w = 2 * M_PI * f / sr, c = 2 * std::cos(w), s1 = 0, s2 = 0; size_t n = b - a;
    for (size_t i = a; i < b; ++i) { double hann = 0.5 - 0.5 * std::cos(2 * M_PI * (double)(i - a) / (double)n); double s = x[i] * hann + c * s1 - s2; s2 = s1; s1 = s; }
    double re = s1 - s2 * std::cos(w), im = s2 * std::sin(w); return 4.0 * std::sqrt(re * re + im * im) / (double)n;
}
/// Spectral centroid via a coarse 1/12-octave Goertzel scan.
inline double centroid(const std::vector<float>& x, double sr, size_t a = 0, size_t b = SIZE_MAX) {
    double num = 0, den = 0; for (double f = 40; f < sr * 0.45 && f < 20000; f *= std::pow(2.0, 1.0 / 12)) { double m = tone(x, f, sr, a, b); m *= m * f; num += m * f; den += m; } return den > 0 ? num / den : 0;
}
/// Average frequency by counting positive-going zero crossings in [a,b)
inline double zcFreq(const std::vector<float>& x, double sr, size_t a, size_t b) {
    b = std::min(b, x.size()); double first = -1, last = -1; int n = 0;
    for (size_t i = a + 1; i < b; ++i) if (x[i - 1] < 0 && x[i] >= 0) { double t = (double)(i - 1) + x[i - 1] / (x[i - 1] - x[i]); if (first < 0) first = t; else ++n; last = t; }
    return n > 0 ? n * sr / (last - first) : 0;
}
inline void writeWav(const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr) {
    FILE* f = std::fopen(path.c_str(), "wb"); if (!f) return; uint32_t n = (uint32_t)L.size(), data = n * 2 * 3;
    auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); }; auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f); u32(36 + data); std::fwrite("WAVEfmt ", 1, 8, f); u32(16); u16(1); u16(2); u32((uint32_t)sr); u32((uint32_t)sr * 6); u16(6); u16(24); std::fwrite("data", 1, 4, f); u32(data);
    for (uint32_t i = 0; i < n; ++i) for (int c = 0; c < 2; ++c) { float v = bk::clampf((c ? R : L)[i], -1, 1); int32_t s = (int32_t)std::lround(v * 8388607.0); uint8_t b[3] = {(uint8_t)s, (uint8_t)(s >> 8), (uint8_t)(s >> 16)}; std::fwrite(b, 1, 3, f); }
    std::fclose(f);
}
/// Render the machine with a simulated host transport. Returns stereo main out (and optional aux outs).
struct Render { std::vector<float> L, R; std::vector<std::vector<float>> aux; double seconds = 0; };
inline Render render(bk::DrumMachine& m, double sr, double seconds, double bpm, int block = 512, bool playing = true, int auxPairs = 0, double startPpq = 0,
                     std::function<void(int64_t, std::vector<bk::MidiEvent>&)> midi = nullptr) {
    Render r; int64_t total = (int64_t)(sr * seconds); r.L.resize((size_t)total); r.R.resize((size_t)total); r.aux.assign((size_t)auxPairs * 2, std::vector<float>((size_t)total));
    std::vector<float> buf((size_t)(2 + auxPairs * 2) * (size_t)block); std::vector<float*> ptr((size_t)(2 + auxPairs * 2));
    bk::HostInfo hi; hi.playing = playing; hi.hasPpq = true; hi.hasBpm = true; hi.bpm = bpm;
    std::vector<bk::MidiEvent> ev;
    for (int64_t pos = 0; pos < total; pos += block) {
        int n = (int)std::min<int64_t>(block, total - pos);
        for (size_t c = 0; c < ptr.size(); ++c) ptr[c] = buf.data() + c * (size_t)block;
        hi.ppq = startPpq + (double)pos * bpm / 60.0 / sr;
        ev.clear(); if (midi) midi(pos, ev);
        auto t0 = std::chrono::steady_clock::now();
        m.process(ptr.data(), (int)ptr.size(), n, hi, ev.data(), (int)ev.size());
        r.seconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        for (int i = 0; i < n; ++i) { r.L[(size_t)(pos + i)] = ptr[0][i]; r.R[(size_t)(pos + i)] = ptr[1][i]; for (int a = 0; a < auxPairs * 2; ++a) r.aux[(size_t)a][(size_t)(pos + i)] = ptr[(size_t)(2 + a)][i]; }
    }
    return r;
}
inline std::unique_ptr<bk::DrumMachine> machine(double sr) { auto m = std::make_unique<bk::DrumMachine>(); m->prepare(sr, 512); return m; }
inline void clearAll(bk::DrumMachine& m) { for (int p = 0; p < bk::kPatterns; ++p) m.bank.clear(p); }
/// Pure-tone kick: no click/noise/punch/sub/saturation so the pitch can be measured.
inline void pureKick(float* v, int ch) { bk::applyEngineDefaults(v, ch, bk::E_KICK); for (int k : {bk::V_CLICK, bk::V_PUNCH, bk::V_SUB, bk::V_NOISE, bk::V_SAT, bk::V_DRIVE, bk::V_DIST}) v[bk::cp(ch, k)] = 0; v[bk::cp(ch, bk::V_LEVEL)] = 0; }
}
