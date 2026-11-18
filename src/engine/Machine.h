// BAKERY — the complete drum machine: 12 voice channels, sequencer, MIDI mapping, mixer, sends, master bus, outputs.
// process() is real-time safe: fixed-size storage, no locks, no allocation, no I/O.
#pragma once
#include "Effects.h"
#include "Sequencer.h"
#include "Voice.h"

namespace bk {
struct MidiEvent { int offset; uint8_t status, d1, d2; };
struct HostInfo { bool playing = false, hasPpq = false, hasBpm = false; double ppq = 0, bpm = 120; };

class DrumMachine {
public:
    static constexpr int kChunk = 256;
    float params[kNumParams]{};
    PatternBank bank;
    // Sample hand-over (message thread publishes, audio thread acknowledges before old data may be freed).
    std::atomic<const SampleData*> samplePending[kChannels]{}, sampleAck[kChannels]{};
    // UI feedback
    std::atomic<float> meter[kChannels]{}, masterMeter[2]{}, masterGr{0}, limiterGr{0};
    std::atomic<int> playStep{-1}, playPattern{0}; std::atomic<uint32_t> hits[kChannels]{};
    std::atomic<int> audition[kChannels]{};
    std::atomic<bool> running{false};
    // CC events for MIDI learn / mapping (single producer: audio, single consumer: message thread)
    std::array<std::atomic<uint32_t>, 256> ccRing{}; std::atomic<uint32_t> ccWrite{0}, ccRead{0};
    uint64_t seedBase = 1;
    /// Test hook only (null in the plugin): records (absolute sample, channel) of every trigger.
    std::vector<std::pair<int64_t, int>>* trigLog = nullptr;

    DrumMachine() { loadDefaults(params); for (auto& s : samplePending) s.store(nullptr); for (auto& s : sampleAck) s.store(nullptr); }

    void prepare(double sampleRate, int /*maxBlock*/) {
        sr = sampleRate; os = sr < 80000 ? 2 : 1; float fsInt = (float)(sr * os);
        for (int c = 0; c < kChannels; ++c) {
            auto& ch = chans[(size_t)c];
            for (auto& v : ch.v) v.prepare(fsInt, (float)sr);
            for (auto& f : ch.fx) f.prepare((float)sr, 1.0f);
            ch.decL.reset(); ch.decR.reset(); ch.idle = 1 << 20; ch.cur = 0; ch.gl.init = ch.gr.init = false; ch.sendA.init = ch.sendB.init = false;
            sampleCur[c] = nullptr;
        }
        for (auto& f : master) f.prepare((float)sr, 2.5f);
        for (auto& f : sends) f.prepare((float)sr, 2.5f);
        limiter.prepare((float)sr); for (auto& l : auxLim) l.prepare((float)sr);
        rng.setSeed(seedBase); pendingCount = 0; clock = 0; internalPpq = 0; wasRunning = false; lastK = INT64_MIN; mvol.init = false;
    }
    double sampleRate() const { return sr; }
    int oversampling() const { return os; }

    void process(float* const* outs, int numOut, int n, const HostInfo& info, const MidiEvent* midi, int numMidi) {
        for (int c = 0; c < numOut; ++c) if (outs[c]) std::fill(outs[c], outs[c] + n, 0.f);
        for (int c = 0; c < kChannels; ++c) {
            auto* p = samplePending[c].load(std::memory_order_acquire);
            if (p != sampleCur[c]) { sampleCur[c] = p; for (auto& v : chans[(size_t)c].v) v.dropSample(); }
            sampleAck[c].store(p, std::memory_order_release);
        }
        running.store(true, std::memory_order_relaxed);
        int mi = 0;
        for (int off = 0; off < n; off += kChunk) {
            int m = std::min(kChunk, n - off);
            nev = 0;
            if (off == 0) for (int c = 0; c < kChannels; ++c) { int a = audition[c].exchange(0); if (a > 0) addEvent(0, c, 0, a / 127.f, (int64_t)(0.2 * sr)); }
            while (mi < numMidi && midi[mi].offset < off + m) { handleMidi(midi[mi], std::clamp(midi[mi].offset - off, 0, m - 1)); ++mi; }
            runSequencer(info, off, m);
            for (int i = 0; i < pendingCount;) { // due ratchets / humanized hits
                if (pending[i].time < clock + m) { auto e = pending[i]; addEvent((int)std::max<int64_t>(0, e.time - clock), e.ch, 0, e.vel, e.gate); pending[i] = pending[--pendingCount]; }
                else ++i;
            }
            std::stable_sort(events.begin(), events.begin() + nev, [](const Ev& a, const Ev& b) { return a.offset < b.offset; });
            for (int c = 0; c < kChannels; ++c) { auto& ch = chans[(size_t)c]; std::fill(ch.osL.begin(), ch.osL.begin() + m * os, 0.f); std::fill(ch.osR.begin(), ch.osR.begin() + m * os, 0.f); }
            int pos = 0;
            for (int e = 0; e < nev; ++e) { int o = std::clamp(events[(size_t)e].offset, 0, m - 1); if (o > pos) { renderVoices(pos, o); pos = o; } apply(events[(size_t)e]); }
            renderVoices(pos, m);
            mixChunk(outs, numOut, off, m, info.hasBpm ? info.bpm : (double)params[gp(G_TEMPO)]);
            clock += m;
        }
    }

    /// Trigger now (used by tests and the offline renderer). vel 0..1.3
    void triggerNow(int ch, float vel, int64_t gate = -1) { addEventDirect(ch, vel, gate); }

private:
    struct Channel {
        DrumVoice v[2]; int cur = 0; Decimator2 decL, decR; FxSlot fx[2];
        std::array<float, kChunk * 2> osL{}, osR{}; std::array<float, kChunk> L{}, R{};
        Ramp gl, gr, sendA, sendB; int idle = 1 << 20;
    };
    struct Ev { int offset; int ch; int type; float vel; int64_t gate; };
    struct Pending { int64_t time; int ch; float vel; int64_t gate; };
    std::array<Channel, kChannels> chans; std::array<FxSlot, kMasterSlots> master; std::array<FxSlot, kSendSlots> sends;
    const SampleData* sampleCur[kChannels]{};
    Limiter limiter, auxLim[kChannels]; Ramp mvol;
    std::array<Ev, 2048> events{}; int nev = 0;
    std::array<Pending, 1024> pending{}; int pendingCount = 0;
    std::array<float, kChunk> mainL{}, mainR{}, aL{}, aR{}, bL{}, bR{};
    std::array<std::array<float, kChunk>, kChannels> auxL{}, auxR{};
    double sr = 48000; int os = 2; int64_t clock = 0; double internalPpq = 0; bool wasRunning = false; int64_t lastK = INT64_MIN; Rng rng;

    void addEvent(int offset, int ch, int type, float vel, int64_t gate) { if (nev < (int)events.size()) events[(size_t)nev++] = {offset, ch, type, vel, gate}; }
    void addEventDirect(int ch, float vel, int64_t gate) { doTrigger(ch, vel, gate, clock); }

    void handleMidi(const MidiEvent& e, int o) {
        int type = e.status & 0xF0, chn = (e.status & 0x0F) + 1, want = (int)params[gp(G_MIDICH)];
        if (want != 0 && chn != want) return;
        if (type == 0x90 && e.d2 > 0) { for (int c = 0; c < kChannels; ++c) if ((int)std::lround(params[cp(c, V_NOTE)]) == e.d1) addEvent(o, c, 0, e.d2 / 127.f, -1); }
        else if (type == 0x80 || (type == 0x90 && e.d2 == 0)) { for (int c = 0; c < kChannels; ++c) if ((int)std::lround(params[cp(c, V_NOTE)]) == e.d1) addEvent(o, c, 1, 0, 0); }
        else if (type == 0xB0) {
            if (e.d1 == 120 || e.d1 == 123) { for (int c = 0; c < kChannels; ++c) addEvent(o, c, 1, 0, 0); }
            uint32_t w = ccWrite.load(std::memory_order_relaxed);
            if (w - ccRead.load(std::memory_order_acquire) < ccRing.size()) { ccRing[w % ccRing.size()].store((uint32_t)e.d1 << 8 | e.d2, std::memory_order_relaxed); ccWrite.store(w + 1, std::memory_order_release); }
        }
    }

    void runSequencer(const HostInfo& info, int off, int m) {
        int mode = (int)params[gp(G_SEQ)]; double bpm = info.hasBpm && info.bpm > 1 ? info.bpm : (double)params[gp(G_TEMPO)];
        double dpps = bpm / 60.0 / sr; bool run = false; double ppq0 = 0;
        if (mode > 0 && info.playing && info.hasPpq) { run = true; ppq0 = info.ppq + off * dpps; }
        else if (mode == 2 && params[gp(G_RUN)] > 0.5f) { run = true; ppq0 = internalPpq; internalPpq += m * dpps; }
        else internalPpq = 0;
        if (run != wasRunning) {
            pendingCount = 0; lastK = INT64_MIN;
            if (!run) { for (int c = 0; c < kChannels; ++c) addEvent(0, c, 1, 0, 0); playStep.store(-1); }
            wasRunning = run;
        }
        if (!run) return;
        int pat = std::clamp((int)params[gp(G_PATTERN)] - 1, 0, kPatterns - 1), len = bank.length(pat);
        double L = stepBeats((int)params[gp(G_RATE)]), sw = swingOffset(params[gp(G_SWING)]), ppq1 = ppq0 + m * dpps;
        float human = params[gp(G_HUMAN)];
        for (int64_t k = (int64_t)std::floor(ppq0 / L) - 1; k <= (int64_t)std::floor(ppq1 / L); ++k) {
            double tk = (double)k * L + ((k & 1) ? sw * L : 0.0);
            if (tk < ppq0 - 1e-9 || tk >= ppq1 - 1e-9 || k == lastK) continue;
            lastK = k;
            int o = std::clamp((int)std::floor((tk - ppq0) / dpps + 1e-6), 0, m - 1);
            int s = (int)(((k % len) + len) % len);
            playStep.store(s, std::memory_order_relaxed); playPattern.store(pat, std::memory_order_relaxed);
            double stepSamples = L / dpps;
            for (int t = 0; t < kChannels; ++t) {
                Step st = bank.get(pat, t, s); if (!st.on) continue;
                if (st.prob < 100 && rng.uni() * 100.f >= (float)st.prob) continue;
                float vel = st.vel / 127.f; if (st.accent) vel = std::max(vel, 1.f) * 1.25f;
                if (human > 0) vel = clampf(vel * (1.f + human * 0.15f * rng.bi()), 0.05f, 1.3f);
                int64_t delay = human > 0 ? (int64_t)(human * rng.uni() * 0.010 * sr) : 0;
                int r = std::max<int>(1, st.ratchet); int64_t gate = std::max<int64_t>(16, (int64_t)(stepSamples * 0.9 / r));
                for (int j = 0; j < r; ++j) {
                    int64_t at = (int64_t)o + delay + (int64_t)std::llround(j * stepSamples / r);
                    float v = j == 0 ? vel : vel * 0.85f;
                    if (at < m) addEvent((int)at, t, 0, v, gate);
                    else if (pendingCount < (int)pending.size()) pending[(size_t)pendingCount++] = {clock + at, t, v, gate};
                }
            }
        }
    }

    void doTrigger(int c, float vel, int64_t gate, int64_t when) {
        auto& ch = chans[(size_t)c]; const float* p = &params[cp(c, 0)];
        int g = (int)p[V_CHOKE];
        if (g > 0) for (int d = 0; d < kChannels; ++d) if (d != c && (int)params[cp(d, V_CHOKE)] == g) for (auto& v : chans[(size_t)d].v) v.fadeOut(0.008f);
        auto& old = ch.v[ch.cur]; if (old.active()) old.fadeOut(0.004f);
        ch.cur ^= 1; auto& nv = ch.v[ch.cur]; if (nv.active()) nv.fadeOut(0.001f);
        nv.reset(); nv.trigger(p, vel, gate < 0 ? -1 : gate * os, seedBase * 1000003ull + (uint64_t)when * 31ull + (uint64_t)c, sampleCur[c]);
        ch.idle = 0; hits[c].fetch_add(1, std::memory_order_relaxed);
    }
    void apply(const Ev& e) {
        if (e.type == 0) { if (trigLog) trigLog->push_back({clock + e.offset, e.ch}); doTrigger(e.ch, e.vel, e.gate, clock + e.offset); }
        else for (auto& v : chans[(size_t)e.ch].v) v.release();
    }
    void renderVoices(int a, int b) {
        if (b <= a) return;
        for (auto& ch : chans) for (auto& v : ch.v) if (v.active()) v.render(ch.osL.data() + a * os, ch.osR.data() + a * os, (b - a) * os);
    }

    void mixChunk(float* const* outs, int numOut, int off, int m, double bpm) {
        std::fill(mainL.begin(), mainL.begin() + m, 0.f); std::fill(mainR.begin(), mainR.begin() + m, 0.f);
        std::fill(aL.begin(), aL.begin() + m, 0.f); std::fill(aR.begin(), aR.begin() + m, 0.f); std::fill(bL.begin(), bL.begin() + m, 0.f); std::fill(bR.begin(), bR.begin() + m, 0.f);
        bool anySolo = false; for (int c = 0; c < kChannels; ++c) anySolo |= params[cp(c, M_SOLO)] > 0.5f;
        bool auxUsed[kChannels]{};
        for (int c = 0; c < kChannels; ++c) {
            auto& ch = chans[(size_t)c]; const float* p = &params[cp(c, 0)];
            bool voices = ch.v[0].active() || ch.v[1].active();
            bool fxOn = (int)p[F1_TYPE] != FX_OFF || (int)p[F2_TYPE] != FX_OFF;
            if (voices) ch.idle = 0;
            bool decActive = ch.idle < HalfBand::N * 2 + m;
            if (!decActive && !fxOn) { meter[c].store(meter[c].load(std::memory_order_relaxed) * 0.7f, std::memory_order_relaxed); ch.gl.init = false; continue; }
            if (decActive) {
                if (os == 2) for (int i = 0; i < m; ++i) { ch.L[(size_t)i] = ch.decL.process(ch.osL[(size_t)(2 * i)], ch.osL[(size_t)(2 * i + 1)]); ch.R[(size_t)i] = ch.decR.process(ch.osR[(size_t)(2 * i)], ch.osR[(size_t)(2 * i + 1)]); }
                else { std::copy(ch.osL.begin(), ch.osL.begin() + m, ch.L.begin()); std::copy(ch.osR.begin(), ch.osR.begin() + m, ch.R.begin()); }
                if (!voices) ch.idle += m;
            } else { std::fill(ch.L.begin(), ch.L.begin() + m, 0.f); std::fill(ch.R.begin(), ch.R.begin() + m, 0.f); }
            ch.fx[0].process(ch.L.data(), ch.R.data(), m, &p[F1_TYPE], bpm);
            ch.fx[1].process(ch.L.data(), ch.R.data(), m, &p[F2_TYPE], bpm);
            bool audible = anySolo ? p[M_SOLO] > 0.5f : p[M_MUTE] < 0.5f;
            float vol = p[M_VOL] <= -59.9f ? 0.f : dbToGain(p[M_VOL]); if (!audible) vol = 0; if (p[M_PHASE] > 0.5f) vol = -vol;
            float ang = (clampf(p[M_PAN], -1, 1) + 1.f) * kPi * 0.25f;
            ch.gl.setTarget(vol * std::cos(ang) * 1.41421356f, m); ch.gr.setTarget(vol * std::sin(ang) * 1.41421356f, m);
            ch.sendA.setTarget(p[M_SENDA], m); ch.sendB.setTarget(p[M_SENDB], m);
            int o = (int)p[M_OUT]; float* dl = mainL.data(); float* dr = mainR.data();
            if (o >= 1 && o <= kChannels && numOut >= 2 + 2 * o && outs[2 * o] && outs[2 * o + 1]) { dl = auxL[(size_t)o - 1].data(); dr = auxR[(size_t)o - 1].data(); if (!auxUsed[o - 1]) { std::fill(dl, dl + m, 0.f); std::fill(dr, dr + m, 0.f); auxUsed[o - 1] = true; } }
            float pk = 0;
            for (int i = 0; i < m; ++i) {
                float l = ch.L[(size_t)i] * ch.gl.tick(), r = ch.R[(size_t)i] * ch.gr.tick();
                dl[i] += l; dr[i] += r; pk = std::max(pk, std::max(std::abs(l), std::abs(r)));
                float sa = ch.sendA.tick(), sb = ch.sendB.tick(); aL[(size_t)i] += l * sa; aR[(size_t)i] += r * sa; bL[(size_t)i] += l * sb; bR[(size_t)i] += r * sb;
            }
            meter[c].store(std::max(pk, meter[c].load(std::memory_order_relaxed) * 0.7f), std::memory_order_relaxed);
        }
        sends[0].process(aL.data(), aR.data(), m, &params[sp(0, 0)], bpm);
        sends[1].process(bL.data(), bR.data(), m, &params[sp(1, 0)], bpm);
        float ra = params[gp(G_RETA)], rb = params[gp(G_RETB)];
        for (int i = 0; i < m; ++i) { mainL[(size_t)i] += aL[(size_t)i] * ra + bL[(size_t)i] * rb; mainR[(size_t)i] += aR[(size_t)i] * ra + bR[(size_t)i] * rb; }
        for (int s = 0; s < kMasterSlots; ++s) master[(size_t)s].process(mainL.data(), mainR.data(), m, &params[mp(s, 0)], bpm);
        float gr = 0; for (auto& f : master) gr = std::max(gr, f.gainReduction()); masterGr.store(gr, std::memory_order_relaxed);
        float mv = params[gp(G_MVOL)] <= -59.9f ? 0.f : dbToGain(params[gp(G_MVOL)]); mvol.setTarget(mv, m);
        float ceil = dbToGain(clampf(params[gp(G_CEIL)], -12.f, 0.f));
        float pl = 0, pr = 0;
        for (int i = 0; i < m; ++i) {
            float g = mvol.tick(), l = mainL[(size_t)i] * g, r = mainR[(size_t)i] * g;
            if (!std::isfinite(l) || !std::isfinite(r)) { l = r = 0; }
            limiter.process(l, r, ceil);
            if (outs[0]) outs[0][off + i] = l; if (numOut > 1 && outs[1]) outs[1][off + i] = r;
            pl = std::max(pl, std::abs(l)); pr = std::max(pr, std::abs(r));
        }
        limiterGr.store(limiter.reduction(), std::memory_order_relaxed);
        masterMeter[0].store(std::max(pl, masterMeter[0].load(std::memory_order_relaxed) * 0.7f), std::memory_order_relaxed);
        masterMeter[1].store(std::max(pr, masterMeter[1].load(std::memory_order_relaxed) * 0.7f), std::memory_order_relaxed);
        for (int o = 0; o < kChannels; ++o) {
            if (!auxUsed[o]) continue;
            float* ol = outs[2 + 2 * o]; float* orr = outs[3 + 2 * o];
            for (int i = 0; i < m; ++i) { float l = auxL[(size_t)o][(size_t)i], r = auxR[(size_t)o][(size_t)i]; if (!std::isfinite(l) || !std::isfinite(r)) l = r = 0; auxLim[o].process(l, r, ceil); ol[off + i] = l; orr[off + i] = r; }
        }
    }
};
} // namespace bk
