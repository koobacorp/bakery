// BAKERY — effects used per voice (2 serial insert slots), on the 2 parallel send buses and the master bus (4 slots).
// Every slot owns all effect processors (memory allocated in prepare); only the selected one runs, so "Off" costs ~nothing.
#pragma once
#include "Params.h"

namespace bk {
struct Compressor {
    float env = 0, grDb = 0;
    void reset() { env = 0; grDb = 0; }
    static float gainReductionDb(float lvlDb, float thr, float ratio, float knee = 6.f) {
        float over = lvlDb - thr, s = 1.f - 1.f / std::max(1.f, ratio);
        if (over <= -knee * 0.5f) return 0.f;
        if (over >= knee * 0.5f) return over * s;
        float x = over + knee * 0.5f; return x * x / (2.f * knee) * s;
    }
    void process(float* L, float* R, int n, float fs, float thr, float ratio, float attMs, float relMs, float makeup) {
        float aa = tauCoef(attMs * 0.001f, fs), ar = tauCoef(relMs * 0.001f, fs), mk = dbToGain(makeup);
        float maxGr = 0;
        for (int i = 0; i < n; ++i) {
            float x = std::max(std::abs(L[i]), std::abs(R[i]));
            env = x > env ? aa * env + (1 - aa) * x : ar * env + (1 - ar) * x; env = flushTiny(env);
            float gr = gainReductionDb(gainToDb(env), thr, ratio); maxGr = std::max(maxGr, gr);
            float gg = dbToGain(-gr) * mk; L[i] *= gg; R[i] *= gg;
        }
        grDb = maxGr;
    }
};

struct TransientShaper {
    float fast = 0, slow = 0;
    void reset() { fast = slow = 0; }
    void process(float* L, float* R, int n, float fs, float attack, float sustain, float speed, float outDb, float clip) {
        float fa = tauCoef(0.0003f, fs), fr = tauCoef((5.f + speed * 40.f) * 0.001f, fs);
        float sa = tauCoef((10.f + speed * 60.f) * 0.001f, fs), sr = tauCoef((100.f + speed * 300.f) * 0.001f, fs), og = dbToGain(outDb);
        for (int i = 0; i < n; ++i) {
            float x = std::max(std::abs(L[i]), std::abs(R[i]));
            fast = x > fast ? fa * fast + (1 - fa) * x : fr * fast + (1 - fr) * x;
            slow = x > slow ? sa * slow + (1 - sa) * x : sr * slow + (1 - sr) * x;
            fast = flushTiny(fast); slow = flushTiny(slow);
            float diff = gainToDb(fast + 1e-7f) - gainToDb(slow + 1e-7f);
            float gdb = attack * 15.f * clampf(diff / 8.f, 0.f, 1.f) + sustain * 15.f * clampf(-diff / 8.f, 0.f, 1.f);
            float gg = dbToGain(gdb) * og;
            float l = L[i] * gg, r = R[i] * gg;
            if (clip > 0.01f) { l = lerpf(l, std::tanh(l), clip); r = lerpf(r, std::tanh(r), clip); }
            L[i] = l; R[i] = r;
        }
    }
};

struct StereoDelay {
    DelayLine dl, dr; OnePole fl, fr; float curL = 0, curR = 0; bool init = false;
    void prepare(float fs, float maxSec) { dl.prepare((int)(fs * maxSec) + 8); dr.prepare((int)(fs * maxSec * 1.4f) + 8); reset(); }
    void reset() { dl.reset(); dr.reset(); fl.reset(); fr.reset(); init = false; }
    void process(float* L, float* R, int n, float fs, float timeMs, float fb, float filt, float spread) {
        float tl = clampf(timeMs * 0.001f * fs, 1.f, (float)dl.capacity()), tr = clampf(tl * (1.f + spread * 0.35f), 1.f, (float)dr.capacity());
        if (!init) { curL = tl; curR = tr; init = true; }
        fl.set(filt, fs); fr.set(filt, fs); float sm = tauCoef(0.05f, fs), cross = spread * 0.5f;
        for (int i = 0; i < n; ++i) {
            curL = tl + (curL - tl) * sm; curR = tr + (curR - tr) * sm;
            float yl = dl.tap(curL), yr = dr.tap(curR);
            float fbl = fl.lp(yl), fbr = fr.lp(yr);
            dl.push(softClip(L[i] + fb * lerpf(fbl, fbr, cross)));
            dr.push(softClip(R[i] + fb * lerpf(fbr, fbl, cross)));
            L[i] = yl; R[i] = yr;
        }
    }
};

struct Reverb { // Freeverb-style: 8 damped combs + 4 allpasses per side
    static constexpr int kComb[8] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
    static constexpr int kAp[4] = {556, 441, 341, 225};
    DelayLine comb[2][8], ap[2][4], pre[2]; float filt[2][8]{}; float scale = 1;
    void prepare(float fs) {
        scale = fs / 44100.f;
        for (int s = 0; s < 2; ++s) { for (int i = 0; i < 8; ++i) comb[s][i].prepare((int)((kComb[i] + 23) * scale * 1.3f) + 8); for (int i = 0; i < 4; ++i) ap[s][i].prepare((int)((kAp[i] + 23) * scale) + 8); pre[s].prepare((int)(fs * 0.11f)); }
        reset();
    }
    void reset() { for (int s = 0; s < 2; ++s) { for (auto& c : comb[s]) c.reset(); for (auto& a : ap[s]) a.reset(); pre[s].reset(); for (auto& f : filt[s]) f = 0; } }
    void process(float* L, float* R, int n, float fs, float size, float decay, float damp, float diffusion, float preMs) {
        float fb = 0.7f + decay * 0.28f, d = damp * 0.6f, g = 0.25f + diffusion * 0.45f, sz = 0.4f + size * 0.8f;
        int pd = std::max(1, (int)(preMs * 0.001f * fs));
        int cl[2][8], al[2][4];
        for (int s = 0; s < 2; ++s) { for (int i = 0; i < 8; ++i) cl[s][i] = std::min(comb[s][i].capacity(), std::max(2, (int)((kComb[i] + s * 23) * scale * sz))); for (int i = 0; i < 4; ++i) al[s][i] = std::min(ap[s][i].capacity(), std::max(2, (int)((kAp[i] + s * 23) * scale))); }
        for (int i = 0; i < n; ++i) {
            float in = (L[i] + R[i]) * 0.5f * 0.03f;
            for (int s = 0; s < 2; ++s) {
                pre[s].push(in); float x = pre[s].tapInt(pd), acc = 0;
                for (int c = 0; c < 8; ++c) { float y = comb[s][c].tapInt(cl[s][c]); filt[s][c] = flushTiny(y * (1 - d) + filt[s][c] * d); comb[s][c].push(x + filt[s][c] * fb); acc += y; }
                for (int a = 0; a < 4; ++a) { float v = ap[s][a].tapInt(al[s][a]); float w = acc + g * v; ap[s][a].push(w); acc = v - g * w; }
                (s == 0 ? L : R)[i] = acc * 3.f;
            }
        }
    }
};

struct PitchShifter {
    DelayLine d[2]; float p = 0;
    void prepare(float fs) { d[0].prepare((int)(fs * 0.25f)); d[1].prepare((int)(fs * 0.25f)); p = 0; }
    void reset() { d[0].reset(); d[1].reset(); p = 0; }
    void process(float* L, float* R, int n, float fs, float semis, float winMs, float cents) {
        float ratio = std::exp2((semis + cents * 0.01f) / 12.f), win = clampf(winMs * 0.001f * fs, 16.f, (float)d[0].capacity() - 4.f), inc = (1.f - ratio) / win;
        for (int i = 0; i < n; ++i) {
            d[0].push(L[i]); d[1].push(R[i]);
            p += inc; p -= std::floor(p); float p2 = wrap01(p + 0.5f);
            float g1 = std::sin(kPi * p), g2 = std::sin(kPi * p2); g1 *= g1; g2 *= g2;
            L[i] = d[0].tap(1.f + p * win) * g1 + d[0].tap(1.f + p2 * win) * g2;
            R[i] = d[1].tap(1.f + p * win) * g1 + d[1].tap(1.f + p2 * win) * g2;
        }
    }
};

class FxSlot {
public:
    void prepare(float sampleRate, float maxDelaySec) {
        fs = sampleRate; delay.prepare(fs, maxDelaySec); reverb.prepare(fs); pitch.prepare(fs);
        wobble[0].prepare((int)(fs * 0.02f)); wobble[1].prepare((int)(fs * 0.02f)); haas.prepare((int)(fs * 0.025f));
        for (int i = 0; i < 2; ++i) { dc[i].prepare(fs); }
        dry.assign(4096, 0.f); dryR.assign(4096, 0.f); reset(); rng.setSeed(77);
    }
    void reset() {
        comp.reset(); trans.reset(); delay.reset(); reverb.reset(); pitch.reset(); wobble[0].reset(); wobble[1].reset(); haas.reset();
        for (auto& s : svf) s.reset(); for (auto& o : op) o.reset(); for (int i = 0; i < 2; ++i) { up[i].reset(); down[i].reset(); dc[i].reset(); hold[i] = 0; }
        holdPh = 0; envF = 0; ringPh = lfoPh = wowPh = flutPh = 0; crackle.v = 0; drop = 1; dropTarget = 1; dropCount = 0; mixRamp.init = false;
    }
    float gainReduction() const { return lastType == FX_COMP ? comp.grDb : 0.f; }
    /// slot: [type, p1..p5, mix]. In-place on L/R (n <= 4096).
    void process(float* L, float* R, int n, const float* slot, double bpm) {
        int type = (int)slot[0];
        if (type != lastType) { reset(); lastType = type; }
        if (type <= FX_OFF || type >= FX_COUNT || n <= 0) return;
        float v[5]; for (int i = 0; i < 5; ++i) v[i] = fxValue(type, i, slot[1 + i]);
        float mix = clampf(slot[6], 0, 1);
        mixRamp.setTarget(mix, n);
        bool needDry = mix < 0.999f || !mixRamp.init || std::abs(mixRamp.inc) > 0;
        if (needDry) { std::copy(L, L + n, dry.begin()); std::copy(R, R + n, dryR.begin()); }
        switch (type) {
            case FX_DIST: dist(L, R, n, v); break;
            case FX_COMP: comp.process(L, R, n, fs, v[0], v[1], v[2], v[3], v[4]); break;
            case FX_FILTER: filter(L, R, n, v); break;
            case FX_TRANS: trans.process(L, R, n, fs, v[0], v[1], v[2], v[3], v[4]); break;
            case FX_DELAY: { float ms = v[0]; int sync = (int)v[4]; if (sync > 0 && bpm > 1) ms = (float)(syncBeats(sync) * 60000.0 / bpm); delay.process(L, R, n, fs, ms, v[1], v[2], v[3]); break; }
            case FX_REVERB: reverb.process(L, R, n, fs, v[0], v[1], v[2], v[3], v[4]); break;
            case FX_CRUSH: crush(L, R, n, v); break;
            case FX_WIDE: wide(L, R, n, v); break;
            case FX_RING: ring(L, R, n, v); break;
            case FX_PITCH: pitch.process(L, R, n, fs, v[0], v[1], v[2]); break;
            case FX_DEGRADE: degrade(L, R, n, v); break;
            default: break;
        }
        for (int i = 0; i < n; ++i) {
            float m = mixRamp.tick();
            float l = needDry ? lerpf(dry[(size_t)i], L[i], m) : L[i], r = needDry ? lerpf(dryR[(size_t)i], R[i], m) : R[i];
            if (!std::isfinite(l)) l = 0; if (!std::isfinite(r)) r = 0;
            L[i] = clampf(l, -8.f, 8.f); R[i] = clampf(r, -8.f, 8.f);
        }
        if (type == FX_DIST || type == FX_DEGRADE) for (int i = 0; i < n; ++i) { L[i] = dc[0].tick(L[i]); R[i] = dc[1].tick(R[i]); }
    }

private:
    float fs = 48000; int lastType = 0; std::vector<float> dry, dryR; Ramp mixRamp; Rng rng;
    Compressor comp; TransientShaper trans; StereoDelay delay; Reverb reverb; PitchShifter pitch;
    Svf svf[2]; OnePole op[6]; Interpolator2 up[2]; Decimator2 down[2]; DcBlocker dc[2]; DelayLine wobble[2], haas;
    float hold[2]{}, holdPh = 0, envF = 0, ringPh = 0, lfoPh = 0, wowPh = 0, flutPh = 0, drop = 1, dropTarget = 1, cutSm = -1; int dropCount = 0; Decay crackle;

    void dist(float* L, float* R, int n, const float* v) {
        float g = dbToGain(v[0]); int mode = (int)v[1]; float bias = v[3] * 0.5f, out = dbToGain(v[4]);
        int sm = mode == 0 ? 3 : (mode == 1 ? 4 : (mode == 2 ? 1 : (mode == 3 ? 2 : 0)));
        op[0].set(v[2], fs); op[1].set(v[2], fs);
        float* ch[2] = {L, R};
        for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) {
            float a, b; up[c].process(ch[c][i], a, b);
            auto sh = [&](float x) { float y = x * g + bias; if (mode == 4) { y = clampf(y, -1.5f, 1.5f); return (1.5f * y - 0.5f * y * y * y) * 0.95f; } return shapeMode(y, sm == 3 ? 0 : sm); };
            float y = down[c].process(sh(a), sh(b));
            ch[c][i] = op[c].lp(y) * out;
        }
    }
    void filter(float* L, float* R, int n, const float* v) {
        int mode = (int)v[2]; float drv = 1.f + v[3] * 8.f, norm = v[3] > 0.01f ? 1.f / std::tanh(drv) : 1.f;
        float ea = tauCoef(0.002f, fs), er = tauCoef(0.15f, fs);
        float target = std::log2(v[0]); if (cutSm < 0) cutSm = target; 
        for (int i = 0; i < n; i += 16) {
            int m = std::min(16, n - i);
            for (int k = 0; k < m; ++k) { float x = std::max(std::abs(L[i + k]), std::abs(R[i + k])); envF = x > envF ? ea * envF + (1 - ea) * x : er * envF + (1 - er) * x; }
            cutSm += (target - cutSm) * 0.08f;
            float hz = std::exp2(cutSm + v[4] * 4.f * clampf(envF * 2.f, 0.f, 1.f));
            svf[0].set(hz, v[1], fs); svf[1].set(hz, v[1], fs);
            for (int k = 0; k < m; ++k) {
                float l = L[i + k], r = R[i + k];
                if (v[3] > 0.01f) { l = std::tanh(l * drv) * norm; r = std::tanh(r * drv) * norm; }
                L[i + k] = svf[0].process(l, mode); R[i + k] = svf[1].process(r, mode);
            }
        }
    }
    void crush(float* L, float* R, int n, const float* v) {
        float step = 2.f / std::exp2(v[0]), inc = std::min(1.f, v[1] / fs); float sm = expMap(1.f - v[2], 2000.f, 20000.f);
        op[2].set(sm, fs); op[3].set(sm, fs);
        for (int i = 0; i < n; ++i) {
            holdPh += inc; if (holdPh >= 1.f || inc >= 1.f) { holdPh -= std::floor(holdPh); hold[0] = L[i]; hold[1] = R[i]; }
            float l = std::round(hold[0] / step) * step, r = std::round(hold[1] / step) * step;
            if (v[2] > 0.01f) { l = op[2].lp(l); r = op[3].lp(r); }
            L[i] = l; R[i] = r;
        }
    }
    void wide(float* L, float* R, int n, const float* v) {
        float hz = v[2]; if (hz > 0) op[4].set(hz, fs); int hd = (int)(v[1] * 0.001f * fs);
        for (int i = 0; i < n; ++i) {
            float m = (L[i] + R[i]) * 0.5f, s = (L[i] - R[i]) * 0.5f;
            if (hz > 0) s = op[4].hp(s);
            s *= v[0];
            float l = m + s, r = m - s;
            haas.push(r); if (hd > 0) r = haas.tapInt(hd);
            L[i] = l; R[i] = r;
        }
    }
    void ring(float* L, float* R, int n, const float* v) {
        for (int i = 0; i < n; ++i) {
            lfoPh = wrap01(lfoPh + v[2] / fs);
            float f = v[0] * (1.f + v[3] * 0.9f * sinCycle(lfoPh));
            ringPh = wrap01(ringPh + f / fs);
            float s = sinCycle(ringPh), c = lerpf(s, std::tanh(s * 8.f), v[1]);
            L[i] *= c; R[i] *= c;
        }
    }
    void degrade(float* L, float* R, int n, const float* v) {
        op[5].set(v[3], fs); OnePole& lpR = op[4]; lpR.set(v[3], fs);
        for (int i = 0; i < n; ++i) {
            wowPh = wrap01(wowPh + 0.6f / fs); flutPh = wrap01(flutPh + 7.f / fs);
            float dms = 4.f + v[2] * 3.f * (0.7f * sinCycle(wowPh) + 0.3f * sinCycle(flutPh));
            wobble[0].push(L[i]); wobble[1].push(R[i]);
            float l = v[2] > 0.01f ? wobble[0].tap(dms * 0.001f * fs) : L[i], r = v[2] > 0.01f ? wobble[1].tap(dms * 0.001f * fs) : R[i];
            if (rng.uni() < v[1] * 0.0006f * 48000.f / fs) crackle.trigger(0.002f, fs, rng.bi() * 0.4f);
            float cr = crackle.tick(), h = rng.bi() * 0.04f * v[0] * v[0];
            if (--dropCount <= 0) { dropTarget = (rng.uni() < v[4] * 0.4f) ? 0.1f : 1.f; dropCount = (int)(fs * rng.range(0.03f, 0.12f)); }
            drop += (dropTarget - drop) * 0.004f;
            L[i] = op[5].lp(l * drop + h + cr); R[i] = lpR.lp(r * drop + h + cr);
        }
    }
};

/// Zero-latency peak limiter: instant attack (output never exceeds the ceiling), smooth release.
struct Limiter {
    float g = 1, rel = 0.999f;
    void prepare(float fs) { rel = tauCoef(0.08f, fs); g = 1; }
    float reduction() const { return gainToDb(g); }
    inline void process(float& l, float& r, float ceiling) {
        float pk = std::max(std::abs(l), std::abs(r)), want = pk * g > ceiling ? ceiling / pk : 1.f;
        g = want < g ? want : std::min(want, 1.f - (1.f - g) * rel);
        l *= g; r *= g;
        l = clampf(l, -ceiling, ceiling); r = clampf(r, -ceiling, ceiling);
    }
};
} // namespace bk
