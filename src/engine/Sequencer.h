// BAKERY — pattern storage (lock-free, every field atomic) and Standard MIDI File export.
#pragma once
#include <atomic>
#include <string>
#include "Params.h"

namespace bk {
constexpr int kPatterns = 16, kMaxSteps = 64;
struct Step { uint8_t on = 0, vel = 100, prob = 100, accent = 0, ratchet = 1; };

/// 16 patterns x 12 tracks x 64 steps. The UI (message thread) writes, the audio thread reads; each byte is an
/// independent relaxed atomic, so no locks and no torn values — at worst a step edit lands one step later.
class PatternBank {
public:
    PatternBank() { for (int p = 0; p < kPatterns; ++p) clear(p); }
    Step get(int p, int t, int s) const {
        const auto* x = d[idx(p, t, s)]; Step st;
        st.on = x[0].load(std::memory_order_relaxed); st.vel = x[1].load(std::memory_order_relaxed); st.prob = x[2].load(std::memory_order_relaxed);
        st.accent = x[3].load(std::memory_order_relaxed); st.ratchet = x[4].load(std::memory_order_relaxed); return st;
    }
    void set(int p, int t, int s, Step st) {
        auto* x = d[idx(p, t, s)];
        st.vel = (uint8_t)std::clamp<int>(st.vel, 1, 127); st.prob = (uint8_t)std::clamp<int>(st.prob, 0, 100); st.ratchet = (uint8_t)std::clamp<int>(st.ratchet, 1, 4);
        x[1].store(st.vel, std::memory_order_relaxed); x[2].store(st.prob, std::memory_order_relaxed); x[3].store(st.accent ? 1 : 0, std::memory_order_relaxed);
        x[4].store(st.ratchet, std::memory_order_relaxed); x[0].store(st.on ? 1 : 0, std::memory_order_relaxed);
    }
    int length(int p) const { return len[clampP(p)].load(std::memory_order_relaxed); }
    void setLength(int p, int l) { len[clampP(p)].store((uint8_t)std::clamp(l, 1, kMaxSteps), std::memory_order_relaxed); }
    void clear(int p) { for (int t = 0; t < kChannels; ++t) for (int s = 0; s < kMaxSteps; ++s) set(p, t, s, Step{}); setLength(p, 16); }
    void clearTrack(int p, int t) { for (int s = 0; s < kMaxSteps; ++s) set(p, t, s, Step{}); }
    void copy(int from, int to) { if (from == to) return; for (int t = 0; t < kChannels; ++t) for (int s = 0; s < kMaxSteps; ++s) set(to, t, s, get(from, t, s)); setLength(to, length(from)); }
    int hits(int p) const { int n = 0; for (int t = 0; t < kChannels; ++t) for (int s = 0; s < length(p); ++s) n += get(p, t, s).on; return n; }
    /// Text form used for presets/state: one line per track; per step "on,vel,prob,acc,rat;"
    std::string serialise(int p) const {
        std::string out = std::to_string(length(p)) + "\n";
        for (int t = 0; t < kChannels; ++t) {
            for (int s = 0; s < kMaxSteps; ++s) { auto st = get(p, t, s); if (!st.on && st.vel == 100 && st.prob == 100 && !st.accent && st.ratchet == 1) { out += "."; continue; }
                out += "[" + std::to_string(st.on) + "," + std::to_string(st.vel) + "," + std::to_string(st.prob) + "," + std::to_string(st.accent) + "," + std::to_string(st.ratchet) + "]"; }
            out += "\n";
        }
        return out;
    }
    bool deserialise(int p, const std::string& txt) {
        size_t pos = 0; auto line = [&](std::string& l) { if (pos > txt.size()) return false; size_t e = txt.find('\n', pos); if (e == std::string::npos) e = txt.size(); l = txt.substr(pos, e - pos); pos = e + 1; return true; };
        std::string l; if (!line(l)) return false; int length = std::atoi(l.c_str()); if (length < 1 || length > kMaxSteps) return false;
        clear(p); setLength(p, length);
        for (int t = 0; t < kChannels && line(l); ++t) {
            int s = 0; for (size_t i = 0; i < l.size() && s < kMaxSteps; ++s) {
                if (l[i] == '.') { ++i; continue; }
                if (l[i] != '[') return false; size_t e = l.find(']', i); if (e == std::string::npos) return false;
                int v[5] = {0, 100, 100, 0, 1}; std::sscanf(l.substr(i + 1, e - i - 1).c_str(), "%d,%d,%d,%d,%d", &v[0], &v[1], &v[2], &v[3], &v[4]);
                Step st; st.on = (uint8_t)(v[0] != 0); st.vel = (uint8_t)std::clamp(v[1], 1, 127); st.prob = (uint8_t)std::clamp(v[2], 0, 100); st.accent = (uint8_t)(v[3] != 0); st.ratchet = (uint8_t)std::clamp(v[4], 1, 4);
                set(p, t, s, st); i = e + 1;
            }
        }
        return true;
    }
    std::string names[kPatterns];
private:
    static int clampP(int p) { return std::clamp(p, 0, kPatterns - 1); }
    static size_t idx(int p, int t, int s) { return ((size_t)clampP(p) * kChannels + (size_t)std::clamp(t, 0, kChannels - 1)) * kMaxSteps + (size_t)std::clamp(s, 0, kMaxSteps - 1); }
    std::atomic<uint8_t> d[kPatterns * kChannels * kMaxSteps][5];
    std::atomic<uint8_t> len[kPatterns];
};

inline double stepBeats(int rate) { const double b[] = {0.5, 0.25, 1.0 / 6.0, 0.125}; return b[std::clamp(rate, 0, 3)]; }
/// Swing in percent (50 = straight, 75 = heavy): odd steps are pushed later by (swing-50)/50 of a step.
inline double swingOffset(double swingPct) { return (clampf((float)swingPct, 50.f, 75.f) - 50.0) / 50.0; }

/// Standard MIDI File (format 0, 960 PPQ, channel 10) of one pass of a pattern. Each track uses its MIDI note.
inline std::vector<uint8_t> patternToMidi(const PatternBank& bank, int p, const float* params, double bpm, int passes = 1) {
    const int ppq = 960; int len = bank.length(p); int rate = (int)params[gp(G_RATE)];
    double stepT = stepBeats(rate) * ppq, sw = swingOffset(params[gp(G_SWING)]);
    struct Ev { int64_t t; uint8_t a, b, c; };
    std::vector<Ev> ev;
    for (int pass = 0; pass < passes; ++pass)
        for (int tr = 0; tr < kChannels; ++tr) {
            int note = (int)params[cp(tr, V_NOTE)];
            for (int s = 0; s < len; ++s) {
                Step st = bank.get(p, tr, s); if (!st.on) continue;
                int64_t k = (int64_t)pass * len + s; double start = (double)k * stepT + ((k & 1) ? sw * stepT : 0.0);
                int vel = std::clamp(st.accent ? std::max<int>(st.vel, 120) : (int)st.vel, 1, 127);
                for (int r = 0; r < st.ratchet; ++r) {
                    int64_t on = (int64_t)std::llround(start + r * stepT / st.ratchet), off = on + std::max<int64_t>(1, (int64_t)(stepT / st.ratchet * 0.5));
                    ev.push_back({on, (uint8_t)0x99, (uint8_t)note, (uint8_t)vel}); ev.push_back({off, (uint8_t)0x89, (uint8_t)note, (uint8_t)0});
                }
            }
        }
    std::stable_sort(ev.begin(), ev.end(), [](const Ev& a, const Ev& b) { return a.t != b.t ? a.t < b.t : (a.a & 0xF0) < (b.a & 0xF0); });
    std::vector<uint8_t> trk;
    auto vlq = [&](uint64_t v) { uint8_t buf[10]; int n = 0; buf[n++] = v & 0x7F; while (v >>= 7) buf[n++] = (uint8_t)((v & 0x7F) | 0x80); while (n) trk.push_back(buf[--n]); };
    std::string name = bank.names[std::clamp(p, 0, kPatterns - 1)].empty() ? "BAKERY Pattern " + std::to_string(p + 1) : bank.names[std::clamp(p, 0, kPatterns - 1)];
    vlq(0); trk.push_back(0xFF); trk.push_back(0x03); vlq(name.size()); trk.insert(trk.end(), name.begin(), name.end());
    uint32_t us = (uint32_t)std::llround(60000000.0 / std::max(20.0, bpm));
    vlq(0); trk.insert(trk.end(), {0xFF, 0x51, 0x03, (uint8_t)(us >> 16), (uint8_t)(us >> 8), (uint8_t)us});
    vlq(0); trk.insert(trk.end(), {0xFF, 0x58, 0x04, 4, 2, 24, 8});
    int64_t last = 0;
    for (auto& e : ev) { vlq((uint64_t)(e.t - last)); last = e.t; trk.push_back(e.a); trk.push_back(e.b); trk.push_back(e.c); }
    int64_t end = (int64_t)std::llround((double)len * passes * stepT); vlq((uint64_t)std::max<int64_t>(0, end - last)); trk.insert(trk.end(), {0xFF, 0x2F, 0x00});
    std::vector<uint8_t> f{'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, (uint8_t)(ppq >> 8), (uint8_t)ppq, 'M', 'T', 'r', 'k'};
    uint32_t n = (uint32_t)trk.size(); f.push_back((uint8_t)(n >> 24)); f.push_back((uint8_t)(n >> 16)); f.push_back((uint8_t)(n >> 8)); f.push_back((uint8_t)n);
    f.insert(f.end(), trk.begin(), trk.end());
    return f;
}
} // namespace bk
