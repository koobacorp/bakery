// BAKERY engine tests: pure C++, no JUCE. Every number checked here is measured from rendered audio.
#include "TestUtil.h"
#include "Key.h"
using namespace bk;
static const double kRates[] = {44100, 48000, 88200, 96000, 192000};
static float V[kNumParams];
static void fresh() { loadDefaults(V); }

static void testRegistry() {
    const auto& r = registry(); CHECK((int)r.size() == kNumParams);
    std::set<std::string> ids; for (auto& p : r) { ids.insert(p.id); CHECKM(p.def >= p.lo && p.def <= p.hi, "%s", p.id.c_str()); CHECK(!p.name.empty()); if (!p.choices.empty()) CHECK((int)p.choices.size() == (int)(p.hi - p.lo) + 1); }
    CHECK((int)ids.size() == kNumParams);
    for (int i = 0; i < kNumParams; i += 7) CHECK(paramIndex(r[(size_t)i].id) == i);
    CHECK(paramIndex("c1_freq") == cp(0, V_FREQ)); CHECK(paramIndex("g_swing") == gp(G_SWING)); CHECK(paramIndex("m1type") == mp(0, 0)); CHECK(paramIndex("sbmix") == sp(1, 6)); CHECK(paramIndex("nope") == -1);
    fresh(); for (int c = 0; c < kChannels; ++c) { CHECK((int)V[cp(c, V_ENGINE)] == defaultEngine(c)); CHECK((int)V[cp(c, V_NOTE)] == defaultNote(c)); }
    CHECK(V[cp(2, V_CHOKE)] == 1 && V[cp(3, V_CHOKE)] == 1 && V[cp(0, V_CHOKE)] == 0);
    for (int t = 0; t < FX_COUNT; ++t) for (int i = 0; i < 5; ++i) for (float x : {0.f, 0.37f, 1.f}) { CHECK(std::isfinite(fxValue(t, i, x))); CHECK(!fxText(t, i, x).empty()); }
}

static void testEnginesAllRates() {
    int checked = 0;
    for (double sr : kRates) for (int e = 0; e < E_COUNT; ++e) {
        fresh(); applyEngineDefaults(V, 0, e); V[cp(0, V_LEVEL)] = 0;
        int n = (int)(sr * 3.0); std::vector<float> R; auto L = renderHit(&V[cp(0, 0)], sr, n, 1.f, nullptr, &R);
        float pk = tu::peak(L); double early = tu::rms(L, 0, (size_t)(sr * 0.05)), late = tu::rms(L, (size_t)(sr * 2.7), (size_t)n);
        CHECKM(pk > 0.02f, "%s @%g silent pk=%g", engineName(e), sr, pk);
        CHECKM(pk < 4.f, "%s @%g pk=%g", engineName(e), sr, pk);
        CHECKM(tu::finite(L) && tu::finite(R), "%s @%g non-finite or denormal", engineName(e), sr);
        CHECKM(late < early * 0.05 + 1e-6, "%s @%g does not decay (early %g late %g)", engineName(e), sr, early, late);
        ++checked;
    }
    CHECK(checked == 5 * E_COUNT);
    // engines really differ: pairwise spectral centroids are not all equal
    fresh(); std::vector<double> cents;
    for (int e = 0; e < E_COUNT; ++e) { applyEngineDefaults(V, 0, e); auto h = renderHit(&V[cp(0, 0)], 48000, 24000); cents.push_back(tu::centroid(h, 48000, 0, 12000)); }
    std::sort(cents.begin(), cents.end()); int distinct = 1; for (size_t i = 1; i < cents.size(); ++i) if (cents[i] > cents[i - 1] * 1.05) ++distinct;
    CHECKM(distinct >= 9, "only %d distinct spectral centroids across %d engines", distinct, E_COUNT);
}

static void testKickPitch() {
    for (double sr : kRates) {
        fresh(); tu::pureKick(V, 0); V[cp(0, V_FREQ)] = 50; V[cp(0, V_SWEEP)] = 24; V[cp(0, V_SWEEPT)] = 30; V[cp(0, V_DEC)] = 3000;
        auto h = renderHit(&V[cp(0, 0)], sr, (int)(sr * 0.8));
        // predicted f(t) = f0 * 2^(sweep/12 * e^(-t/tau)); check average over windows
        for (double t0 : {0.010, 0.040, 0.080}) {
            double t1 = t0 + 0.025; if (t0 >= 0.08) t1 = t0 + 0.05;
            double meas = tu::zcFreq(h, sr, (size_t)(t0 * sr), (size_t)(t1 * sr));
            double pred = 0; int k = 0; for (double t = t0; t < t1; t += 0.0005, ++k) pred += 50.0 * std::pow(2.0, 2.0 * std::exp(-t / 0.030)); pred /= k;
            CHECKM(std::abs(meas / pred - 1) < 0.06, "kick sweep @%g t=%.3f meas %.1f pred %.1f", sr, t0, meas, pred);
        }
        double fin = tu::zcFreq(h, sr, (size_t)(0.4 * sr), (size_t)(0.8 * sr));
        CHECKM(std::abs(fin - 50) < 1.0, "kick fundamental @%g = %.2f", sr, fin);
        V[cp(0, V_TUNE)] = 12; auto h2 = renderHit(&V[cp(0, 0)], sr, (int)(sr * 0.8)); double f2 = tu::zcFreq(h2, sr, (size_t)(0.4 * sr), (size_t)(0.8 * sr));
        CHECKM(std::abs(f2 - 100) < 2.0, "kick tune +12 @%g = %.2f", sr, f2);
    }
    // decay time: Decay = T60. With sweep off, level at T/2 should be -30 dB
    for (double sr : {44100.0, 96000.0}) {
        fresh(); tu::pureKick(V, 0); V[cp(0, V_SWEEP)] = 0; V[cp(0, V_DEC)] = 400; V[cp(0, V_FREQ)] = 60;
        auto h = renderHit(&V[cp(0, 0)], sr, (int)sr);
        double a0 = tu::peak(h, (size_t)(0.005 * sr), (size_t)(0.03 * sr)), a1 = tu::peak(h, (size_t)(0.19 * sr), (size_t)(0.21 * sr));
        double db = 20 * std::log10(a1 / a0 + 1e-12), expected = -60.0 * (0.2 - 0.0175) / 0.4;
        CHECKM(std::abs(db - expected) < 3.0, "kick decay @%g: %.1f dB, expected %.1f", sr, db, expected);
    }
    // FM and AM add sidebands to the kick
    fresh(); tu::pureKick(V, 0); V[cp(0, V_SWEEP)] = 0; V[cp(0, V_FREQ)] = 80; V[cp(0, V_DEC)] = 2000; V[cp(0, V_FMRATIO)] = 3;
    auto plain = renderHit(&V[cp(0, 0)], 48000, 24000); V[cp(0, V_FMAMT)] = 0.8f; auto fm = renderHit(&V[cp(0, 0)], 48000, 24000);
    CHECKM(tu::tone(fm, 320, 48000, 2400, 12000) > 10 * tu::tone(plain, 320, 48000, 2400, 12000), "kick FM sideband f+fm missing");
    V[cp(0, V_FMAMT)] = 0; V[cp(0, V_RING)] = 1; auto am = renderHit(&V[cp(0, 0)], 48000, 24000);
    CHECK(tu::tone(am, 160, 48000, 2400, 12000) > 0.05 && tu::tone(am, 320, 48000, 2400, 12000) > 0.05 && tu::tone(am, 80, 48000, 2400, 12000) < 0.02);
    // saturation/distortion add harmonics, Sub stays clean
    fresh(); tu::pureKick(V, 0); V[cp(0, V_SWEEP)] = 0; V[cp(0, V_FREQ)] = 60; V[cp(0, V_DEC)] = 2000;
    auto clean = renderHit(&V[cp(0, 0)], 48000, 24000); V[cp(0, V_DRIVE)] = 0.7f; V[cp(0, V_DIST)] = 0.6f; auto dirty = renderHit(&V[cp(0, 0)], 48000, 24000);
    CHECK(tu::tone(dirty, 180, 48000, 2400, 12000) > 20 * tu::tone(clean, 180, 48000, 2400, 12000) + 0.01);
    fresh(); tu::pureKick(V, 0); V[cp(0, V_BODY)] = 0; V[cp(0, V_SUB)] = 1; V[cp(0, V_DRIVE)] = 1; V[cp(0, V_DIST)] = 1; V[cp(0, V_SWEEP)] = 0; V[cp(0, V_DEC)] = 2000;
    auto sub = renderHit(&V[cp(0, 0)], 48000, 24000); CHECK(tu::tone(sub, 50, 48000, 2400, 12000) > 0.1 && tu::tone(sub, 150, 48000, 2400, 12000) < 0.01);
    // click adds high-frequency transient
    fresh(); tu::pureKick(V, 0); auto nc = renderHit(&V[cp(0, 0)], 48000, 4800); V[cp(0, V_CLICK)] = 1; auto wc = renderHit(&V[cp(0, 0)], 48000, 4800);
    auto hf = [](const std::vector<float>& x) { double e = 0; for (size_t i = 1; i < 480; ++i) e += (x[i] - x[i - 1]) * (x[i] - x[i - 1]); return e; }; CHECK(hf(wc) > 4 * hf(nc));
}

static void testSnareHatNoise() {
    fresh(); applyEngineDefaults(V, 0, E_SNARE); V[cp(0, V_BODY)] = 0; V[cp(0, V_SNAP)] = 0; V[cp(0, V_FTYPE)] = 1; V[cp(0, V_CUT)] = 1500; V[cp(0, V_LEVEL)] = 0;
    auto sn = renderHit(&V[cp(0, 0)], 48000, 9600); double cSn = tu::centroid(sn, 48000, 0, 4800);
    CHECKM(cSn > 2500, "snare noise centroid %.0f", cSn);
    V[cp(0, V_NCOLOR)] = 1; auto dark = renderHit(&V[cp(0, 0)], 48000, 9600); CHECK(tu::centroid(dark, 48000, 0, 4800) < cSn * 0.8);
    // body vs noise blend are independent
    fresh(); applyEngineDefaults(V, 0, E_SNARE); V[cp(0, V_NOISE)] = 0; V[cp(0, V_SNAP)] = 0; auto body = renderHit(&V[cp(0, 0)], 48000, 9600);
    CHECK(tu::tone(body, 185, 48000, 480, 4800) > 3 * tu::tone(sn, 185, 48000, 480, 4800));
    // width: decorrelated noise between L and R
    fresh(); applyEngineDefaults(V, 0, E_SNARE); V[cp(0, V_WIDTH)] = 1; std::vector<float> R; auto L = renderHit(&V[cp(0, 0)], 48000, 4800, 1, nullptr, &R);
    double diff = 0; for (size_t i = 0; i < L.size(); ++i) diff += std::abs(L[i] - R[i]); CHECK(diff > 1.0);
    V[cp(0, V_WIDTH)] = 0; L = renderHit(&V[cp(0, 0)], 48000, 4800, 1, nullptr, &R); diff = 0; for (size_t i = 0; i < L.size(); ++i) diff += std::abs(L[i] - R[i]); CHECK(diff < 1e-3);
    // hats: bright, 808 metal ratios present when filters are opened
    fresh(); applyEngineDefaults(V, 0, E_CHAT); auto ch = renderHit(&V[cp(0, 0)], 48000, 4800); double cH = tu::centroid(ch, 48000, 0, 2400);
    CHECKM(cH > 6000, "closed hat centroid %.0f", cH);
    fresh(); applyEngineDefaults(V, 0, E_OHAT); V[cp(0, V_NOISE)] = 0; V[cp(0, V_FREQ)] = 205.3f; V[cp(0, V_HARM)] = 0.2f; V[cp(0, V_CUT)] = 20; V[cp(0, V_RES)] = 0; V[cp(0, V_TONE)] = -1;
    auto bank = renderHit(&V[cp(0, 0)], 48000, 24000);
    for (double f : {205.3, 304.4, 369.6, 522.7, 540.0, 800.0}) CHECKM(tu::tone(bank, f, 48000, 0, 19200) > 5 * tu::tone(bank, 450, 48000, 0, 19200), "808 hat partial %.1f missing", f);
    // open hat decays slower than closed
    fresh(); applyEngineDefaults(V, 0, E_CHAT); auto c2 = renderHit(&V[cp(0, 0)], 48000, 48000); applyEngineDefaults(V, 0, E_OHAT); auto o2 = renderHit(&V[cp(0, 0)], 48000, 48000);
    CHECK(tu::rms(o2, 9600, 19200) > 10 * tu::rms(c2, 9600, 19200));
    // metallic character changes the spectrum
    fresh(); applyEngineDefaults(V, 0, E_CHAT); V[cp(0, V_HARM)] = 0; auto h0 = renderHit(&V[cp(0, 0)], 48000, 4800); V[cp(0, V_HARM)] = 1; auto h1 = renderHit(&V[cp(0, 0)], 48000, 4800);
    double d = 0; for (size_t i = 0; i < h0.size(); ++i) d += std::abs(h0[i] - h1[i]); CHECK(d > 1.0);
}

static void testPercussion() {
    // cowbell: two squares at 540 / 800 Hz
    fresh(); applyEngineDefaults(V, 0, E_COWBELL); V[cp(0, V_CUT)] = 700; V[cp(0, V_RES)] = 0;
    auto cb = renderHit(&V[cp(0, 0)], 48000, 24000);
    double e540 = tu::tone(cb, 540, 48000, 0, 9600), e800 = tu::tone(cb, 800, 48000, 0, 9600), e670 = tu::tone(cb, 670, 48000, 0, 9600);
    CHECKM(e540 > 10 * e670 && e800 > 10 * e670, "cowbell partials 540 %.4f 800 %.4f off %.4f", e540, e800, e670);
    // metallic FM: sidebands at f0 +- n*fm
    fresh(); applyEngineDefaults(V, 0, E_METAL); V[cp(0, V_FREQ)] = 400; V[cp(0, V_FMRATIO)] = 3.5f; V[cp(0, V_RING)] = 0; V[cp(0, V_MOD)] = 0; V[cp(0, V_FMAMT)] = 0;
    auto pure = renderHit(&V[cp(0, 0)], 48000, 24000); V[cp(0, V_FMAMT)] = 0.6f; auto fm = renderHit(&V[cp(0, 0)], 48000, 24000);
    CHECKM(tu::tone(fm, 1800, 48000, 0, 9600) > 20 * tu::tone(pure, 1800, 48000, 0, 9600) + 0.002 && tu::tone(fm, 1000, 48000, 0, 9600) > 20 * tu::tone(pure, 1000, 48000, 0, 9600) + 0.002, "FM sidebands missing");
    // ring: sum/difference present, carrier suppressed
    V[cp(0, V_FMAMT)] = 0; V[cp(0, V_RING)] = 1; V[cp(0, V_HARM)] = 0.5f; auto rm = renderHit(&V[cp(0, 0)], 48000, 24000); double r2 = 1.13 + 0.5 * 2.7;
    CHECKM(tu::tone(rm, 400 * (1 + r2), 48000, 0, 9600) > 0.05 && tu::tone(rm, 400 * (r2 - 1), 48000, 0, 9600) > 0.05 && tu::tone(rm, 400, 48000, 0, 9600) < 0.01, "ring products wrong");
    // tom: membrane modes 1, 1.593, 2.135
    fresh(); applyEngineDefaults(V, 0, E_TOM); V[cp(0, V_SWEEP)] = 0; V[cp(0, V_HARM)] = 1; V[cp(0, V_NOISE)] = 0; V[cp(0, V_RES)] = 0; V[cp(0, V_CLICK)] = 0; V[cp(0, V_FREQ)] = 120;
    auto tom = renderHit(&V[cp(0, 0)], 48000, 24000);
    for (double r : {1.0, 1.593, 2.135}) CHECKM(tu::tone(tom, 120 * r, 48000, 0, 4800) > 5 * tu::tone(tom, 120 * 1.3, 48000, 0, 4800), "tom mode %.3f", r);
    // rim and woodblock resonators ring at their set frequencies
    fresh(); applyEngineDefaults(V, 0, E_RIM); V[cp(0, V_DRIVE)] = 0; V[cp(0, V_NOISE)] = 0; V[cp(0, V_CLICK)] = 0; auto rim = renderHit(&V[cp(0, 0)], 48000, 4800);
    CHECK(tu::tone(rim, 455, 48000, 0, 2400) > 5 * tu::tone(rim, 700, 48000, 0, 2400) && tu::tone(rim, 455 * 3.665, 48000, 0, 2400) > 5 * tu::tone(rim, 1200, 48000, 0, 2400));
    fresh(); applyEngineDefaults(V, 0, E_WOOD); V[cp(0, V_CLICK)] = 0; auto wood = renderHit(&V[cp(0, 0)], 48000, 4800);
    CHECK(tu::tone(wood, 1250, 48000, 0, 2400) > 5 * tu::tone(wood, 1700, 48000, 0, 2400));
    // clap: distinct bursts before the tail (envelope has several peaks in the first 40 ms)
    fresh(); applyEngineDefaults(V, 0, E_CLAP); V[cp(0, V_MOD)] = 0; V[cp(0, V_HARM)] = 0.4f; V[cp(0, V_SWEEPT)] = 10;
    auto clap = renderHit(&V[cp(0, 0)], 48000, 9600); int peaks = 0; double prev = 0; bool rising = false;
    for (int w = 0; w < 50; ++w) { double e = tu::rms(clap, (size_t)(w * 48), (size_t)(w * 48 + 48)); if (e > prev * 1.6 && e > 0.02) { if (!rising) ++peaks; rising = true; } else if (e < prev) rising = false; prev = e; }
    CHECKM(peaks >= 3, "clap bursts %d", peaks);
    // industrial comb resonance, experimental randomness, click shortness, impact length
    fresh(); applyEngineDefaults(V, 0, E_CLICK); auto ck = renderHit(&V[cp(0, 0)], 48000, 48000); CHECK(tu::rms(ck, 2400, 48000) < 0.001);
    fresh(); applyEngineDefaults(V, 0, E_IMPACT); auto im = renderHit(&V[cp(0, 0)], 48000, 96000); CHECK(tu::rms(im, 48000, 72000) > 0.005);
    fresh(); applyEngineDefaults(V, 0, E_INDUSTRIAL); auto in1 = renderHit(&V[cp(0, 0)], 48000, 24000); V[cp(0, V_RES)] = 0; auto in0 = renderHit(&V[cp(0, 0)], 48000, 24000);
    double dd = 0; for (size_t i = 0; i < in1.size(); ++i) dd += std::abs(in1[i] - in0[i]); CHECK(dd > 5);
    fresh(); applyEngineDefaults(V, 0, E_EXPERIMENTAL); V[cp(0, V_SWEEP)] = 24; V[cp(0, V_SWEEPT)] = 20; auto ex = renderHit(&V[cp(0, 0)], 48000, 24000);
    CHECK(std::abs(tu::zcFreq(ex, 48000, 0, 960) - tu::zcFreq(ex, 48000, 9600, 10560)) > 1 || tu::centroid(ex, 48000, 0, 4800) != tu::centroid(ex, 48000, 9600, 14400));
}

static void testMachineChokeMidiOutputs() {
    // choke: OH on aux out, CH on main
    for (bool chokeOn : {true, false}) {
        auto m = tu::machine(48000); tu::clearAll(*m); m->params[cp(3, M_OUT)] = 4; if (!chokeOn) m->params[cp(3, V_CHOKE)] = 0;
        auto r = tu::render(*m, 48000, 1.0, 120, 256, false, 12, 0, [&](int64_t pos, std::vector<MidiEvent>& ev) { if (pos == 0) ev.push_back({0, 0x99, 46, 120}); if (pos == 256 * 40) ev.push_back({0, 0x99, 42, 120}); });
        auto& oh = r.aux[6]; int64_t t = 256 * 40;
        double before = tu::rms(oh, (size_t)(t - 2400), (size_t)t), after = tu::rms(oh, (size_t)(t + 960), (size_t)(t + 3360));
        if (chokeOn) CHECKM(after < before * 0.001, "choke failed %g -> %g", before, after); else CHECKM(after > before * 0.2, "no-choke still choked %g -> %g", before, after);
        CHECK(tu::rms(r.L, 0, (size_t)t) < 1e-6); // OH not on main when routed to out 4
        CHECK(tu::rms(r.L, (size_t)t, (size_t)(t + 2400)) > 1e-3); // CH on main
    }
    // MIDI note map, channel filter, velocity sensitivity
    {
        auto m = tu::machine(48000); std::vector<std::pair<int64_t, int>> log; m->trigLog = &log;
        tu::render(*m, 48000, 0.2, 120, 256, false, 0, 0, [&](int64_t pos, std::vector<MidiEvent>& ev) { if (pos == 0) { ev.push_back({10, 0x99, 38, 100}); ev.push_back({20, 0x90, 36, 100}); ev.push_back({30, 0x90, 99, 100}); } });
        CHECK(log.size() == 2); if (log.size() == 2) { CHECK(log[0].second == 1 && log[0].first == 10); CHECK(log[1].second == 0 && log[1].first == 20); }
        log.clear(); m->params[gp(G_MIDICH)] = 10; m->params[cp(5, V_NOTE)] = 60;
        tu::render(*m, 48000, 0.1, 120, 256, false, 0, 0, [&](int64_t pos, std::vector<MidiEvent>& ev) { if (pos == 0) { ev.push_back({5, 0x90, 38, 100}); ev.push_back({7, 0x99, 60, 100}); } });
        CHECK(log.size() == 1 && log[0].second == 5);
    }
    for (float vs : {1.f, 0.f}) {
        double pk[2]; int i = 0;
        for (int vel : {127, 32}) { auto m = tu::machine(48000); m->params[cp(0, V_VSENS)] = vs; m->params[mp(0, 0)] = 0;
            auto r = tu::render(*m, 48000, 0.3, 120, 256, false, 0, 0, [&](int64_t pos, std::vector<MidiEvent>& ev) { if (pos == 0) ev.push_back({0, 0x99, 36, (uint8_t)vel}); }); pk[i++] = tu::peak(r.L); }
        if (vs > 0) CHECKM(std::abs(pk[1] / pk[0] - 32.0 / 127) < 0.05, "velocity ratio %.3f", pk[1] / pk[0]); else CHECK(std::abs(pk[1] / pk[0] - 1) < 0.01);
    }
    // mute / solo / phase / pan / volume
    {
        auto run = [](std::function<void(DrumMachine&)> f) { auto m = tu::machine(48000); m->params[mp(0, 0)] = 0; f(*m);
            return tu::render(*m, 48000, 0.25, 120, 256, false, 0, 0, [&](int64_t pos, std::vector<MidiEvent>& ev) { if (pos == 0) { ev.push_back({0, 0x99, 36, 127}); ev.push_back({0, 0x99, 38, 127}); } }); };
        auto both = run([](DrumMachine&) {}), mute = run([](DrumMachine& m) { m.params[cp(0, M_MUTE)] = 1; m.params[cp(1, M_MUTE)] = 1; }), solo = run([](DrumMachine& m) { m.params[cp(1, M_SOLO)] = 1; });
        auto kickOnly = run([](DrumMachine& m) { m.params[cp(1, M_MUTE)] = 1; });
        CHECK(tu::peak(mute.L) == 0.f); CHECK(tu::rms(solo.L) > 0 && tu::rms(both.L) > tu::rms(solo.L));
        auto ph = run([](DrumMachine& m) { m.params[cp(1, M_MUTE)] = 1; m.params[cp(0, M_PHASE)] = 1; });
        double s = 0; for (size_t i = 0; i < ph.L.size(); ++i) s += std::abs(ph.L[i] + kickOnly.L[i]); CHECK(s < 1e-3);
        auto pan = run([](DrumMachine& m) { m.params[cp(1, M_MUTE)] = 1; m.params[cp(0, M_PAN)] = -1; }); CHECK(tu::peak(pan.R) < 1e-6 && tu::peak(pan.L) > 0.1);
        auto quiet = run([](DrumMachine& m) { m.params[cp(1, M_MUTE)] = 1; m.params[cp(0, M_VOL)] = -12; }); CHECK(std::abs(20 * std::log10(tu::peak(quiet.L) / tu::peak(kickOnly.L)) + 12) < 0.5);
    }
    // every channel can go to its own output; aux missing -> falls back to main
    {
        auto m = tu::machine(48000); for (int c = 0; c < kChannels; ++c) m->params[cp(c, M_OUT)] = (float)(c + 1);
        auto r = tu::render(*m, 48000, 0.3, 120, 256, false, 12, 0, [&](int64_t pos, std::vector<MidiEvent>& ev) { if (pos == 0) for (int c = 0; c < kChannels; ++c) ev.push_back({0, 0x99, (uint8_t)defaultNote(c), 120}); });
        for (int c = 0; c < kChannels; ++c) CHECKM(tu::rms(r.aux[(size_t)(2 * c)]) > 1e-4, "aux %d silent", c + 2);
        CHECK(tu::rms(r.L) < 1e-3); // only send returns could reach main (none set)
        auto m2 = tu::machine(48000); for (int c = 0; c < kChannels; ++c) m2->params[cp(c, M_OUT)] = (float)(c + 1);
        auto r2 = tu::render(*m2, 48000, 0.3, 120, 256, false, 0, 0, [&](int64_t pos, std::vector<MidiEvent>& ev) { if (pos == 0) ev.push_back({0, 0x99, 36, 120}); });
        CHECK(tu::rms(r2.L) > 1e-3);
    }
    // sends: channel send A reaches the reverb return on main
    { auto m = tu::machine(48000); m->params[cp(1, M_SENDA)] = 1; m->params[cp(1, M_VOL)] = -60;
      auto r = tu::render(*m, 48000, 1.0, 120, 256, false, 0, 0, [&](int64_t pos, std::vector<MidiEvent>& ev) { if (pos == 0) ev.push_back({0, 0x99, 38, 120}); });
      CHECK(tu::rms(r.L) < 1e-6); m->params[cp(1, M_VOL)] = 0; m->params[sp(0, 6)] = 1;
      auto r2 = tu::render(*m, 48000, 1.5, 120, 256, false, 0, 0, [&](int64_t pos, std::vector<MidiEvent>& ev) { if (pos == 0) ev.push_back({0, 0x99, 38, 120}); });
      CHECK(tu::rms(r2.L, 24000, 36000) > 1e-4); }
}

static void testSequencer() {
    for (double sr : {44100.0, 48000.0, 96000.0}) for (double bpm : {90.0, 120.0, 140.0, 174.0}) for (double swing : {50.0, 62.0}) {
        auto m = tu::machine(sr); tu::clearAll(*m); std::vector<std::pair<int64_t, int>> log; m->trigLog = &log; m->params[gp(G_SWING)] = (float)swing;
        for (int s = 0; s < 16; ++s) { Step st; st.on = 1; m->bank.set(0, 0, s, st); }
        tu::render(*m, sr, 4.0, bpm, 333);
        double stepS = 0.25 * 60.0 / bpm * sr, sw = swingOffset(swing); int bad = 0; size_t k = 0;
        for (auto& [t, ch] : log) { double exp = k * stepS + ((k & 1) ? sw * stepS : 0); if (std::abs((double)t - exp) > 1.0) ++bad; ++k; }
        size_t expect = (size_t)std::floor((4.0 * sr - 1) / stepS) + 1; if ((expect - 1) % 2 == 1 && (expect - 1) * stepS + sw * stepS >= 4.0 * sr) --expect;
        CHECKM(bad == 0 && std::abs((long)log.size() - (long)expect) <= 1, "seq timing sr %g bpm %g swing %g: %d bad of %zu (expected %zu)", sr, bpm, swing, bad, log.size(), expect);
    }
    // rates: 1/8, 1/16T, 1/32
    for (int rate : {0, 2, 3}) {
        auto m = tu::machine(48000); tu::clearAll(*m); std::vector<std::pair<int64_t, int>> log; m->trigLog = &log; m->params[gp(G_RATE)] = (float)rate;
        for (int s = 0; s < 16; ++s) { Step st; st.on = 1; m->bank.set(0, 0, s, st); }
        tu::render(*m, 48000, 2.0, 120, 512); double stepS = stepBeats(rate) * 0.5 * 48000; bool ok = log.size() > 3;
        for (size_t i = 0; i < log.size(); ++i) ok &= std::abs((double)log[i].first - i * stepS) <= 1.0;
        CHECKM(ok, "rate %d", rate);
    }
    // probability statistics
    { auto m = tu::machine(48000); tu::clearAll(*m); std::vector<std::pair<int64_t, int>> log; m->trigLog = &log;
      for (int s = 0; s < 16; ++s) { Step st; st.on = 1; st.prob = 50; m->bank.set(0, 0, s, st); st.prob = 25; m->bank.set(0, 1, s, st); }
      tu::render(*m, 48000, 120.0, 240, 1024); int n0 = 0, n1 = 0; for (auto& e : log) (e.second == 0 ? n0 : n1)++;
      double steps = 120.0 * 240 / 60 * 4; CHECKM(std::abs(n0 / steps - 0.5) < 0.04 && std::abs(n1 / steps - 0.25) < 0.04, "prob %.3f %.3f", n0 / steps, n1 / steps); }
    // ratchets: 3 evenly spaced hits per step, also across block boundaries
    { auto m = tu::machine(48000); tu::clearAll(*m); std::vector<std::pair<int64_t, int>> log; m->trigLog = &log;
      Step st; st.on = 1; st.ratchet = 3; m->bank.set(0, 0, 0, st); m->bank.set(0, 0, 8, st);
      tu::render(*m, 48000, 2.0, 120, 64); double stepS = 6000; CHECK(log.size() == 6);
      if (log.size() == 6) for (int i = 0; i < 3; ++i) { CHECK(std::abs((double)log[(size_t)i].first - i * stepS / 3) <= 1); CHECK(std::abs((double)log[(size_t)(3 + i)].first - (8 * stepS + i * stepS / 3)) <= 1); } }
    // transport: stopped = silence, start mid-bar, loop jump back
    { auto m = tu::machine(48000); tu::clearAll(*m); std::vector<std::pair<int64_t, int>> log; m->trigLog = &log;
      for (int s = 0; s < 16; ++s) { Step st; st.on = 1; m->bank.set(0, 0, s, st); }
      tu::render(*m, 48000, 1.0, 120, 256, false); CHECK(log.empty());
      tu::render(*m, 48000, 0.13, 120, 256, true, 0, 3.0); CHECK(!log.empty() && m->playStep.load() >= 12);
      if (!log.empty()) CHECK(log.size() >= 1);
      log.clear(); tu::render(*m, 48000, 0.01, 120, 256, false); // stop
      m->params[gp(G_SEQ)] = 0; tu::render(*m, 48000, 1.0, 120, 256, true); CHECK(log.empty()); m->params[gp(G_SEQ)] = 1;
      // loop: play 0..1 beat, then host jumps back to 0
      log.clear(); tu::render(*m, 48000, 0.5, 120, 480, true, 0, 0.0); size_t first = log.size(); tu::render(*m, 48000, 0.5, 120, 480, true, 0, 0.0);
      CHECK(first == 4 && log.size() == 8); }
    // internal clock when host is stopped (mode Host + internal)
    { auto m = tu::machine(48000); tu::clearAll(*m); std::vector<std::pair<int64_t, int>> log; m->trigLog = &log; m->params[gp(G_SEQ)] = 2; m->params[gp(G_RUN)] = 1; m->params[gp(G_TEMPO)] = 120;
      Step st; st.on = 1; m->bank.set(0, 0, 0, st); m->bank.set(0, 0, 4, st); tu::render(*m, 48000, 2.0, 120, 256, false); CHECK(log.size() == 2 && log[1].first == 24000); }
    // humanize never early, at most 10 ms late
    { auto m = tu::machine(48000); tu::clearAll(*m); std::vector<std::pair<int64_t, int>> log; m->trigLog = &log; m->params[gp(G_HUMAN)] = 1;
      for (int s = 0; s < 16; s += 2) { Step st; st.on = 1; m->bank.set(0, 0, s, st); }
      tu::render(*m, 48000, 8.0, 120, 256); bool ok = !log.empty(); double late = 0; size_t k = 0;
      for (auto& e : log) { double exp = k * 12000.0; double d = (double)e.first - exp; ok &= d >= -0.5 && d <= 480.5; late += d; ++k; }
      CHECK(ok && late > 0); }
    // pattern switching, length, mute keeps sequencer running
    { auto m = tu::machine(48000); tu::clearAll(*m); std::vector<std::pair<int64_t, int>> log; m->trigLog = &log;
      Step st; st.on = 1; m->bank.set(3, 2, 0, st); m->bank.setLength(3, 4); m->params[gp(G_PATTERN)] = 4;
      tu::render(*m, 48000, 2.0, 120, 256); CHECK(log.size() == 4); for (auto& e : log) CHECK(e.second == 2); }
    // pattern serialisation + accents/velocity
    { PatternBank b; Step st; st.on = 1; st.vel = 77; st.prob = 33; st.accent = 1; st.ratchet = 4; b.set(2, 5, 9, st); b.setLength(2, 48);
      auto txt = b.serialise(2); PatternBank c; CHECK(c.deserialise(7, txt)); auto r = c.get(7, 5, 9); CHECK(r.on && r.vel == 77 && r.prob == 33 && r.accent && r.ratchet == 4); CHECK(c.length(7) == 48); CHECK(c.hits(7) == 1);
      CHECK(!c.deserialise(1, "garbage")); }
}

static void testMidiExport() {
    static PatternBank b; fresh(); auto* p = findPattern("Trap Rolls"); CHECK(p != nullptr); applyPattern(b, 0, *p);
    auto bytes = patternToMidi(b, 0, V, 140);
    CHECK(bytes.size() > 30 && std::memcmp(bytes.data(), "MThd", 4) == 0 && std::memcmp(bytes.data() + 14, "MTrk", 4) == 0);
    uint32_t tl = (uint32_t)bytes[18] << 24 | (uint32_t)bytes[19] << 16 | (uint32_t)bytes[20] << 8 | bytes[21]; CHECK(tl + 22 == bytes.size());
    // parse
    size_t i = 22; int64_t t = 0; int ons = 0, offs = 0; bool end = false, ok = true; std::vector<int64_t> onTimes; uint8_t run = 0;
    while (i < bytes.size() && ok) {
        uint64_t d = 0; uint8_t c; do { c = bytes[i++]; d = (d << 7) | (c & 0x7F); } while ((c & 0x80) && i < bytes.size()); t += (int64_t)d;
        uint8_t st = bytes[i]; if (st & 0x80) { run = st; ++i; } else st = run;
        if (st == 0xFF) { uint8_t ty = bytes[i++]; uint8_t len = bytes[i++]; if (ty == 0x2F) end = true; i += len; }
        else if ((st & 0xF0) == 0x90) { if (bytes[i + 1] > 0) { ++ons; onTimes.push_back(t); } i += 2; }
        else if ((st & 0xF0) == 0x80) { ++offs; i += 2; } else ok = false;
    }
    int expected = 0; for (int tr = 0; tr < kChannels; ++tr) for (int s = 0; s < b.length(0); ++s) { auto st = b.get(0, tr, s); if (st.on) expected += st.ratchet; }
    CHECKM(ok && end && ons == expected && offs == expected, "midi parse ok=%d end=%d ons=%d offs=%d expected=%d", ok, end, ons, offs, expected);
    CHECK(!onTimes.empty() && onTimes.front() == 0);
    CHECK(t == (int64_t)b.length(0) * 240); // 32 sixteenths at 960 ppq
}

static void testEffects() {
    const double sr = 48000; FxSlot fx; fx.prepare((float)sr, 2.f);
    auto runSine = [&](float slot[7], double f, double amp, int n, std::vector<float>* outR = nullptr) {
        std::vector<float> L((size_t)n), R((size_t)n); for (int i = 0; i < n; ++i) L[(size_t)i] = R[(size_t)i] = (float)(amp * std::sin(2 * M_PI * f * i / sr));
        for (int o = 0; o < n; o += 256) fx.process(L.data() + o, R.data() + o, std::min(256, n - o), slot, 120); if (outR) *outR = R; return L; };
    auto inv = [](float target, float lo, float hi) { return std::log(target / lo) / std::log(hi / lo); };
    // filter modes
    for (int mode = 0; mode < 4; ++mode) {
        float slot[7] = {FX_FILTER, inv(1000, 20, 20000), 0.f, (mode + 0.5f) / 4.f, 0, 0.5f, 1};
        fx.reset(); auto lo = runSine(slot, 100, 0.5, 24000); fx.reset(); auto mid = runSine(slot, 1000, 0.5, 24000); fx.reset(); auto hi = runSine(slot, 10000, 0.5, 24000);
        double gl = tu::peak(lo, 12000) / 0.5, gm = tu::peak(mid, 12000) / 0.5, gh = tu::peak(hi, 12000) / 0.5;
        if (mode == 0) CHECKM(gl > 0.97 && gh < 0.03, "LP %.3f %.3f", gl, gh);
        if (mode == 1) CHECKM(gh > 0.97 && gl < 0.03, "HP %.3f %.3f", gl, gh);
        if (mode == 2) CHECKM(gm > 0.9 && gl < 0.2 && gh < 0.2, "BP %.3f %.3f %.3f", gl, gm, gh);
        if (mode == 3) CHECKM(gm < 0.1 && gl > 0.9 && gh > 0.9, "Notch %.3f %.3f %.3f", gl, gm, gh);
    }
    // resonance boosts at cutoff
    { float a[7] = {FX_FILTER, inv(1000, 20, 20000), 0.9f, 0.1f, 0, 0.5f, 1}; fx.reset(); auto r = runSine(a, 1000, 0.1, 24000); CHECK(tu::peak(r, 12000) > 0.3); }
    // compressor static curve: -6 dBFS sine, thr -24, ratio 4 -> -19.5 dBFS (soft knee irrelevant at 18 dB over)
    { float slot[7] = {FX_COMP, (60.f - 24.f) / 60.f, std::sqrt(3.f / 19.f), 0.2f, 0.4f, 0, 1};
      fx.reset(); auto o = runSine(slot, 200, 0.5012, 48000); double db = 20 * std::log10(tu::peak(o, 36000));
      CHECKM(std::abs(db + 19.5) < 1.0, "compressor output %.2f dB", db); CHECK(fx.gainReduction() > 12 && fx.gainReduction() < 15);
      CHECK(std::abs(Compressor::gainReductionDb(-6, -24, 4) - 13.5f) < 1e-3); CHECK(Compressor::gainReductionDb(-40, -24, 4) == 0); }
    // transient shaper: attack+ boosts onset vs tail; sustain- reduces tail
    { auto hit = [&](float att, float sus) { float slot[7] = {FX_TRANS, (att + 1) / 2, (sus + 1) / 2, 0.4f, 0.5f, 0, 1}; fx.reset();
        std::vector<float> L(24000), R(24000); for (int i = 0; i < 24000; ++i) L[(size_t)i] = R[(size_t)i] = (float)(std::sin(2 * M_PI * 150 * i / sr) * std::exp(-i / (0.12 * sr)));
        for (int o = 0; o < 24000; o += 256) fx.process(L.data() + o, R.data() + o, (int)std::min<size_t>(256, L.size() - o), slot, 120); return L; };
      auto flat = hit(0, 0), punch = hit(1, 0), dry = hit(0, -1);
      CHECK(tu::peak(punch, 0, 480) > 1.5 * tu::peak(flat, 0, 480)); CHECK(tu::rms(dry, 9600, 19200) < 0.5 * tu::rms(flat, 9600, 19200)); CHECK(std::abs(tu::rms(flat) / tu::rms(hit(0, 0)) - 1) < 1e-6); }
    // delay timing and feedback
    { float slot[7] = {FX_DELAY, std::log(250.f) / std::log(1000.f), 0.5f / 0.95f, 1.f, 0.f, 0.f, 1}; fx.reset();
      std::vector<float> L(48000), R(48000); L[0] = R[0] = 1; for (int o = 0; o < 48000; o += 256) fx.process(L.data() + o, R.data() + o, (int)std::min<size_t>(256, L.size() - o), slot, 120);
      size_t p1 = (size_t)(std::max_element(L.begin(), L.begin() + 18000, [](float a, float b) { return std::abs(a) < std::abs(b); }) - L.begin());
      size_t p2 = (size_t)(std::max_element(L.begin() + 18000, L.end(), [](float a, float b) { return std::abs(a) < std::abs(b); }) - L.begin());
      CHECKM(std::abs((double)p1 - 12000) <= 2 && std::abs((double)p2 - 24000) <= 3, "delay taps %zu %zu", p1, p2);
      double g = tu::peak(L, 23000, 25000) / tu::peak(L, 11000, 13000); CHECKM(std::abs(g - 0.5) < 0.1, "feedback %.3f", g);
      float s2[7] = {FX_DELAY, 0.5f, 0, 1, 0, (9 + 0.5f) / 11.f, 1}; fx.reset(); std::vector<float> A(48000), B(48000); A[0] = B[0] = 1; for (int o = 0; o < 48000; o += 256) fx.process(A.data() + o, B.data() + o, (int)std::min<size_t>(256, A.size() - o), s2, 120);
      size_t q = (size_t)(std::max_element(A.begin() + 100, A.end(), [](float a, float b) { return std::abs(a) < std::abs(b); }) - A.begin()); CHECKM(std::abs((double)q - 24000) <= 2, "synced 1/4 at 120 bpm: %zu", q); }
    // reverb: stable, decays, finite at max settings
    { float slot[7] = {FX_REVERB, 1, 1, 0, 1, 0, 1}; fx.reset(); std::vector<float> L(48000 * 20), R(48000 * 20); L[0] = R[0] = 1;
      for (size_t o = 0; o < L.size(); o += 256) fx.process(L.data() + o, R.data() + o, (int)std::min<size_t>(256, L.size() - o), slot, 120);
      CHECK(tu::finite(L) && tu::finite(R) && tu::peak(L) < 2.f); CHECK(tu::rms(L, 48000 * 18) < tu::rms(L, 0, 48000 * 2) * 0.5);
      float s2[7] = {FX_REVERB, 0.5f, 0.3f, 0.5f, 0.7f, 0, 1}; fx.reset(); std::vector<float> A(96000), B(96000); A[0] = B[0] = 1; for (size_t o = 0; o < A.size(); o += 256) fx.process(A.data() + o, B.data() + o, (int)std::min<size_t>(256, A.size() - o), s2, 120);
      CHECK(tu::rms(A, 0, 24000) > 1e-4 && tu::rms(A, 72000) < tu::rms(A, 0, 24000) * 0.05); double d = 0; for (size_t i = 0; i < A.size(); ++i) d += std::abs(A[i] - B[i]); CHECK(d > 0.1); }
    // bitcrush levels
    { float slot[7] = {FX_CRUSH, 12.f / 15.f, 0, 0, 0, 0, 1}; fx.reset(); auto o = runSine(slot, 100, 0.9, 4800); std::set<int> lv; for (float x : o) lv.insert((int)std::lround(x * 1000)); CHECKM(lv.size() <= 17, "4-bit levels %zu", lv.size()); }
    // sample-rate reduction creates aliasing images
    { float slot[7] = {FX_CRUSH, 0, 0.7f, 0, 0, 0, 1}; fx.reset(); auto o = runSine(slot, 1000, 0.5, 24000); float rate = fxValue(FX_CRUSH, 1, 0.7f); CHECK(tu::tone(o, rate - 1000, sr, 4800) > 0.01); }
    // pitch shift +12 doubles frequency
    { float slot[7] = {FX_PITCH, 0.75f, 0.4f, 0.5f, 0, 0, 1}; fx.reset(); auto o = runSine(slot, 440, 0.5, 24000); CHECK(tu::tone(o, 880, sr, 4800) > 4 * tu::tone(o, 440, sr, 4800)); }
    // ring mod: sum and difference
    { float slot[7] = {FX_RING, inv(1000, 20, 5000), 0, 0, 0, 0, 1}; fx.reset(); auto o = runSine(slot, 300, 0.5, 24000);
      CHECK(tu::tone(o, 700, sr, 4800) > 0.15 && tu::tone(o, 1300, sr, 4800) > 0.15 && tu::tone(o, 300, sr, 4800) < 0.02); }
    // widener: width 0 = mono, width 2 increases side
    { float slot[7] = {FX_WIDE, 0, 0, 0, 0, 0, 1}; fx.reset(); std::vector<float> L(4800), R(4800); for (int i = 0; i < 4800; ++i) { L[(size_t)i] = (float)std::sin(i * 0.05); R[(size_t)i] = (float)std::sin(i * 0.07); }
      fx.process(L.data(), R.data(), 4096, slot, 120); double d = 0; for (int i = 0; i < 4096; ++i) d += std::abs(L[(size_t)i] - R[(size_t)i]); CHECK(d < 1e-3); }
    // distortion adds harmonics, every mode bounded
    for (int mode = 0; mode < 5; ++mode) { float slot[7] = {FX_DIST, 0.6f, (mode + 0.5f) / 5.f, 1, 0, 0.8f, 1}; fx.reset(); auto o = runSine(slot, 200, 0.8, 24000);
      CHECKM(tu::tone(o, 600, sr, 4800) > 0.01 || tu::tone(o, 400, sr, 4800) > 0.01, "dist mode %d no harmonics", mode); CHECK(tu::peak(o) < 3.f); }
    // mix 0 = dry, every type at every rate finite
    for (double r : kRates) { FxSlot f; f.prepare((float)r, 1.f); Rng g(3);
        for (int t = 1; t < FX_COUNT; ++t) { float slot[7] = {(float)t, g.uni(), g.uni(), g.uni(), g.uni(), g.uni(), 1}; std::vector<float> L(8192), R(8192); for (auto& x : L) x = g.bi(); R = L;
            for (int o = 0; o < 8192; o += 512) f.process(L.data() + o, R.data() + o, 512, slot, 133); CHECKM(tu::finite(L) && tu::finite(R) && tu::peak(L) <= 8.f, "fx %d @%g", t, r); }
        float off[7] = {FX_COMP, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0}; std::vector<float> L(1024), R(1024); for (int i = 0; i < 1024; ++i) L[(size_t)i] = R[(size_t)i] = (float)std::sin(i * 0.1); auto dry = L;
        f.process(L.data(), R.data(), 1024, off, 120); f.process(L.data(), R.data(), 1024, off, 120); double d = 0; for (int i = 0; i < 1024; ++i) d += std::abs(L[(size_t)i] - dry[(size_t)i]); CHECK(d < 1e-3); }
}

static void testLimiterAndSafety() {
    auto m = tu::machine(48000); for (int c = 0; c < kChannels; ++c) { m->params[cp(c, V_LEVEL)] = 6; m->params[cp(c, M_VOL)] = 6; m->params[cp(c, V_DRIVE)] = 1; }
    m->params[gp(G_MVOL)] = 6; m->params[gp(G_CEIL)] = -1;
    for (int t = 0; t < kChannels; ++t) for (int s = 0; s < 16; ++s) { Step st; st.on = 1; st.accent = 1; st.ratchet = 4; m->bank.set(0, t, s, st); }
    auto r = tu::render(*m, 48000, 4.0, 160); float pk = std::max(tu::peak(r.L), tu::peak(r.R));
    CHECKM(pk <= dbToGain(-1.f) + 1e-6f && pk > 0.5f, "limiter peak %.4f", pk); CHECK(tu::finite(r.L) && tu::finite(r.R));
    // silence after the hit really is zero (no denormals)
    auto m2 = tu::machine(48000); m2->params[mp(0, 0)] = 0; auto r2 = tu::render(*m2, 48000, 40.0, 120, 512, false, 0, 0, [](int64_t pos, std::vector<MidiEvent>& ev) { if (pos == 0) for (int n : {36, 38, 42, 46, 39, 45, 37, 56, 76, 53, 49, 75}) ev.push_back({0, 0x99, (uint8_t)n, 127}); });
    CHECK(tu::finite(r2.L)); CHECK(tu::peak(r2.L, 48000 * 38) == 0.f);
}

static void testBlockSizesAndRates() {
    auto setup = [](DrumMachine& m) { auto* k = findPattern("Breakbeat"); applyPattern(m.bank, 0, *k); m.params[mp(0, 0)] = 0; m.params[sp(0, 0)] = 0; m.params[sp(1, 0)] = 0; };
    auto ref = tu::machine(48000); setup(*ref); auto rr = tu::render(*ref, 48000, 3.0, 128, 512);
    for (int bs : {1, 2, 7, 64, 255, 333, 1024, 2048}) {
        auto m = tu::machine(48000); setup(*m); auto r = tu::render(*m, 48000, 3.0, 128, bs); double d = 0; for (size_t i = 0; i < r.L.size(); ++i) d = std::max(d, (double)std::abs(r.L[i] - rr.L[i]));
        CHECKM(d < 1e-4, "block %d differs by %g", bs, d);
    }
    for (double sr : kRates) for (int bs : {16, 512, 2048}) {
        auto m = tu::machine(sr); for (auto& k : factoryKits()) { if (std::string(k.name) == "Distorted Industrial Kit") applyKit(m->params, &m->bank, 0, k); }
        auto r = tu::render(*m, sr, 2.0, 140, bs); CHECKM(tu::finite(r.L) && tu::rms(r.L) > 0.01 && tu::peak(r.L) <= dbToGain(-0.3f) + 1e-6f, "kit @%g block %d", sr, bs);
    }
}

static void testSampleLayer() {
    const double ssr = 44100; SampleData sd; sd.rate = ssr; int n = (int)(ssr * 0.5);
    for (int i = 0; i < n; ++i) { float env = (float)i / n; sd.L.push_back((float)(0.8 * env * std::sin(2 * M_PI * 1000 * i / ssr))); } sd.R = sd.L;
    fresh(); V[cp(0, S_SYNTH)] = 0; V[cp(0, S_LEVEL)] = 1; V[cp(0, V_LEVEL)] = 0;
    auto len = [](const std::vector<float>& x) { for (size_t i = x.size(); i-- > 0;) if (std::abs(x[i]) > 1e-3) return (double)i / 48000; return 0.0; };
    auto base = renderHit(&V[cp(0, 0)], 48000, 48000, 1, &sd, nullptr, 48000);
    CHECK(tu::peak(base) > 0.5 && std::abs(len(base) - 0.5) < 0.01); CHECK(std::abs(tu::zcFreq(base, 48000, 12000, 22000) - 1000) < 5);
    V[cp(0, S_START)] = 0.5f; auto half = renderHit(&V[cp(0, 0)], 48000, 48000, 1, &sd, nullptr, 48000); CHECK(std::abs(len(half) - 0.25) < 0.01);
    V[cp(0, S_START)] = 0; V[cp(0, S_END)] = 0.5f; auto endc = renderHit(&V[cp(0, 0)], 48000, 48000, 1, &sd, nullptr, 48000); CHECK(std::abs(len(endc) - 0.25) < 0.01 && tu::peak(endc) < 0.45);
    V[cp(0, S_END)] = 1; V[cp(0, S_REV)] = 1; auto rev = renderHit(&V[cp(0, 0)], 48000, 48000, 1, &sd, nullptr, 48000); CHECK(tu::peak(rev, 0, 2400) > 3 * tu::peak(rev, 19200, 21600));
    V[cp(0, S_REV)] = 0; V[cp(0, S_PITCH)] = 12; auto up = renderHit(&V[cp(0, 0)], 48000, 48000, 1, &sd, nullptr, 48000); CHECK(std::abs(tu::zcFreq(up, 48000, 6000, 11000) - 2000) < 10 && std::abs(len(up) - 0.25) < 0.01);
    V[cp(0, S_PITCH)] = 0; V[cp(0, S_STRETCH)] = 2; auto st = renderHit(&V[cp(0, 0)], 48000, 72000, 1, &sd, nullptr, 72000);
    CHECKM(std::abs(len(st) - 1.0) < 0.06, "stretch length %.3f", len(st)); CHECK(std::abs(tu::zcFreq(st, 48000, 24000, 36000) - 1000) < 15);
    V[cp(0, S_STRETCH)] = 1; V[cp(0, S_LOOP)] = 1; auto lp = renderHit(&V[cp(0, 0)], 48000, 72000, 1, &sd, nullptr, 72000); CHECK(tu::peak(lp, 36000, 60000) > 0.3);
    V[cp(0, S_LOOP)] = 0; V[cp(0, S_DEC)] = 100; auto dec = renderHit(&V[cp(0, 0)], 48000, 48000, 1, &sd, nullptr, 48000); CHECK(len(dec) < 0.12);
    V[cp(0, S_DEC)] = 10000; V[cp(0, S_CUT)] = 200; auto lpf = renderHit(&V[cp(0, 0)], 48000, 48000, 1, &sd, nullptr, 48000); CHECK(tu::peak(lpf) < 0.2 * tu::peak(base));
    // layering: synth + sample both present
    V[cp(0, S_CUT)] = 20000; V[cp(0, S_SYNTH)] = 1; auto both = renderHit(&V[cp(0, 0)], 48000, 48000, 1, &sd, nullptr, 48000); CHECK(tu::tone(both, 1000, 48000, 12000, 22000) > 0.05 && tu::tone(both, 50, 48000, 0, 9600) > 0.02);
    // sample hand-over through the machine
    auto m = tu::machine(48000); m->params[cp(0, S_SYNTH)] = 0; m->params[cp(0, S_LEVEL)] = 1; m->params[mp(0, 0)] = 0; m->samplePending[0].store(&sd);
    auto r = tu::render(*m, 48000, 0.6, 120, 256, false, 0, 0, [](int64_t pos, std::vector<MidiEvent>& ev) { if (pos == 2560) ev.push_back({0, 0x99, 36, 127}); });
    CHECK(m->sampleAck[0].load() == &sd && tu::peak(r.L) > 0.3);
}

static void testFactoryAndRandom() {
    int perCat[C_COUNT] = {};
    for (auto& s : factorySounds()) {
        fresh(); CHECK(applySound(V, 0, s)); auto h = renderHit(&V[cp(0, 0)], 48000, 48000); float pk = tu::peak(h); double db = 20 * std::log10(hitPeak(h, 48000) + 1e-12);
        CHECKM(pk < 0.95f && pk > 0.05f && db < normTarget(s.engine) + 2.0 && db > normTarget(s.engine) - 6.0, "%s peak %.3f (%.1f dB)", s.name, pk, db); ++perCat[s.category];
        CHECK(tu::finite(h));
    }
    for (int c = 0; c < C_KIT; ++c) CHECKM(perCat[c] >= 4, "%s has %d", categoryName(c), perCat[c]);
    CHECK(factoryKits().size() >= 10 && factoryPatterns().size() >= 10);
    for (auto& k : factoryKits()) {
        auto m = tu::machine(48000); CHECKM(applyKit(m->params, &m->bank, 0, k), "%s", k.name);
        auto r = tu::render(*m, 48000, 4.0, 124); double rm = tu::rms(r.L); float pk = std::max(tu::peak(r.L), tu::peak(r.R));
        CHECKM(rm > 0.01 && pk <= dbToGain(-0.3f) + 1e-6f && tu::finite(r.L), "kit %s rms %.4f peak %.3f", k.name, rm, pk);
    }
    for (auto& p : factoryPatterns()) { static PatternBank b; CHECK(applyPattern(b, 1, p)); CHECKM(b.hits(1) > 4 && b.length(1) == p.length, "%s", p.name); }
    // random sounds: thousands of seeds, every mode and engine
    int bad = 0, total = 0;
    for (int seed = 1; seed <= 1400; ++seed) {
        Rng rng((uint64_t)seed); fresh(); Locks lk; int e = seed % E_COUNT, mode = (seed / E_COUNT) % R_COUNT; float inten = (seed % 11) / 10.f;
        randomSound(V, 0, mode, inten, rng, lk, e);
        auto h = renderHit(&V[cp(0, 0)], 44100, 44100); float pk = tu::peak(h); ++total;
        if (!(pk < 0.95f && pk > 0.01f && tu::finite(h))) { ++bad; if (bad < 5) std::printf("  random sound seed %d engine %s mode %s peak %.4f\n", seed, engineName(e), randomModeName(mode), pk); }
    }
    CHECKM(bad == 0, "%d of %d random sounds unsafe", bad, total);
    // random kits & patterns: coherent, locks respected, renders safe
    int kbad = 0;
    for (int seed = 1; seed <= 140; ++seed) {
        Rng rng((uint64_t)seed * 77); auto m = tu::machine(44100); Locks lk; lk.sound[1] = true; lk.track[0] = true; lk.params.insert(cp(0, V_DEC));
        for (int s = 0; s < 16; s += 4) { Step st; st.on = 1; m->bank.set(0, 0, s, st); }
        float snare[kChanParams]; std::copy(&m->params[cp(1, 0)], &m->params[cp(1, 0)] + kChanParams, snare); float dec = m->params[cp(0, V_DEC)];
        int mode = seed % R_COUNT; randomKit(m->params, mode, (seed % 10) / 9.f, rng, lk); randomPattern(m->bank, 0, mode, 0.6f, rng, lk);
        CHECK(std::equal(snare, snare + kChanParams, &m->params[cp(1, 0)])); CHECK(m->params[cp(0, V_DEC)] == dec);
        int kicks = 0; for (int s = 0; s < 16; ++s) kicks += m->bank.get(0, 0, s).on; CHECK(kicks == 4 && m->bank.hits(0) > 4);
        for (int c = 0; c < kChannels; ++c) for (int k = 0; k < kChanParams; ++k) { float x = m->params[cp(c, k)]; const auto& d = registry()[(size_t)cp(c, k)]; if (!(x >= d.lo && x <= d.hi)) ++kbad; }
        if (seed % 10 == 0) { auto r = tu::render(*m, 44100, 3.0, 128); if (!(tu::finite(r.L) && tu::rms(r.L) > 0.003 && tu::peak(r.L) <= dbToGain(-0.3f) + 1e-6f)) ++kbad; }
    }
    CHECKM(kbad == 0, "%d random kit problems", kbad);
    { Rng rng(9); static PatternBank b; Locks lk; applyPattern(b, 0, *findPattern("Boom Bap")); patternVariation(b, 0, 1, 0.6f, rng, lk); int diff = 0;
      for (int t = 0; t < kChannels; ++t) for (int s = 0; s < 16; ++s) diff += b.get(0, t, s).on != b.get(1, t, s).on || b.get(0, t, s).ratchet != b.get(1, t, s).ratchet || b.get(0, t, s).prob != b.get(1, t, s).prob;
      CHECK(diff > 0 && diff < 20); for (int s = 0; s < 16; ++s) CHECK(b.get(0, 0, s).on == b.get(1, 0, s).on); }
}


static void testKeyDisplayAndSnap() {
    auto n = noteCentsFromHz(440.f);
    CHECK(n.valid && n.pc == 9 && n.octave == 4);
    CHECK(std::abs(n.cents) < 0.51f);
    CHECK(formatNoteCents(n) == "A4");
    n = noteCentsFromHz(hzFromMidi(36.f)); // C2
    CHECK(n.pc == 0 && n.octave == 2 && std::abs(n.cents) < 0.51f);
    CHECK(formatNoteCents(n) == "C2");
    n = noteCentsFromHz(50.f);
    CHECK(n.pc == 7 && n.octave == 1);
    CHECK(std::abs(n.cents - 35.12f) < 1.f);
    CHECK(formatNoteCents(n).find("G1") != std::string::npos);
    CHECK(formatNoteCents(n).find("+35") != std::string::npos);
    // +12 semitones of Tune is exactly one octave
    CHECK(std::abs(midiFromHz(settledHz(440.f, 12.f)) - 81.f) < 1e-4f);
    // Sweep does not change settled body pitch
    fresh(); applyEngineDefaults(V, 0, E_KICK); V[cp(0, V_FREQ)] = 50; V[cp(0, V_TUNE)] = 0; V[cp(0, V_SWEEP)] = 36; V[cp(0, V_SWEEPT)] = 80;
    CHECK(std::abs(settledHz(V[cp(0, V_FREQ)], V[cp(0, V_TUNE)]) - 50.f) < 1e-5f);
    auto shown = noteCentsFromHz(settledHz(V[cp(0, V_FREQ)], V[cp(0, V_TUNE)]));
    CHECK(shown.pc == 7 && shown.octave == 1);
    // Keep off: no change
    V[gp(G_KEEPKEY)] = 0; V[gp(G_KEY)] = 4; V[gp(G_SCALE)] = (float)SC_MINOR; V[gp(G_SNAPMODE)] = 0;
    float f0 = V[cp(0, V_FREQ)]; applyKeepInKey(V); CHECK(std::abs(V[cp(0, V_FREQ)] - f0) < 1e-6f);
    // E minor: G is in the scale, so 50 Hz snaps to exact G1 (cents go to 0)
    V[gp(G_KEEPKEY)] = 1;
    applyKeepInKey(V);
    auto after = noteCentsFromHz(settledHz(V[cp(0, V_FREQ)], V[cp(0, V_TUNE)]));
    CHECK(after.pc == 7 && after.octave == 1 && std::abs(after.cents) < 0.51f);
    // F1 (MIDI 29) in E minor snaps to E1 (nearer than G)
    V[cp(0, V_FREQ)] = hzFromMidi(29.f); V[cp(0, V_TUNE)] = 0;
    applyKeepInKey(V);
    after = noteCentsFromHz(settledHz(V[cp(0, V_FREQ)], V[cp(0, V_TUNE)]));
    CHECK(after.pc == 4 && after.octave == 1 && std::abs(after.cents) < 0.51f);
    // Hats are not snapped
    applyEngineDefaults(V, 2, E_CHAT); float hatF = V[cp(2, V_FREQ)]; applyKeepInKey(V);
    CHECK(std::abs(V[cp(2, V_FREQ)] - hatF) < 1e-5f);
    // Root snap: a G kick goes to E
    applyEngineDefaults(V, 0, E_KICK); V[cp(0, V_FREQ)] = 50; V[cp(0, V_TUNE)] = 0;
    V[gp(G_SNAPMODE)] = 1; applyKeepInKey(V);
    after = noteCentsFromHz(settledHz(V[cp(0, V_FREQ)], V[cp(0, V_TUNE)]));
    CHECK(after.pc == 4 && std::abs(after.cents) < 0.51f);
    // Tune is preserved: settled lands on the note, Tune unchanged
    applyEngineDefaults(V, 5, E_TOM); V[cp(5, V_FREQ)] = 110; V[cp(5, V_TUNE)] = 3; V[gp(G_SNAPMODE)] = 0; V[gp(G_KEY)] = 0; V[gp(G_SCALE)] = (float)SC_MAJOR; V[gp(G_KEEPKEY)] = 1;
    float tuneBefore = V[cp(5, V_TUNE)];
    applyKeepInKey(V);
    CHECK(std::abs(V[cp(5, V_TUNE)] - tuneBefore) < 1e-6f);
    after = noteCentsFromHz(settledHz(V[cp(5, V_FREQ)], V[cp(5, V_TUNE)]));
    CHECK(std::abs(after.cents) < 0.51f);
    // registry grew by 4 globals
    CHECK(paramIndex("g_keepkey") == gp(G_KEEPKEY));
    CHECK(paramIndex("g_key") == gp(G_KEY));
}

static void testLongRunAndCpu() {
    auto m = tu::machine(48000); applyKit(m->params, &m->bank, 0, factoryKits()[3]); m->params[cp(2, F1_TYPE)] = FX_DELAY; m->params[cp(5, F1_TYPE)] = FX_REVERB; m->params[mp(1, 0)] = FX_DIST; m->params[mp(2, 0)] = FX_REVERB; m->params[mp(2, 6)] = 0.2f;
    Rng rng(4); double audio = 0, cpu = 0; float pk = 0; bool fin = true;
    for (int seg = 0; seg < 20; ++seg) { // 20 x 30 s = 10 minutes, kit re-randomised every 30 s
        if (seg > 0) { Locks lk; randomKit(m->params, seg % R_COUNT, 0.8f, rng, lk); randomPattern(m->bank, 0, seg % R_COUNT, 0.8f, rng, lk); }
        auto r = tu::render(*m, 48000, 30.0, 100 + seg * 5, 512, true, 0, seg * 64.0); audio += 30; cpu += r.seconds; pk = std::max(pk, tu::peak(r.L)); fin &= tu::finite(r.L) && tu::finite(r.R);
    }
    CHECKM(fin && pk <= dbToGain(-0.3f) + 1e-6f, "long run fin=%d peak %.3f", fin, pk);
    std::printf("CPU long run (10 min, busy kits, FX): %.2f%% of one core (%.1fx realtime)\n", 100 * cpu / audio, audio / cpu);
    // worst-case: all 12 tracks on every 1/32 step with ratchets, all inserts on
    auto w = tu::machine(48000); for (int c = 0; c < kChannels; ++c) { w->params[cp(c, F1_TYPE)] = (float)(1 + c % (FX_COUNT - 1)); w->params[cp(c, F2_TYPE)] = (float)(1 + (c + 5) % (FX_COUNT - 1)); w->params[cp(c, M_SENDA)] = 0.3f; w->params[cp(c, M_SENDB)] = 0.3f; }
    for (int s = 0; s < 4; ++s) w->params[mp(s, 0)] = (float)(1 + s * 2);
    w->params[gp(G_RATE)] = 3; for (int t = 0; t < kChannels; ++t) for (int s = 0; s < 16; ++s) { Step st; st.on = 1; st.ratchet = 2; w->bank.set(0, t, s, st); }
    auto r = tu::render(*w, 48000, 20.0, 174, 256); std::printf("CPU worst case (12 tracks x 1/32 ratchets, 24 inserts + 4 master + 2 sends, 48 kHz): %.2f%% of one core\n", 100 * r.seconds / 20.0);
    CHECK(tu::finite(r.L) && r.seconds / 20.0 < 0.5);
    auto w2 = tu::machine(96000); applyKit(w2->params, &w2->bank, 0, factoryKits()[1]); auto r2 = tu::render(*w2, 96000, 10.0, 124, 256); std::printf("CPU 909 House Kit @96 kHz: %.2f%% of one core\n", 100 * r2.seconds / 10.0);
    auto w3 = tu::machine(44100); applyKit(w3->params, &w3->bank, 0, factoryKits()[1]); auto r3 = tu::render(*w3, 44100, 10.0, 124, 256); std::printf("CPU 909 House Kit @44.1 kHz: %.2f%% of one core\n", 100 * r3.seconds / 10.0);
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    bool quick = argc > 1 && std::string(argv[1]) == "--quick"; std::string only = argc > 2 ? argv[2] : "";
    struct T { const char* name; void (*fn)(); } tests[] = {{"registry", testRegistry}, {"engines x rates", testEnginesAllRates}, {"kick pitch/decay/FM/AM", testKickPitch}, {"snare/hat noise", testSnareHatNoise},
        {"percussion engines", testPercussion}, {"choke/MIDI/mixer/outputs", testMachineChokeMidiOutputs}, {"sequencer", testSequencer}, {"MIDI export", testMidiExport}, {"effects", testEffects},
        {"limiter/denormals", testLimiterAndSafety}, {"block sizes/rates", testBlockSizesAndRates}, {"sample layer", testSampleLayer}, {"factory + random", testFactoryAndRandom}, {"key display + keep in key", testKeyDisplayAndSnap}, {"long run + CPU", testLongRunAndCpu}};
    for (auto& t : tests) { if (quick && std::string(t.name) == "long run + CPU") continue; if (!only.empty() && std::string(t.name).find(only) == std::string::npos) continue; int f0 = g_fail; auto t0 = std::chrono::steady_clock::now(); t.fn();
        std::printf("[%s] %s (%.1fs)\n", g_fail == f0 ? "ok" : "FAILED", t.name, std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count()); }
    std::printf("ENGINE: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
