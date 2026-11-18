// BAKERY — drum voice engines. Each engine uses its own synthesis technique (see engineBlurb()).
// A voice renders at the internal (oversampled) rate; nonlinear stages therefore run oversampled.
#pragma once
#include <memory>
#include <string>
#include "Params.h"

namespace bk {
/// Immutable decoded WAV published to the audio thread by pointer.
struct SampleData { std::vector<float> L, R; double rate = 44100; std::string name, path; int frames() const { return (int)L.size(); } };

/// Optional sample layer: start/end, pitch, reverse, loop, granular time-stretch, LP filter and envelope.
struct SamplePlayer {
    const SampleData* s = nullptr; double u = 0; float inc = 1, tpInc = 1, lvl = 0; int a = 0, b = 0; bool rev = false, loop = false, stretch = false, on = false;
    float gp[2] = {0, 0.5f}; double gstart[2] = {0, 0}; double tp = 0; float glen = 1; Adsr env; Svf lp[2]; float cut = 20000, fs = 48000;
    void start(const SampleData* sd, const float* q, float fsInt, float vel, int64_t gate) {
        s = sd; on = false; if (!s || s->frames() < 2 || q[S_LEVEL] <= 0.0001f) return;
        fs = fsInt; int n = s->frames();
        a = (int)clampf(q[S_START] * (float)(n - 1), 0.f, (float)(n - 2)); b = (int)clampf(q[S_END] * (float)n, (float)(a + 1), (float)n);
        float ratio = std::exp2(q[S_PITCH] / 12.f), st = q[S_STRETCH];
        inc = (float)(ratio * s->rate / fs); stretch = std::abs(st - 1.f) > 0.01f; tpInc = (float)(s->rate / fs / st);
        rev = q[S_REV] > 0.5f; loop = q[S_LOOP] > 0.5f; lvl = q[S_LEVEL] * vel; u = 0; tp = 0; glen = 0.04f * fs; gp[0] = 0; gp[1] = 0.5f; gstart[0] = gstart[1] = 0;
        bool hold = q[S_DEC] >= 9999.f;
        env.trigger(q[S_ATT] * 0.001f, q[S_DEC] * 0.001f, hold ? 1.f : 0.f, 0.02f, fs, gate);
        cut = q[S_CUT]; for (auto& f : lp) { f.reset(); f.set(cut, 0.f, fs); }
        on = true;
    }
    void stop() { on = false; s = nullptr; }
    void release() { if (on) { env.release(); } }
    inline float readAt(const std::vector<float>& d, double logical) const {
        int len = b - a; double x = rev ? (double)(b - 1) - logical : (double)a + logical;
        (void)len; int i = (int)std::floor(x); float f = (float)(x - i);
        auto at = [&](int k) { k = k < a ? a : (k >= b ? b - 1 : k); return d[(size_t)k]; };
        float y0 = at(i - 1), y1 = at(i), y2 = at(i + 1), y3 = at(i + 2); // Catmull-Rom
        return y1 + 0.5f * f * (y2 - y0 + f * (2.f * y0 - 5.f * y1 + 4.f * y2 - y3 + f * (3.f * (y1 - y2) + y3 - y0)));
    }
    inline void tick(float& l, float& r) {
        if (!on) return;
        double len = (double)(b - a); float e = env.tick(); if (!env.active()) { on = false; return; }
        float sl = 0, sr = 0;
        if (!stretch) {
            if (u >= len) { if (loop) u = std::fmod(u, len); else { on = false; return; } }
            sl = readAt(s->L, u); sr = readAt(s->R, u); u += inc;
        } else {
            for (int g = 0; g < 2; ++g) {
                double pos = gstart[g] + (double)gp[g] * glen * inc;
                if (loop) pos = std::fmod(pos, len);
                if (pos < len) { float w = std::sin(kPi * gp[g]); w *= w; sl += w * readAt(s->L, pos); sr += w * readAt(s->R, pos); }
                gp[g] += 1.f / glen; if (gp[g] >= 1.f) { gp[g] -= 1.f; gstart[g] = tp; }
            }
            tp += tpInc; if (tp >= len) { if (loop) tp = std::fmod(tp, len); else if (tp >= len + glen * inc) { on = false; return; } }
        }
        if (cut < 19900.f) { sl = lp[0].process(sl, 0); sr = lp[1].process(sr, 0); }
        l += sl * e * lvl; r += sr * e * lvl;
    }
};

class DrumVoice {
public:
    void prepare(float fsInternal, float fsBase) {
        fs = fsInternal; base = fsBase;
        comb.prepare((int)(fs / 15.f) + 8); stereo.prepare((int)(fs * 0.004f) + 8);
        for (auto& d : diff) d.prepare((int)(fs * 0.08f) + 8);
        reset();
    }
    void reset() { activeFlag = false; fade = 1; fadeInc = 0; smp.stop(); comb.reset(); stereo.reset(); for (auto& d : diff) d.reset(); }
    bool active() const { return activeFlag; }
    int engine() const { return eng; }
    float level() const { return lastPeak; }
    /// Start a hit. `c` points to this channel's kChanParams values. vel 0..1.3 (accent > 1).
    void trigger(const float* c, float vel, int64_t gateSamples, uint64_t seed, const SampleData* sample) {
        std::copy(c, c + kChanParams, q); eng = (int)q[V_ENGINE];
        t = 0; fade = 1; fadeInc = 0; quiet = 0; activeFlag = true; lastPeak = 0;
        float v = clampf(vel, 0.f, 1.3f); velGain = (1.f - q[V_VSENS] * (1.f - std::min(v, 1.f))) * (v > 1.f ? v : 1.f);
        gain = dbToGain(q[V_LEVEL]) * velGain * q[S_SYNTH];
        f0 = clampf(q[V_FREQ] * std::exp2(q[V_TUNE] / 12.f), 10.f, fs * 0.4f);
        sweepLn = q[V_SWEEP] / 12.f * 0.693147f; pe = 1.f; pc = tauCoef(q[V_SWEEPT] * 0.001f, fs);
        if (q[V_SWEEP] <= 0.f) pe = 0.f;
        float att = q[V_ATT] * 0.001f, dec = q[V_DEC] * 0.001f, rel = q[V_REL] * 0.001f;
        bool modal = eng == E_RIM || eng == E_WOOD;
        amp.trigger(att, modal ? dec * 1.6f + 0.02f : dec, q[V_SUS], rel, fs, gateSamples);
        amp2.trigger(att, dec * (eng == E_KICK ? 1.25f : 1.4f), q[V_SUS], rel * 1.2f, fs, gateSamples);
        attS = std::max(1.f, att * fs);
        nEnv.trigger(q[V_NDEC] * 0.001f, fs); clickEnv.trigger(0.006f, fs); punchEnv.trigger(0.035f, fs);
        fmEnv.trigger(std::max(0.01f, dec * 0.5f), fs); env2.trigger(dec * 0.55f, fs); env3.trigger(dec * 0.35f, fs);
        rng.setSeed(seed); nz[0].seed(seed * 3 + 1); nz[1].seed(seed * 7 + 5);
        for (auto& p : ph) p = 0; for (auto& s : sq) s.ph = rng.uni();
        for (auto& f : flt) f.reset(); for (auto& m : modes) m.reset(); for (auto& o : op) o.reset(); dc[0].reset(); dc[1].reset();
        dc[0].prepare(fs); dc[1].prepare(fs); tilt[0].set(700.f, fs); tilt[1].set(700.f, fs); tilt[0].reset(); tilt[1].reset();
        drive = q[V_DRIVE]; sat = q[V_SAT]; dist = q[V_DIST]; dmode = (int)q[V_DMODE]; tone = q[V_TONE];
        driveG = 1.f + 19.f * drive * drive; driveNorm = 1.f / std::tanh(driveG);
        postFilter = q[V_CUT] < 19500.f || (int)q[V_FTYPE] != 0 || q[V_RES] > 0.02f;
        ftype = (int)q[V_FTYPE];
        prevY = 0; prevBank = 0; held = 0; holdCount = 0; gph = 0; jumpCount = 0;
        setupEngine();
        smp.start(sample, q, fs, velGain, gateSamples);
    }
    void release() { amp.release(); amp2.release(); smp.release(); }
    /// Fast fade-out (used for choke and click-free retrigger).
    void fadeOut(float seconds) { if (activeFlag) fadeInc = -1.f / std::max(1.f, seconds * fs); }
    void dropSample() { smp.stop(); }
    /// Adds n internal-rate samples into L/R.
    void render(float* L, float* R, int n) {
        if (!activeFlag) return;
        for (int i = 0; i < n; ++i) {
            float l = 0, r = 0;
            if (q[S_SYNTH] > 0.0001f) synth(l, r);
            smp.tick(l, r);
            if (fadeInc != 0) { fade += fadeInc; if (fade <= 0) { fade = 0; activeFlag = false; smp.stop(); } l *= fade; r *= fade; }
            if (!std::isfinite(l) || !std::isfinite(r)) { l = r = 0; activeFlag = false; }
            L[i] += l; R[i] += r;
            float a = std::max(std::abs(l), std::abs(r)); lastPeak = std::max(lastPeak * 0.9999f, a);
            ++t;
            if (a < 2e-5f) ++quiet; else quiet = 0;
            if (quiet > (int)(fs * 0.03f) && t > (int64_t)attS + (int64_t)(fs * 0.01f) && !amp.active() && !smp.on) { activeFlag = false; }
            if (t > (int64_t)(fs * 30.f)) activeFlag = false;
            if (!activeFlag) break;
        }
    }

private:
    float q[kChanParams]{}; int eng = 0; float fs = 96000, base = 48000; int64_t t = 0; bool activeFlag = false; int quiet = 0; float lastPeak = 0;
    float fade = 1, fadeInc = 0, velGain = 1, gain = 1, f0 = 100, sweepLn = 0, pe = 0, pc = 0, attS = 1;
    float drive = 0, sat = 0, dist = 0, tone = 0, driveG = 1, driveNorm = 1; int dmode = 0, ftype = 0; bool postFilter = false;
    Adsr amp, amp2; Decay nEnv, clickEnv, punchEnv, fmEnv, env2, env3, burstEnv;
    Rng rng; Noise nz[2]; float ph[8]{}; BlepSquare sq[8]; Svf flt[6]; Mode modes[8]; OnePole op[4], tilt[2]; DcBlocker dc[2];
    DelayLine comb, stereo, diff[8]; int diffLen[8]{};
    float prevY = 0, prevBank = 0, held = 0, ratios[8]{}; int holdCount = 0, holdLen = 1, jumpCount = 0, jumpLen = 1;
    float gph = 0, gInc = 0, curF = 100, combLen = 100, blipLen = 1, ratio2 = 2, center = 5000, bitStep = 0;
    // clap
    int bursts = 4, burstIdx = 0; float burstSpacing = 400; float burstStart[8]{};
    SamplePlayer smp;

    static constexpr float k808[6] = {1.f, 1.4828f, 1.8003f, 2.5459f, 2.6303f, 3.8968f};

    float shape(float x) {
        if (drive > 0.001f) x = std::tanh(x * driveG) * driveNorm;
        if (sat > 0.001f) { float s = std::tanh(x * (1.f + 2.f * sat) + 0.3f * sat * x * x); x = lerpf(x, s, sat); }
        if (dist > 0.001f) { float d = shapeMode(x * (1.f + 7.f * dist), dmode); x = lerpf(x, d, std::min(1.f, dist * 2.f)); }
        return x;
    }
    inline float tiltTone(float x, int ch) {
        if (std::abs(tone) < 0.01f) return x;
        float lo = tilt[ch].lp(x), hi = x - lo;
        return lo * (1.f - 0.5f * std::max(0.f, tone)) + hi * (1.f + tone);
    }
    inline float post(float x, int ch) {
        x = tiltTone(x, ch);
        if (postFilter) x = flt[4 + ch].process(x, ftype);
        return dc[ch].tick(x);
    }
    inline float blip() { return (float)t < blipLen ? std::sin(kPi * (float)t / blipLen) : 0.f; }
    inline float attackRamp() const { return (float)t < attS ? (float)t / attS : 1.f; }
    inline float pitchNow() { float f = f0; if (pe > 0.f) { f = f0 * std::exp(sweepLn * pe); pe *= pc; if (pe < 1e-6f) pe = 0; } return std::min(f, fs * 0.45f); }
    /// Stereo decorrelation with a short delay (mono compatible-ish) scaled by Width.
    inline float widen(float x) { stereo.push(x); return lerpf(x, stereo.tap(fs * 0.00063f), q[V_WIDTH] * 0.8f); }
    inline float allpass(DelayLine& d, int len, float x, float g) { float v = d.tapInt(len); float w = x + g * v; d.push(w); return v - g * w; }

    void setupEngine() {
        float res = q[V_RES], harm = q[V_HARM];
        for (int c = 0; c < 2; ++c) flt[4 + c].set(q[V_CUT], res, fs);
        blipLen = fs * 0.0012f;
        switch (eng) {
            case E_KICK: op[0].set(1800.f, fs); blipLen = fs * (0.0006f + 0.0012f * (1.f - (tone + 1.f) * 0.5f)); break;
            case E_SNARE:
                ratio2 = 1.5f + harm;
                for (int c = 0; c < 2; ++c) flt[c].set(q[V_CUT], res, fs);
                flt[2].setQ(4200.f, 0.9f, fs); flt[3].setQ(4200.f, 0.9f, fs);
                snapEnvSet(); break;
            case E_CHAT: case E_OHAT: {
                float ex = 0.84f + 0.8f * harm;
                for (int i = 0; i < 6; ++i) ratios[i] = std::pow(k808[i], ex);
                center = expMap((tone + 1.f) * 0.5f, 2500.f, 12000.f);
                flt[0].setQ(center, 1.2f, fs); flt[1].set(q[V_CUT], res * 0.9f, fs); postFilter = false; break; }
            case E_CLAP: {
                bursts = 2 + (int)std::round(harm * 5.f); burstSpacing = clampf(q[V_SWEEPT], 2.f, 40.f) * 0.001f * fs;
                for (int b = 0; b < bursts; ++b) burstStart[b] = (float)b * burstSpacing + (b > 0 ? rng.bi() * q[V_MOD] * 0.35f * burstSpacing : 0.f);
                burstIdx = 0; burstEnv.trigger(burstSpacing / fs * 1.1f, fs, 0.f);
                for (int c = 0; c < 2; ++c) flt[c].setQ(q[V_FREQ], 0.8f + res * 6.f, fs);
                op[0].set(400.f, fs); op[1].set(400.f, fs);
                amp.trigger(q[V_ATT] * 0.001f, q[V_DEC] * 0.001f, q[V_SUS], q[V_REL] * 0.001f, fs, -1); amp.stage = Adsr::Idle; amp.v = 0; // tail starts after last burst
                tailStarted = false; break; }
            case E_TOM:
                flt[0].setQ(f0 * 5.f, 1.5f, fs);
                modes[0].set(f0, q[V_DEC] * 0.001f * (0.3f + res), fs, 1.f); break;
            case E_RIM:
                modes[0].set(f0, q[V_DEC] * 0.001f, fs, 1.f); modes[1].set(f0 * (2.f + harm * 3.f), q[V_DEC] * 0.0008f, fs, 0.8f);
                op[0].set(300.f, fs); op[1].set(2500.f, fs); break;
            case E_COWBELL: ratio2 = 1.25f + harm * 0.5f; flt[0].setQ(q[V_CUT], 0.7f + res * 5.f, fs); break;
            case E_WOOD: {
                float qm = 0.4f + res * 1.2f, d = q[V_DEC] * 0.001f * qm;
                modes[0].set(f0, d, fs, 1.f); modes[1].set(f0 * (2.f + harm * 2.5f), d * 0.5f, fs, 0.5f); modes[2].set(f0 * (4.2f + harm * 3.f), d * 0.25f, fs, 0.25f);
                op[0].set(6000.f, fs); break; }
            case E_METAL: {
                static const float bar[4] = {1.f, 2.756f, 5.404f, 8.933f}; float ex = 0.7f + harm * 0.6f, d = q[V_DEC] * 0.001f;
                const float dm[4] = {1.f, 0.7f, 0.5f, 0.35f}, am[4] = {1.f, 0.6f, 0.4f, 0.25f};
                for (int i = 0; i < 4; ++i) modes[i].set(f0 * std::pow(bar[i], ex), d * dm[i], fs, am[i]);
                ratio2 = 1.13f + harm * 2.7f; break; }
            case E_CLICK: flt[0].setQ(f0, 0.7f + res * 8.f, fs); break;
            case E_IMPACT: {
                for (int c = 0; c < 2; ++c) flt[c].set(q[V_CUT], res * 0.5f, fs);
                static const float dl[8] = {11.3f, 17.9f, 23.7f, 31.1f, 13.1f, 19.7f, 27.3f, 35.9f};
                for (int i = 0; i < 8; ++i) diffLen[i] = std::max(4, (int)(dl[i] * 0.001f * fs * (0.5f + q[V_MOD]) * (i >= 4 ? 1.f : 1.f)));
                for (auto& d : diff) d.reset(); break; }
            case E_INDUSTRIAL:
                combLen = clampf(fs / (f0 * (1.f + harm * 4.f)), 2.f, (float)comb.capacity()); comb.reset();
                bitStep = q[V_MOD] > 0.01f ? 2.f / std::exp2(16.f - q[V_MOD] * 13.f) : 0.f; holdLen = 1 + (int)(q[V_MOD] * 12.f); holdCount = 0; break;
            case E_EXPERIMENTAL:
                curF = f0; jumpLen = std::max(1, (int)(q[V_SWEEPT] * 0.001f * fs)); jumpCount = 0; gInc = (4.f + q[V_MOD] * 40.f) / fs; gph = 0;
                flt[0].setQ(f0 * 2.f, 2.f, fs); pe = 0; break;
            default: break;
        }
    }
    Decay snapEnv; bool tailStarted = false;
    void snapEnvSet() { snapEnv.trigger(0.006f + 0.03f * (1.f - q[V_SNAP]), fs); }

    void synth(float& L, float& R) {
        const float g = gain;
        switch (eng) {
            case E_KICK: {
                float f = pitchNow(), fe = fmEnv.tick(), a = amp.tick(), a2 = amp2.tick(), pu = punchEnv.tick();
                ph[1] = wrap01(ph[1] + f * q[V_FMRATIO] / fs); float mod = sinCycle(ph[1]);
                ph[0] = wrap01(ph[0] + f / fs); float pm = q[V_FMAMT] * 1.2f * fe * mod;
                float car = lerpf(sinCycle(ph[0] + pm), triCycle(ph[0] + pm), q[V_WAVE]);
                car *= 1.f - q[V_RING] + q[V_RING] * mod;
                float body = car * q[V_BODY] * a * (1.f + q[V_PUNCH] * 1.6f * pu) * attackRamp();
                float ce = clickEnv.tick(), cn = op[0].hp(nz[0].white());
                float click = q[V_CLICK] * (0.6f * cn * ce + 0.7f * blip());
                float ns = nz[1].tick(q[V_NCOLOR]) * q[V_NOISE] * nEnv.tick() * 0.5f;
                float x = post(shape(body + click + ns), 0);
                ph[2] = wrap01(ph[2] + f / fs);
                x += sinCycle(ph[2]) * q[V_SUB] * a2 * attackRamp() * 0.8f;
                L += x * g; R += x * g; break;
            }
            case E_SNARE: {
                float f1 = pitchNow(), a = amp.tick(), e2 = env2.tick(), fe = fmEnv.tick(), ne = nEnv.tick() * attackRamp(), se = snapEnv.tick();
                ph[1] = wrap01(ph[1] + f1 * ratio2 / fs); float m = sinCycle(ph[1]);
                ph[0] = wrap01(ph[0] + f1 / fs); float b1 = sinCycle(ph[0] + q[V_FMAMT] * 0.8f * m * fe);
                ph[2] = wrap01(ph[2] + f1 * q[V_FMRATIO] * 1.37f / fs);
                float body = (b1 + 0.55f * m * e2) * a * attackRamp();
                body = lerpf(body, b1 * sinCycle(ph[2]) * a, q[V_RING]);
                float nl = nz[0].tick(q[V_NCOLOR]), nr = lerpf(nl, nz[1].tick(q[V_NCOLOR]), q[V_WIDTH]);
                nl = flt[0].process(nl, ftype); nr = flt[1].process(nr, ftype);
                float sn = q[V_SNAP] * se, bl = 0.5f * sn * blip(), snl = flt[2].process(nz[0].white(), 2) * sn * 1.4f + bl, snr = lerpf(snl, flt[3].process(nz[1].white(), 2) * sn * 1.4f + bl, q[V_WIDTH]);
                float cl = q[V_CLICK] * clickEnv.tick() * 0.8f * blip();
                float xl = body * q[V_BODY] + nl * q[V_NOISE] * ne + snl + cl, xr = body * q[V_BODY] + nr * q[V_NOISE] * ne + snr + cl;
                L += post(shape(xl), 0) * g; R += post(shape(xr), 1) * g; break;
            }
            case E_CHAT: case E_OHAT: {
                float a = amp.tick(), ne = nEnv.tick() * attackRamp(), n = nz[0].tick(q[V_NCOLOR]);
                float fm = 1.f + q[V_FMAMT] * 0.5f * prevBank + q[V_MOD] * 0.05f * n;
                float s[6]; float bank = 0;
                for (int i = 0; i < 6; ++i) { s[i] = sq[i].tick(f0 * ratios[i] * fm / fs); bank += s[i]; }
                bank *= (1.f / 6.f); prevBank = bank;
                float ringv = (s[0] * s[1] + s[2] * s[3] + s[4] * s[5]) * (1.f / 3.f);
                float metal = lerpf(bank, ringv, q[V_RING]);
                float src = metal * q[V_BODY] * a * attackRamp() + n * q[V_NOISE] * ne;
                float bp = flt[0].process(src, 2);
                float y = flt[1].process(bp * 0.75f + src * 0.35f, 1);
                y += q[V_CLICK] * clickEnv.tick() * op[0].hp(nz[1].white()) * 0.5f;
                y = shape(y) * 1.6f; y = dc[0].tick(y);
                L += y * g; R += widen(y) * g; break;
            }
            case E_CLAP: {
                float nlw = nz[0].tick(q[V_NCOLOR]), nrw = lerpf(nlw, nz[1].tick(q[V_NCOLOR]), q[V_WIDTH]);
                if (burstIdx < bursts && (float)t >= burstStart[burstIdx]) { burstEnv.v = 1.f; ++burstIdx; if (burstIdx == bursts && !tailStarted) { tailStarted = true; amp.trigger(0, q[V_DEC] * 0.001f, q[V_SUS], q[V_REL] * 0.001f, fs, -1); } }
                float be = burstEnv.tick(), a = amp.active() ? amp.tick() : 0.f;
                if (!tailStarted) amp.stage = Adsr::Idle;
                float env = q[V_CLICK] * be + q[V_BODY] * a * 0.6f;
                float l = flt[0].process(nlw, 2), r = flt[1].process(nrw, 2);
                l = l * 0.85f + op[0].hp(nlw) * 0.25f; r = r * 0.85f + op[1].hp(nrw) * 0.25f;
                L += post(shape(l * env * q[V_NOISE] * 1.6f), 0) * g; R += post(shape(r * env * q[V_NOISE] * 1.6f), 1) * g;
                if (!tailStarted) quiet = 0; // keep alive between bursts
                break;
            }
            case E_TOM: {
                float f = pitchNow(), a = amp.tick(), e2 = env2.tick(), e3 = env3.tick(), fe = fmEnv.tick(), ne = nEnv.tick();
                ph[3] = wrap01(ph[3] + f * q[V_FMRATIO] / fs); float pm = q[V_FMAMT] * fe * sinCycle(ph[3]);
                ph[0] = wrap01(ph[0] + f / fs); ph[1] = wrap01(ph[1] + f * 1.593f / fs); ph[2] = wrap01(ph[2] + f * 2.135f / fs);
                float tonal = (lerpf(sinCycle(ph[0] + pm), triCycle(ph[0] + pm), q[V_WAVE]) * a + sinCycle(ph[1]) * q[V_HARM] * 0.8f * e2 + sinCycle(ph[2]) * q[V_HARM] * 0.5f * e3) * attackRamp();
                tonal *= 1.f - q[V_RING] + q[V_RING] * sinCycle(ph[3]);
                float stick = flt[0].process(nz[0].tick(q[V_NCOLOR]), 2) * ne;
                float exc = stick + (t == 0 ? 1.f : 0.f);
                float shell = modes[0].tick(exc) * q[V_RES] * 0.5f;
                float x = tonal * q[V_BODY] + stick * q[V_NOISE] + shell + q[V_CLICK] * 0.6f * blip() * clickEnv.tick();
                x = post(shape(x), 0);
                L += x * g; R += x * g; break;
            }
            case E_RIM: {
                float a = amp.tick() * attackRamp(), ne = nEnv.tick(), n = nz[0].white();
                float exc = (t == 0 ? 1.f : 0.f) + n * q[V_NOISE] * ne * 0.3f;
                float x = modes[0].tick(exc) + modes[1].tick(exc);
                x = op[0].hp(x) * 1.4f + q[V_CLICK] * op[1].hp(n) * clickEnv.tick() * 0.6f;
                x = post(shape(x * a), 0);
                L += x * g; R += x * g; break;
            }
            case E_COWBELL: {
                float a = amp.tick() * attackRamp(), pu = punchEnv.tick();
                float s = (sq[0].tick(f0 / fs) + sq[1].tick(f0 * ratio2 / fs)) * 0.5f;
                float x = flt[0].process(s, 2) * (a * (1.f - 0.45f * q[V_PUNCH]) + pu * q[V_PUNCH] * 0.9f) * 1.6f;
                x = post(shape(x), 0);
                L += x * g; R += x * g; break;
            }
            case E_WOOD: {
                float a = amp.tick() * attackRamp(), ce = clickEnv.tick();
                float exc = (t == 0 ? 1.f : 0.f) + q[V_CLICK] * 0.2f * op[0].lp(nz[0].white()) * ce;
                float x = modes[0].tick(exc) + modes[1].tick(exc) + modes[2].tick(exc);
                x = post(shape(x * a * 1.2f), 0);
                L += x * g; R += x * g; break;
            }
            case E_METAL: {
                float a = amp.tick() * attackRamp(), fe = fmEnv.tick(), ne = nEnv.tick();
                ph[1] = wrap01(ph[1] + f0 * q[V_FMRATIO] / fs); ph[0] = wrap01(ph[0] + f0 / fs); ph[2] = wrap01(ph[2] + f0 * ratio2 / fs);
                float fmo = sinCycle(ph[0] + q[V_FMAMT] * 2.5f * fe * sinCycle(ph[1]));
                float ringo = sinCycle(ph[0]) * sinCycle(ph[2]);
                float tonal = lerpf(fmo, ringo, q[V_RING]) * a;
                float exc = t == 0 ? 1.f : 0.f, modal = 0; for (int i = 0; i < 4; ++i) modal += modes[i].tick(exc);
                float x = tonal * q[V_BODY] * (1.f - q[V_MOD] * 0.6f) + modal * q[V_MOD] * 0.6f + nz[0].tick(q[V_NCOLOR]) * q[V_NOISE] * ne * 0.5f;
                x = post(shape(x), 0);
                L += x * g; R += widen(x) * g; break;
            }
            case E_CLICK: {
                float f = pitchNow(), a = amp.tick() * attackRamp(), ne = nEnv.tick();
                ph[0] = wrap01(ph[0] + f / fs);
                float x = q[V_BODY] * sinCycle(ph[0]) * a + q[V_NOISE] * flt[0].process(nz[0].white(), 2) * ne * 1.5f + q[V_CLICK] * (t == 0 ? 1.f : 0.f);
                x = post(shape(x), 0);
                L += x * g; R += x * g; break;
            }
            case E_IMPACT: {
                float f = pitchNow(), a = amp.tick(), a2 = amp2.tick(), ne = nEnv.tick() * attackRamp();
                ph[0] = wrap01(ph[0] + f / fs); ph[1] = wrap01(ph[1] + f * 0.5f / fs);
                float boom = sinCycle(ph[0]) * a * q[V_BODY] * std::min(1.f, (float)t / (fs * 0.002f) + 0.2f);
                float nl = flt[0].process(nz[0].tick(q[V_NCOLOR]), 0) * ne * q[V_NOISE], nr = flt[1].process(nz[1].tick(q[V_NCOLOR]), 0) * ne * q[V_NOISE];
                float click = q[V_CLICK] * blip() * clickEnv.tick();
                float dl = boom + nl + click, dr = boom + lerpf(nl, nr, q[V_WIDTH]) + click;
                float wl = dl, wr = dr; for (int i = 0; i < 4; ++i) { wl = allpass(diff[i], diffLen[i], wl, 0.6f); wr = allpass(diff[4 + i], diffLen[4 + i], wr, 0.6f); }
                float mx = q[V_MOD] * 0.65f;
                float xl = lerpf(dl, wl, mx), xr = lerpf(dr, lerpf(wl, wr, q[V_WIDTH]), mx);
                float sub = sinCycle(ph[1]) * q[V_SUB] * a2 * 0.7f;
                L += (post(shape(xl), 0) + sub) * g; R += (post(shape(xr), 1) + sub) * g; break;
            }
            case E_INDUSTRIAL: {
                float f = pitchNow(), a = amp.tick() * attackRamp(), ne = nEnv.tick();
                float src = sq[0].tick(f / fs) * q[V_BODY] * a * 0.6f + nz[0].tick(q[V_NCOLOR]) * q[V_NOISE] * ne;
                float y = src + q[V_RES] * 0.93f * comb.tap(combLen); comb.push(softClip(y));
                float x = shape(y * 0.8f);
                if (holdLen > 1) { if (holdCount-- <= 0) { held = x; holdCount = holdLen - 1; } x = held; }
                if (bitStep > 0) x = std::round(x / bitStep) * bitStep;
                x = post(x, 0);
                L += x * g; R += widen(x) * g; break;
            }
            case E_EXPERIMENTAL: {
                if (q[V_SWEEP] > 0 && --jumpCount <= 0) { jumpCount = jumpLen; curF = clampf(f0 * std::exp2(rng.bi() * q[V_SWEEP] / 12.f), 20.f, fs * 0.3f); }
                float a = amp.tick() * attackRamp(), ne = nEnv.tick();
                ph[1] = wrap01(ph[1] + curF * q[V_FMRATIO] / fs); ph[0] = wrap01(ph[0] + curF / fs);
                float y = std::sin(kTwoPi * ph[0] + q[V_FMAMT] * 2.2f * prevY + q[V_FMAMT] * 2.f * sinCycle(ph[1])); prevY = y;
                float nb = flt[0].process(nz[0].white(), 2);
                y = lerpf(y, y * nb * 3.f, q[V_RING]);
                gph += gInc; if (gph >= 1) gph -= 1;
                float gate = gph < 0.5f ? 1.f : 1.f - q[V_MOD] * 0.9f;
                float x = (y * q[V_BODY] * a + nz[1].tick(q[V_NCOLOR]) * q[V_NOISE] * ne) * gate + q[V_CLICK] * blip() * clickEnv.tick();
                x = post(shape(x), 0);
                L += x * g; R += widen(x) * g; break;
            }
            default: break;
        }
    }
};

/// Offline render of one hit (used by the waveform display, level-normaliser, generator and tests).
inline std::vector<float> renderHit(const float* chanParams, double sr, int frames, float vel = 1.f, const SampleData* s = nullptr, std::vector<float>* right = nullptr, int64_t gate = -1, uint64_t seed = 12345) {
    int os = sr < 80000 ? 2 : 1;
    auto voice = std::make_unique<DrumVoice>(); voice->prepare((float)(sr * os), (float)sr);
    voice->trigger(chanParams, vel, gate < 0 ? (int64_t)(0.25 * sr * os) : gate * os, seed, s);
    std::vector<float> out((size_t)frames), outR((size_t)frames); std::vector<float> bl(512), br(512);
    Decimator2 dl, dr; int done = 0;
    while (done < frames) {
        int n = std::min(256, frames - done); std::fill(bl.begin(), bl.end(), 0.f); std::fill(br.begin(), br.end(), 0.f);
        voice->render(bl.data(), br.data(), n * os);
        for (int i = 0; i < n; ++i) {
            if (os == 2) { out[(size_t)(done + i)] = dl.process(bl[(size_t)(2 * i)], bl[(size_t)(2 * i + 1)]); outR[(size_t)(done + i)] = dr.process(br[(size_t)(2 * i)], br[(size_t)(2 * i + 1)]); }
            else { out[(size_t)(done + i)] = bl[(size_t)i]; outR[(size_t)(done + i)] = br[(size_t)i]; }
        }
        done += n;
    }
    if (right) *right = std::move(outR);
    return out;
}
} // namespace bk
