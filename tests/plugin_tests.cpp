// BAKERY — JUCE-level tests: parameters, buses/multi-out, MIDI, host sequencing, state, presets, MIDI export,
// MIDI learn, macros, editor screenshots and CPU. Runs headless on the CI Mac.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Key.h"
#include <chrono>
#include <cstdio>
using namespace bk;

static int passed = 0, failed = 0;
#define CHECK(x) do { if (x) ++passed; else { ++failed; std::printf("FAIL line %d: %s\n", __LINE__, #x); } } while (0)
#define CHECKM(x, ...) do { if (x) ++passed; else { ++failed; std::printf("FAIL line %d: %s | ", __LINE__, #x); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

struct Head : juce::AudioPlayHead {
    bool playing = false, hasTempo = true; double bpm = 120, ppq = 0;
    juce::Optional<PositionInfo> getPosition() const override { PositionInfo p; p.setIsPlaying(playing); if (hasTempo) p.setBpm(bpm); p.setPpqPosition(ppq); return p; }
};

struct Rig {
    BakeryProcessor p; Head head; double sr; int block;
    std::vector<std::vector<float>> out; // per output channel, accumulated
    Rig(double rate = 48000, int bs = 256) : sr(rate), block(bs) { p.setPlayHead(&head); p.setRateAndBufferSizeDetails(sr, block); p.prepareToPlay(sr, block); }
    void set(int i, float v) { p.setValue(i, v); }
    void seqOff() { set(gp(G_SEQ), 0); }
    /// Run for `seconds`; midiAt(blockStartSample, buffer) may add events.
    double run(double seconds, std::function<void(int64_t, juce::MidiBuffer&)> midiAt = {}, bool record = true, int bs = -1) {
        int b = bs > 0 ? bs : block; int nCh = p.getTotalNumOutputChannels(); if (record) out.assign((size_t)nCh, {});
        int64_t n = (int64_t)(seconds * sr), pos = 0; double t = 0;
        juce::AudioBuffer<float> buf(nCh, b);
        while (pos < n) {
            int m = (int)std::min<int64_t>(b, n - pos); buf.setSize(nCh, m, false, false, true); juce::MidiBuffer midi; if (midiAt) midiAt(pos, midi);
            auto t0 = std::chrono::steady_clock::now(); p.processBlock(buf, midi); t += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            if (record) for (int c = 0; c < nCh; ++c) out[(size_t)c].insert(out[(size_t)c].end(), buf.getReadPointer(c), buf.getReadPointer(c) + m);
            if (head.playing) head.ppq += m * head.bpm / 60.0 / sr;
            pos += m;
        }
        return t;
    }
    float peak(int c, size_t a = 0, size_t b = SIZE_MAX) const { float pk = 0; auto& x = out[(size_t)c]; for (size_t i = a; i < std::min(b, x.size()); ++i) pk = std::max(pk, std::abs(x[i])); return pk; }
    bool finite() const { for (auto& ch : out) for (float v : ch) if (!std::isfinite(v)) return false; return true; }
};
static auto noteAt(int64_t when, int note, int vel = 110) { return [=](int64_t pos, juce::MidiBuffer& m) { if (when >= pos && when < pos + 256) m.addEvent(juce::MidiMessage::noteOn(10, note, (juce::uint8)vel), (int)(when - pos)); }; }
static juce::File tempDir(const juce::String& n) { auto d = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("bakery-tests").getChildFile(n); d.deleteRecursively(); d.createDirectory(); return d; }
static juce::File writeTestWav(double seconds = 0.5) {
    auto f = tempDir("wav").getChildFile("tone.wav"); juce::WavAudioFormat wav; int n = (int)(44100 * seconds);
    juce::AudioBuffer<float> b(2, n); for (int i = 0; i < n; ++i) { float v = 0.5f * std::sin(2 * juce::MathConstants<float>::pi * 440 * i / 44100.f) * std::exp(-i / 8000.f); b.setSample(0, i, v); b.setSample(1, i, -v); }
    std::unique_ptr<juce::FileOutputStream> os(f.createOutputStream().release());
    std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(os.get(), 44100, 2, 24, {}, 0)); if (w) { os.release(); w->writeFromAudioSampleBuffer(b, 0, n); }
    return f;
}
static void savePng(juce::Component& c, const juce::String& name) {
    auto img = c.createComponentSnapshot(c.getLocalBounds(), true, 1.0f); auto f = juce::File::getCurrentWorkingDirectory().getChildFile(name); f.deleteFile();
    juce::FileOutputStream s(f); bool ok = s.openedOk() && juce::PNGImageFormat().writeImageToStream(img, s); CHECK(ok); std::printf("  screenshot %s %dx%d\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0); juce::ScopedJuceInitialiser_GUI gui; juce::ignoreUnused(argc, argv);
    std::puts("BAKERY plugin tests");

    { std::puts("- identity, parameters");
      Rig r; CHECK(r.p.getName() == "BAKERY"); CHECK(r.p.acceptsMidi() && !r.p.producesMidi() && !r.p.isMidiEffect()); CHECK(r.p.getLatencySamples() == 0);
      CHECK((int)r.p.getParameters().size() == kNumParams); int automatable = 0, texts = 0;
      for (auto* prm : r.p.getParameters()) { automatable += prm->isAutomatable(); texts += prm->getCurrentValueAsText().isNotEmpty(); }
      CHECK(automatable == kNumParams); CHECK(texts == kNumParams);
      juce::Random rnd(7); int okRound = 0;
      for (int i = 0; i < kNumParams; ++i) { auto* prm = r.p.param(i); float n = rnd.nextFloat(); prm->setValueNotifyingHost(n); float real = r.p.value(i); bool ok = std::abs(prm->convertTo0to1(real) - prm->convertTo0to1(prm->convertFrom0to1(n))) < 1e-4f; /* switches snap at 0.5 */ okRound += ok; if (!ok && okRound + 5 > i) std::printf("  mismatch %s: set %f real %f getValue %f conv %f\n", prm->paramID.toRawUTF8(), n, real, prm->getValue(), prm->convertTo0to1(real)); }
      CHECK(okRound == kNumParams);
      CHECK(r.p.apvts.getParameter("c1_freq") != nullptr && r.p.apvts.getParameter("g_swing") != nullptr && r.p.apvts.getParameter("m1type") != nullptr && r.p.apvts.getParameter("sbmix") != nullptr);
      std::printf("  %d parameters, all automatable\n", kNumParams); }

    { std::puts("- buses");
      Rig r; CHECK(r.p.getBusCount(false) == 1 + kChannels); CHECK(r.p.getBusCount(true) == 0); CHECK(r.p.getTotalNumOutputChannels() == 2 + 2 * kChannels);
      CHECK(r.p.getBus(false, 0)->getName() == "Main"); CHECK(r.p.getBus(false, 1)->getName() == "Out 2 Kick"); CHECK(r.p.getBus(false, 12)->getName() == "Out 13 Perc 8");
      auto l = r.p.getBusesLayout(); CHECK(r.p.checkBusesLayoutSupported(l));
      auto mono = l; mono.outputBuses.getReference(0) = juce::AudioChannelSet::mono(); CHECK(!r.p.checkBusesLayoutSupported(mono));
      auto mainOnly = l; for (int i = 1; i < mainOnly.outputBuses.size(); ++i) mainOnly.outputBuses.getReference(i) = juce::AudioChannelSet::disabled(); CHECK(r.p.checkBusesLayoutSupported(mainOnly)); }

    { std::puts("- MIDI triggering, silence");
      Rig r; r.seqOff(); r.run(0.5); CHECK(r.peak(0) == 0 && r.peak(1) == 0);
      r.run(0.6, noteAt(1000, 36)); float k = r.peak(0); CHECKM(k > 0.1f && k <= 1.0f, "kick peak %f", k); CHECK(r.finite()); CHECK(r.p.machine().hits[0].load() == 1);
      r.run(0.4, noteAt(100, 38)); CHECK(r.peak(0) > 0.05f); CHECK(r.p.machine().hits[1].load() == 1);
      // wrong MIDI channel is ignored when a channel is set
      r.set(gp(G_MIDICH), 3); r.run(0.3, noteAt(100, 36)); CHECK(r.p.machine().hits[0].load() == 1); r.set(gp(G_MIDICH), 0);
      // first sample of the trigger is at the event offset
      Rig q; q.seqOff(); q.run(0.2, noteAt(777, 36)); size_t first = 0; while (first < q.out[0].size() && q.out[0][first] == 0) ++first; CHECKM(first >= 765 && first <= 800, "first=%zu", first); }

    { std::puts("- multi-out routing");
      Rig r; r.seqOff(); r.set(cp(0, M_OUT), 1); r.run(0.5, noteAt(100, 36));
      CHECK(r.peak(0) < 1e-6f && r.peak(1) < 1e-6f); CHECK(r.peak(2) > 0.1f && r.peak(3) > 0.1f); for (int c = 4; c < 26; ++c) CHECK(r.peak(c) == 0);
      r.set(cp(1, M_OUT), 12); r.run(0.5, noteAt(100, 38)); CHECK(r.peak(24) > 0.05f && r.peak(25) > 0.05f); CHECK(r.peak(0) < 1e-6f);
      // when the host disables aux buses, routed channels fall back to Main
      auto l = r.p.getBusesLayout(); for (int i = 1; i < l.outputBuses.size(); ++i) l.outputBuses.getReference(i) = juce::AudioChannelSet::disabled();
      CHECK(r.p.setBusesLayout(l)); r.p.prepareToPlay(r.sr, r.block); CHECK(r.p.getTotalNumOutputChannels() == 2); r.run(0.5, noteAt(100, 36)); CHECK(r.peak(0) > 0.1f); CHECK(r.finite()); }

    { std::puts("- host-synced sequencer");
      Rig r; auto& b = r.p.bank(); int pat = r.p.currentPattern(); b.clear(pat); for (int s : {0, 4, 8, 12}) b.set(pat, 0, s, Step{1, 110, 100, 0, 1});
      r.run(1.0); CHECK(r.p.machine().hits[0].load() == 0); // host stopped
      std::vector<std::pair<int64_t, int>> log; r.p.machine().trigLog = &log;
      r.head.playing = true; r.head.ppq = 0; r.run(2.0 - 1e-4); CHECKM(r.p.machine().hits[0].load() == 4, "hits %u", r.p.machine().hits[0].load());
      // kick triggers land exactly on beats (24000 samples apart at 120 BPM / 48 kHz)
      CHECKM(log.size() == 4, "triggers %zu", log.size()); for (size_t i = 1; i < log.size(); ++i) CHECKM(std::llabs((log[i].first - log[i - 1].first) - 24000) <= 1, "gap %lld", (long long)(log[i].first - log[i - 1].first));
      if (!log.empty()) CHECK(log[0].first == 48000); r.p.machine().trigLog = nullptr;
      // looping from the host restarts cleanly
      r.head.ppq = 0; uint32_t before = r.p.machine().hits[0].load(); r.run(0.6); CHECK(r.p.machine().hits[0].load() == before + 2);
      // playStep is published for the UI
      CHECK(r.p.machine().playStep.load() >= 0);
      // internal clock (host stopped, mode Host+internal, PLAY on)
      Rig q; q.p.bank().clear(q.p.currentPattern()); q.p.bank().set(q.p.currentPattern(), 0, 0, Step{1, 100, 100, 0, 1}); q.set(gp(G_SEQ), 2); q.set(gp(G_TEMPO), 120); q.set(gp(G_RUN), 1);
      q.head.hasTempo = false; q.run(4.0 - 1e-3); CHECKM(q.p.machine().hits[0].load() == 2, "internal hits %u", q.p.machine().hits[0].load()); }

    { std::puts("- factory kits as programs");
      Rig r; CHECK(r.p.getNumPrograms() == (int)factoryKits().size() && r.p.getNumPrograms() == 12);
      for (int i = 0; i < r.p.getNumPrograms(); ++i) {
          r.p.setCurrentProgram(i); CHECK(r.p.getCurrentProgram() == i); CHECK(r.p.getProgramName(i) == juce::String(factoryKits()[(size_t)i].name));
          CHECK(r.p.bank().hits(r.p.currentPattern()) > 0); r.head.playing = true; r.head.ppq = 0; r.run(2.0); float pk = 0; for (int c = 0; c < 2; ++c) pk = std::max(pk, r.peak(c));
          CHECKM(pk > 0.05f && pk <= 1.0f, "kit %d peak %f", i, pk); CHECK(r.finite()); r.head.playing = false; r.run(0.05, {}, false);
      } }

    { std::puts("- state round trip (params, patterns, sample, MIDI learn, macros, locks, UI)");
      Rig a; a.set(cp(2, V_FREQ), 777); a.set(cp(5, M_PAN), -0.5f); a.set(gp(G_SWING), 61); a.set(mp(1, 0), FX_DIST); a.set(cp(0, V_ENGINE), E_METAL);
      a.p.bank().set(3, 7, 33, Step{1, 99, 55, 1, 3}); a.p.bank().setLength(3, 40); a.p.bank().names[3] = "Test pattern";
      juce::String err; CHECK(a.p.loadSample(4, writeTestWav(), err)); CHECK(a.p.sample(4) != nullptr && a.p.sample(4)->frames() == 22050);
      a.p.ccMap[74] = cp(0, V_DEC); a.p.linkMacro(2, cp(1, V_CUT)); a.p.locks.sound[6] = true; a.p.locks.track[9] = true; a.p.locks.params.insert(cp(3, V_TUNE)); a.p.uiWorkspace = 2; a.p.uiChannel = 5;
      juce::MemoryBlock st; a.p.getStateInformation(st); CHECK(st.getSize() > 1000);
      Rig b; b.p.setStateInformation(st.getData(), (int)st.getSize()); int same = 0; for (int i = 0; i < kNumParams; ++i) same += std::abs(a.p.param(i)->getValue() - b.p.param(i)->getValue()) < 1e-6f;
      CHECKM(same == kNumParams, "%d/%d params equal", same, kNumParams);
      for (int pt = 0; pt < kPatterns; ++pt) CHECK(a.p.bank().serialise(pt) == b.p.bank().serialise(pt));
      CHECK(b.p.bank().names[3] == "Test pattern"); auto s = b.p.bank().get(3, 7, 33); CHECK(s.on && s.vel == 99 && s.prob == 55 && s.accent && s.ratchet == 3);
      CHECK(b.p.sample(4) != nullptr && b.p.sample(4)->frames() == 22050 && std::abs(b.p.sample(4)->L[100] - a.p.sample(4)->L[100]) < 1e-6f && b.p.sample(4)->R[100] == a.p.sample(4)->R[100]);
      CHECK(b.p.ccFor(cp(0, V_DEC)) == 74); CHECK(b.p.macroLinks[2].size() == 1 && b.p.macroLinks[2][0].param == cp(1, V_CUT));
      CHECK(b.p.locks.sound[6] && b.p.locks.track[9] && b.p.locks.isLocked(cp(3, V_TUNE)) && !b.p.locks.sound[0]); CHECK(b.p.uiWorkspace == 2 && b.p.uiChannel == 5);
      b.seqOff(); b.run(0.5, noteAt(10, defaultNote(4))); CHECK(b.peak(0) > 0.01f); CHECK(b.finite());
      // garbage state must not crash or change anything
      std::vector<char> junk(5000); juce::Random jr(3); for (auto& c : junk) c = (char)jr.nextInt(256); b.p.setStateInformation(junk.data(), (int)junk.size()); CHECK(std::abs(b.p.value(cp(2, V_FREQ)) - 777) < 0.5f);
      b.p.setStateInformation(nullptr, 0);
      // sample swap while audio runs, then clear: old data is freed only after the audio thread let go
      b.p.clearSample(4); b.run(0.05, {}, false); b.p.collectGarbage(); CHECK(b.p.sample(4) == nullptr); }

    { std::puts("- presets (save, load, rename, favourite, delete)");
      Rig r; auto dir = tempDir("presets"); r.p.setPresetRoot(dir); CHECK(r.p.presetRoot() == dir);
      int factoryKicks = (int)r.p.presets(C_808K).size(); CHECK(factoryKicks >= 4);
      r.p.uiChannel = 0; r.set(cp(0, V_DEC), 1234); juce::String err; CHECK(r.p.saveUserPreset("My Kick", C_808K, 0, err)); CHECK(dir.getChildFile("808 Kicks/My Kick.bakery").existsAsFile());
      auto list = r.p.presets(C_808K); CHECK((int)list.size() == factoryKicks + 1);
      BakeryProcessor::PresetInfo mine{}; for (auto& p : list) if (!p.factory) mine = p; CHECK(mine.name == "My Kick");
      r.set(cp(0, V_DEC), 50); CHECK(r.p.loadPreset(mine, 0)); CHECKM(std::abs(r.p.value(cp(0, V_DEC)) - 1234) < 1, "dec %f", r.p.value(cp(0, V_DEC)));
      // sound presets load into another channel without touching its note/mixer
      r.set(cp(3, M_VOL), -9); int note3 = (int)r.p.value(cp(3, V_NOTE)); CHECK(r.p.loadPreset(mine, 3)); CHECK(std::abs(r.p.value(cp(3, V_DEC)) - 1234) < 1 && (int)r.p.value(cp(3, V_NOTE)) == note3 && std::abs(r.p.value(cp(3, M_VOL)) + 9) < 0.01f);
      r.p.toggleFavourite(mine); CHECK(r.p.isFavourite(mine)); CHECK(r.p.presets(C_808K)[0].name == "My Kick"); // favourites first
      CHECK(r.p.renameUserPreset(mine, "Big Kick")); list = r.p.presets(C_808K); BakeryProcessor::PresetInfo renamed{}; for (auto& p : list) if (!p.factory) renamed = p;
      CHECK(renamed.name == "Big Kick" && r.p.isFavourite(renamed)); CHECK(!dir.getChildFile("808 Kicks/My Kick.bakery").exists());
      CHECK(r.p.deleteUserPreset(renamed)); CHECK((int)r.p.presets(C_808K).size() == factoryKicks); CHECK(!r.p.deleteUserPreset(r.p.presets(C_808K)[0]));
      // kit preset with an embedded sample + pattern
      juce::String e2; CHECK(r.p.loadSample(2, writeTestWav(0.2), e2)); r.p.bank().set(r.p.currentPattern(), 11, 5, Step{1, 100, 100, 0, 2});
      CHECK(r.p.saveUserPreset("Kit A", C_KIT, 0, e2)); auto pat = r.p.bank().serialise(r.p.currentPattern()); float f5 = r.p.value(cp(5, V_FREQ));
      r.p.setCurrentProgram(3); r.p.clearSample(2); CHECK(r.p.sample(2) == nullptr);
      BakeryProcessor::PresetInfo kit{}; for (auto& p : r.p.presets(C_KIT)) if (!p.factory) kit = p; CHECK(kit.name == "Kit A");
      CHECK(r.p.loadPreset(kit, 0)); CHECK(r.p.sample(2) != nullptr); CHECK(r.p.bank().serialise(r.p.currentPattern()) == pat); CHECK(std::abs(r.p.value(cp(5, V_FREQ)) - f5) < 0.01f);
      // factory content in every category loads
      int loaded = 0, total = 0; for (int c = 0; c < C_COUNT; ++c) for (auto& p : r.p.presets(c)) if (p.factory) { ++total; loaded += r.p.loadPreset(p, 1); }
      CHECK(loaded == total); std::printf("  %d factory presets loaded (sounds + kits + patterns)\n", total);
      // pattern preset
      r.p.loadPreset(r.p.presets(C_PATTERN)[2], 0); CHECK(r.p.saveUserPreset("Groove", C_PATTERN, 0, e2)); auto g1 = r.p.bank().serialise(r.p.currentPattern()); r.p.bank().clear(r.p.currentPattern());
      for (auto& p : r.p.presets(C_PATTERN)) if (!p.factory) CHECK(r.p.loadPreset(p, 0)); CHECK(r.p.bank().serialise(r.p.currentPattern()) == g1);
      juce::String bad; CHECK(!r.p.saveUserPreset("   ", C_KIT, 0, bad) && bad.isNotEmpty()); dir.deleteRecursively(); }

    { std::puts("- MIDI export");
      Rig r; r.p.setCurrentProgram(1); int pat = r.p.currentPattern(); auto bytes = r.p.patternMidi(pat);
      juce::MemoryInputStream in(bytes.data(), bytes.size(), false); juce::MidiFile mf; CHECK(mf.readFrom(in)); CHECK(mf.getTimeFormat() == 960);
      int ons = 0, onCh10 = 0; for (int t = 0; t < mf.getNumTracks(); ++t) for (auto* ev : *mf.getTrack(t)) if (ev->message.isNoteOn()) { ++ons; onCh10 += ev->message.getChannel() == 10; }
      int expected = 0; for (int t = 0; t < kChannels; ++t) for (int s = 0; s < r.p.bank().length(pat); ++s) { auto st = r.p.bank().get(pat, t, s); if (st.on) expected += st.ratchet; }
      CHECKM(ons == expected && onCh10 == ons, "ons %d expected %d", ons, expected);
      auto f = tempDir("midi").getChildFile("p.mid"); CHECK(r.p.saveMidi(pat, f) && f.getSize() == (juce::int64)bytes.size()); }

    { std::puts("- MIDI learn + macros");
      Rig r; r.seqOff(); int target = cp(0, V_DEC); r.p.startLearn(target); CHECK(r.p.learning() == target);
      auto cc = [](int num, int val) { return [=](int64_t pos, juce::MidiBuffer& m) { if (pos == 0) m.addEvent(juce::MidiMessage::controllerEvent(1, num, val), 0); }; };
      r.run(0.01, cc(74, 100)); r.p.processControlQueue(); CHECK(r.p.ccFor(target) == 74 && r.p.learning() == -1); CHECK(std::abs(r.p.param(target)->getValue() - 100 / 127.f) < 1e-3f);
      r.run(0.01, cc(74, 0)); r.p.processControlQueue(); CHECK(r.p.param(target)->getValue() < 1e-3f);
      r.run(0.01, cc(75, 127)); r.p.processControlQueue(); CHECK(r.p.param(target)->getValue() < 1e-3f); // unmapped CC does nothing
      r.p.clearLearn(target); CHECK(r.p.ccFor(target) == -1);
      int cut = cp(1, V_CUT); r.p.param(cut)->setValueNotifyingHost(0.3f); r.p.linkMacro(0, cut); r.p.processControlQueue();
      r.set(gp(G_MACRO1), 1.0f); r.p.processControlQueue(); CHECKM(std::abs(r.p.param(cut)->getValue() - 0.8f) < 1e-3f, "%f", r.p.param(cut)->getValue());
      r.set(gp(G_MACRO1), 0.0f); r.p.processControlQueue(); CHECK(r.p.param(cut)->getValue() < 1e-3f);
      r.p.unlinkMacros(cut); r.set(gp(G_MACRO1), 0.7f); r.p.processControlQueue(); CHECK(r.p.param(cut)->getValue() < 1e-3f); }

    { std::puts("- random generator via the plugin");
      Rig r; for (int m = 0; m < 7; ++m) {
          r.set(gp(G_RNDMODE), (float)m); r.p.randomKit(); r.p.randomPattern(); CHECK(r.p.bank().hits(r.p.currentPattern()) > 0);
          r.head.playing = true; r.head.ppq = 0; r.run(2.0); float pk = std::max(r.peak(0), r.peak(1)); CHECKM(pk > 0.02f && pk <= 1.0f, "mode %d peak %f", m, pk); CHECK(r.finite()); r.head.playing = false;
      }
      r.p.locks.sound[0] = true; std::vector<float> before(kChanParams); for (int k = 0; k < kChanParams; ++k) before[(size_t)k] = r.p.value(cp(0, k));
      r.p.randomKit(); int kept = 0; for (int k = 0; k < kChanParams; ++k) kept += std::abs(before[(size_t)k] - r.p.value(cp(0, k))) < 1e-6f; CHECK(kept == kChanParams);
      r.p.patternVariation(); CHECK(r.p.bank().hits(r.p.currentPattern()) > 0); }

    { std::puts("- block sizes and sample rates");
      for (double sr : {44100.0, 48000.0, 88200.0, 96000.0, 192000.0}) for (int bs : {1, 32, 128, 480, 1024, 4096}) {
          if (bs == 1 && sr > 50000) continue;
          Rig r(sr, bs); r.p.setCurrentProgram(1); r.head.playing = true; r.run(bs == 1 ? 0.3 : 1.0); float pk = std::max(r.peak(0), r.peak(1));
          CHECKM(r.finite() && pk > 0.01f && pk <= 1.0f, "sr %.0f bs %d peak %f", sr, bs, pk);
      }
      // varying block sizes inside one run (FL does this)
      Rig v; v.p.setCurrentProgram(1); v.head.playing = true; juce::Random jr(5); for (int i = 0; i < 400; ++i) v.run(0.0, {}, false); 
      for (int i = 0; i < 300; ++i) { int bs = 1 + jr.nextInt(700); v.run(bs / v.sr, {}, true, bs); CHECK(v.finite()); } }

    { std::puts("- CPU");
      for (int routed = 0; routed < 2; ++routed) {
          Rig r(48000, 256); r.p.setCurrentProgram(4); if (routed) for (int c = 0; c < kChannels; ++c) r.set(cp(c, M_OUT), (float)(c + 1));
          for (int c = 0; c < kChannels; ++c) { r.set(cp(c, F1_TYPE), FX_DIST); r.set(cp(c, F2_TYPE), FX_COMP); }
          r.p.bank().clear(r.p.currentPattern()); for (int t = 0; t < kChannels; ++t) for (int s = 0; s < 16; ++s) r.p.bank().set(r.p.currentPattern(), t, s, Step{1, 110, 100, 0, 1});
          r.head.playing = true; r.head.bpm = 174; double secs = 20; double t = r.run(secs, {}, false);
          std::printf("  CPU %s: all 12 channels on every 1/16 at 174 BPM with 2 FX each: %.2f%% of one core (%.0fx realtime)\n", routed ? "(12 separate outputs)" : "(main out)", 100 * t / secs, secs / t);
          CHECK(t / secs < 0.5);
      }
      Rig r(48000, 256); r.p.setCurrentProgram(1); r.head.playing = true; double t = r.run(20, {}, false); std::printf("  CPU 909 House kit pattern: %.2f%% of one core\n", 100 * t / 20); }

    { std::puts("- editor");
      Rig r; r.p.setCurrentProgram(1); r.head.playing = true; r.run(0.5);
      juce::String e; r.p.loadSample(0, writeTestWav(0.4), e);
      std::unique_ptr<juce::AudioProcessorEditor> ed(r.p.createEditor()); auto* be = dynamic_cast<BakeryEditor*>(ed.get()); CHECK(be != nullptr);
      if (be) {
          CHECK(be->getWidth() == 1360 && be->getHeight() == 860); be->setVisible(true);
          const char* names[3] = {"BAKERY-ui-1-drum-design.png", "BAKERY-ui-2-sequencer.png", "BAKERY-ui-3-kit-mixer.png"};
          for (int w = 0; w < 3; ++w) { be->showWorkspace(w); CHECK(be->workspace() == w); for (int i = 0; i < 4; ++i) { r.run(0.05, {}, false); be->tick(); } savePng(*be, names[w]); }
          be->showWorkspace(0); for (int c = 0; c < kChannels; ++c) { r.p.uiChannel = c; be->tick(); } // every channel rebinding works
          for (int e2 = 0; e2 < E_COUNT; ++e2) { r.p.setEngine(3, e2); r.p.uiChannel = 3; be->tick(); }
          be->showPresets(true); be->tick(); savePng(*be, "BAKERY-ui-4-preset-browser.png"); be->showPresets(false);
          be->setSize(1700, 1075); be->tick(); CHECK(be->getWidth() == 1700); be->setSize(680, 430); be->tick(); be->setSize(1360, 860);
          CHECK(std::abs(r.p.uiScale - 1.f) < 0.01f);
          // key display, Keep in key, Mode label
          be->showWorkspace(0); r.set(cp(0, V_ENGINE), (float)E_KICK); r.set(cp(0, V_FREQ), 50.f); r.set(cp(0, V_TUNE), 0.f); r.set(cp(0, V_SWEEP), 36.f); be->tick();
          auto note = be->design().noteText(); CHECKM(note.contains("G1"), "note was %s", note.toRawUTF8());
          r.set(gp(G_KEY), 4.f); r.set(gp(G_SCALE), (float)SC_MINOR); r.set(gp(G_SNAPMODE), 0.f); r.set(gp(G_KEEPKEY), 1.f); r.p.syncKeepInKey(); be->tick();
          note = be->design().noteText(); CHECKM(note.contains("G1"), "snapped note %s", note.toRawUTF8());
          CHECK(std::abs(noteCentsFromHz(settledHz((float)r.p.value(cp(0, V_FREQ)), (float)r.p.value(cp(0, V_TUNE)))).cents) < 1.f);
          savePng(*be, "BAKERY-ui-5-key-display.png");
          be->showWorkspace(1); be->tick(); CHECK(be->sequencer().modeCaption() == "MODE");
          savePng(*be, "BAKERY-ui-6-mode-label.png");
          // editor reopen restores workspace
          be->showWorkspace(2); ed.reset(); std::unique_ptr<juce::AudioProcessorEditor> ed2(r.p.createEditor()); auto* b2 = dynamic_cast<BakeryEditor*>(ed2.get()); CHECK(b2 && b2->workspace() == 2);
      } }

    std::printf("PLUGIN TESTS: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
