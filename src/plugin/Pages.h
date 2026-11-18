// BAKERY — the three workspaces (DRUM DESIGN, SEQUENCER, KIT MIXER) and the preset browser.
#pragma once
#include "Widgets.h"
#include "Key.h"

namespace ui {
using namespace bk;

/// Shared editor context: the processor, the selected channel and a status line.
struct Ctx {
    BakeryProcessor& p;
    std::function<void(int)> selectChannel;
    std::function<void(const juce::String&)> status;
    int channel() const { return juce::jlimit(0, kChannels - 1, p.uiChannel); }
};

struct MidiDragButton : juce::TextButton {
    using juce::TextButton::TextButton;
    std::function<void()> beginDrag; bool started = false;
    void mouseDown(const juce::MouseEvent& e) override { started = false; TextButton::mouseDown(e); }
    void mouseDrag(const juce::MouseEvent& e) override { TextButton::mouseDrag(e); if (!started && e.getDistanceFromDragStart() > 5) { started = true; if (beginDrag) beginDrag(); } }
};

/// One effect slot: type selector + 5 knobs whose labels/units follow the effect + mix.
class FxPanel : public juce::Component {
public:
    explicit FxPanel(juce::String t) : title(std::move(t)) { addAndMakeVisible(type); for (auto& k : knobs) addAndMakeVisible(k); addAndMakeVisible(mix); }
    void bind(BakeryProcessor& p, int typeIndex, const juce::String& t = {}) {
        P = &p; base = typeIndex; if (t.isNotEmpty()) title = t;
        type.bind(&p, base, "Effect type for this slot. Off = bypassed.");
        mix.bind(&p, base + 6, "Mix", "Dry/wet balance of this effect.");
        lastType = -1; refresh(); repaint();
    }
    void refresh() {
        if (!P) return; type.refresh(); int t = (int)P->value(base);
        if (t != lastType) {
            lastType = t; const auto& m = fxMeta(t);
            for (int i = 0; i < 5; ++i) { knobs[(size_t)i].bind(P, base + 1 + i, m.label[i], juce::String(fxNames()[(size_t)t]) + ": " + m.label[i]); knobs[(size_t)i].setFx(base, i); knobs[(size_t)i].setVisible(t != FX_OFF && m.label[i][0] != 0); }
            mix.setVisible(t != FX_OFF); resized();
        }
        for (auto& k : knobs) if (k.isVisible()) k.refresh();
        mix.refresh();
    }
    void paint(juce::Graphics& g) override { drawModule(g, getLocalBounds(), title, lastType > 0 ? col::orange : col::rail); if (lastType == FX_OFF) { g.setColour(col::rail); g.setFont(font(12.f)); g.drawText("Choose an effect above", getLocalBounds().withTrimmedTop(54), juce::Justification::centredTop); } }
    void resized() override {
        auto r = getLocalBounds().reduced(8); r.removeFromTop(18); type.setBounds(r.removeFromTop(24).withWidth(std::min(r.getWidth(), 170))); r.removeFromTop(4);
        std::vector<Knob*> vis; for (auto& k : knobs) vis.push_back(&k); vis.push_back(&mix);
        int perRow = r.getWidth() >= 6 * 60 ? 6 : 3, kw = r.getWidth() / perRow, rows = (int)(vis.size() + perRow - 1) / perRow, kh = std::min(80, r.getHeight() / rows);
        for (size_t i = 0; i < vis.size(); ++i) vis[i]->setBounds(r.getX() + (int)(i % (size_t)perRow) * kw, r.getY() + (int)(i / (size_t)perRow) * kh, kw, kh);
    }
private:
    juce::String title; BakeryProcessor* P = nullptr; int base = 0, lastType = -1; Choice type; std::array<Knob, 5> knobs; Knob mix;
};

/// Offline-rendered picture of the selected channel's hit.
class WaveView : public juce::Component {
public:
    explicit WaveView(Ctx& c) : ctx(c) {}
    void refresh(bool force = false) {
        int ch = ctx.channel(); std::vector<float> v((size_t)kChanParams); for (int k = 0; k < kChanParams; ++k) v[(size_t)k] = ctx.p.value(cp(ch, k));
        auto* s = ctx.p.sample(ch);
        auto now = juce::Time::getMillisecondCounter();
        if (!force && v == last && s == lastSample) return;
        if (!force && now - lastRender < 90) return;
        last = v; lastSample = s; lastRender = now;
        const double sr = 22050; auto y = renderHit(v.data(), sr, (int)(sr * 2.5), 1.f, s);
        int end = 0; float pk = 0; for (int i = 0; i < (int)y.size(); ++i) { float a = std::abs(y[(size_t)i]); pk = std::max(pk, a); if (a > 0.001f) end = i; }
        end = std::max(end + 1, (int)(sr * 0.06)); data.assign(y.begin(), y.begin() + std::min((int)y.size(), end)); lengthMs = 1000.0 * end / sr; peak = pk; repaint();
    }
    void paint(juce::Graphics& g) override {
        auto r = getLocalBounds().toFloat(); g.setColour(col::black); g.fillRoundedRectangle(r, 4);
        g.setColour(col::line); for (int i = 1; i < 8; ++i) g.drawVerticalLine((int)(r.getX() + r.getWidth() * i / 8), r.getY(), r.getBottom());
        g.drawHorizontalLine((int)r.getCentreY(), r.getX(), r.getRight());
        if (data.empty()) return;
        int W = (int)r.getWidth(); float mid = r.getCentreY(), hh = r.getHeight() * 0.46f; juce::Path top, bot;
        for (int x = 0; x < W; ++x) {
            size_t a = data.size() * (size_t)x / (size_t)W, b = std::max(a + 1, data.size() * (size_t)(x + 1) / (size_t)W); float mx = 0, mn = 0;
            for (size_t i = a; i < b && i < data.size(); ++i) { mx = std::max(mx, data[i]); mn = std::min(mn, data[i]); }
            float px = r.getX() + (float)x; if (x == 0) { top.startNewSubPath(px, mid - mx * hh); bot.startNewSubPath(px, mid - mn * hh); } else { top.lineTo(px, mid - mx * hh); bot.lineTo(px, mid - mn * hh); }
        }
        g.setColour(col::orange); g.strokePath(top, juce::PathStrokeType(1.2f)); g.strokePath(bot, juce::PathStrokeType(1.2f));
        g.setColour(col::cream); g.setFont(font(11.f, true));
        g.drawText("length " + juce::String((int)std::lround(lengthMs)) + " ms", r.reduced(6, 3).toNearestInt(), juce::Justification::topRight);
    }
private:
    Ctx& ctx; std::vector<float> last, data; const SampleData* lastSample = nullptr; juce::uint32 lastRender = 0; double lengthMs = 0; float peak = 0;
};

/// Channel slot in the left rack: pad (audition), LED, engine name, sound lock.
class ChannelSlot : public juce::Component {
public:
    ChannelSlot(Ctx& c, int index) : ctx(c), ch(index) {
        pad.setButtonText(juce::String(ch + 1)); pad.setTooltip("Play this sound (also selects it)");
        pad.onClick = [this] { ctx.p.audition(ch); ctx.selectChannel(ch); }; addAndMakeVisible(pad);
        lock.setButtonText("LOCK"); lock.setClickingTogglesState(true); lock.setTooltip("Lock this sound: Random Kit leaves it alone");
        lock.setColour(juce::TextButton::buttonOnColourId, col::blue);
        lock.onClick = [this] { ctx.p.locks.sound[ch] = lock.getToggleState(); }; addAndMakeVisible(lock); addAndMakeVisible(led);
    }
    void refresh(bool selected) {
        lock.setToggleState(ctx.p.locks.sound[ch], juce::dontSendNotification);
        auto e = engineName((int)ctx.p.value(cp(ch, V_ENGINE))); juce::String sm = ctx.p.sample(ch) ? juce::String(" + ") + ctx.p.sample(ch)->name : juce::String();
        juce::String t = juce::String(e) + sm;
        if (t != eng || selected != sel) { eng = t; sel = selected; repaint(); }
        uint32_t h = ctx.p.machine().hits[ch].load(); if (h != lastHits) { lastHits = h; led.hit(); } else led.decay();
    }
    void paint(juce::Graphics& g) override {
        auto r = getLocalBounds().toFloat().reduced(1);
        g.setColour(sel ? col::panelHi.brighter(0.08f) : col::panel); g.fillRoundedRectangle(r, 4);
        g.setColour(sel ? col::orange : col::line); g.drawRoundedRectangle(r, 4, sel ? 2.f : 1.f);
        auto t = getLocalBounds().reduced(6, 4).withTrimmedLeft(50);
        g.setColour(col::cream); g.setFont(font(14.f, true)); g.drawText(channelName(ch), t.removeFromTop(20), juce::Justification::centredLeft);
        g.setColour(sel ? col::orange : col::creamDim); g.setFont(font(11.5f)); g.drawText(eng, t.removeFromTop(16).withTrimmedRight(50), juce::Justification::centredLeft, true);
    }
    void resized() override {
        auto r = getLocalBounds().reduced(6); pad.setBounds(r.removeFromLeft(40)); led.setBounds(r.getRight() - 12, r.getY() + 2, 10, 10);
        lock.setBounds(r.getRight() - 46, r.getBottom() - 20, 46, 20);
    }
    void mouseDown(const juce::MouseEvent&) override { ctx.selectChannel(ch); }
private:
    Ctx& ctx; int ch; juce::TextButton pad, lock; Led led; juce::String eng; bool sel = false; uint32_t lastHits = 0;
};

/// Sample layer: drop zone, load/clear, waveform and the sample knobs.
class SamplePanel : public juce::Component, public juce::FileDragAndDropTarget {
public:
    explicit SamplePanel(Ctx& c) : ctx(c) {
        for (auto* b : {&load, &clear}) addAndMakeVisible(b);
        load.setTooltip("Load a WAV or AIFF file into this sound (or drag a file onto this panel)"); clear.setTooltip("Remove the sample from this sound");
        load.onClick = [this] {
            chooser = std::make_unique<juce::FileChooser>("Choose a sample", juce::File::getSpecialLocation(juce::File::userMusicDirectory), "*.wav;*.aif;*.aiff");
            chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe = juce::Component::SafePointer<SamplePanel>(this)](const juce::FileChooser& fc) {
                if (safe && fc.getResult() != juce::File()) safe->loadFile(fc.getResult()); });
        };
        clear.onClick = [this] { ctx.p.clearSample(ctx.channel()); ctx.status("Sample removed"); };
        const std::pair<int, const char*> ks[] = {{S_LEVEL, "Level"}, {S_SYNTH, "Synth"}, {S_START, "Start"}, {S_END, "End"}, {S_PITCH, "Pitch"}, {S_STRETCH, "Stretch"}, {S_CUT, "Cutoff"}, {S_ATT, "Attack"}, {S_DEC, "Decay"}};
        for (auto& [k, l] : ks) { keys.push_back(k); labels.push_back(l); knobs.push_back(std::make_unique<Knob>()); addAndMakeVisible(*knobs.back()); }
        addAndMakeVisible(rev); addAndMakeVisible(loop);
    }
    void bind() {
        int ch = ctx.channel();
        const char* tips[] = {"Sample volume.", "Volume of the synth engine under the sample (0 = sample only).", "Where playback starts in the file.", "Where playback ends.", "Repitch the sample in semitones.",
            "Granular time-stretch: change length without changing pitch.", "Low-pass filter on the sample.", "Fade-in of the sample.", "Fade-out time of the sample (max = hold to the end)."};
        for (size_t i = 0; i < knobs.size(); ++i) knobs[i]->bind(&ctx.p, cp(ch, keys[i]), labels[i], tips[i]);
        rev.bind(&ctx.p, cp(ch, S_REV), "Play the sample backwards"); loop.bind(&ctx.p, cp(ch, S_LOOP), "Loop between Start and End while the sound lasts");
    }
    void refresh() {
        for (auto& k : knobs) k->refresh(); rev.refresh(); loop.refresh();
        auto* s = ctx.p.sample(ctx.channel()); if (s != shownPtr) { shownPtr = s; repaint(); }
    }
    void loadFile(const juce::File& f) {
        juce::String err; bool ok = ctx.p.loadSample(ctx.channel(), f, err);
        ctx.status(ok ? (err.isNotEmpty() ? err : "Loaded " + f.getFileName() + " into " + channelName(ctx.channel())) : err);
    }
    bool isInterestedInFileDrag(const juce::StringArray& files) override { for (auto& f : files) if (f.endsWithIgnoreCase(".wav") || f.endsWithIgnoreCase(".aif") || f.endsWithIgnoreCase(".aiff")) return true; return false; }
    void fileDragEnter(const juce::StringArray&, int, int) override { hover = true; repaint(); }
    void fileDragExit(const juce::StringArray&) override { hover = false; repaint(); }
    void filesDropped(const juce::StringArray& files, int, int) override { hover = false; repaint(); for (auto& f : files) if (isInterestedInFileDrag({f})) { loadFile(juce::File(f)); break; } }
    void paint(juce::Graphics& g) override {
        drawModule(g, getLocalBounds(), "SAMPLE LAYER");
        g.setColour(hover ? col::orange.withAlpha(0.25f) : col::black); g.fillRoundedRectangle(wave.toFloat(), 4);
        g.setColour(hover ? col::orange : col::line); g.drawRoundedRectangle(wave.toFloat(), 4, 1);
        auto* shown = ctx.p.sample(ctx.channel());
        if (!shown) { g.setColour(col::creamDim); g.setFont(font(12.f)); g.drawText("Drop a WAV/AIFF here or press LOAD", wave, juce::Justification::centred); return; }
        float mid = (float)wave.getCentreY(), hh = wave.getHeight() * 0.45f; int W = wave.getWidth(); g.setColour(col::cream);
        int n = shown->frames();
        for (int x = 0; x < W; ++x) { int a = (int)((int64_t)n * x / W), b = std::max(a + 1, (int)((int64_t)n * (x + 1) / W)); float m = 0; for (int i = a; i < b && i < n; ++i) m = std::max(m, std::abs(shown->L[(size_t)i])); g.drawVerticalLine(wave.getX() + x, mid - m * hh, mid + m * hh + 1); }
        auto st = (float)ctx.p.value(cp(ctx.channel(), S_START)), en = (float)ctx.p.value(cp(ctx.channel(), S_END));
        g.setColour(col::orange); g.drawVerticalLine(wave.getX() + (int)(st * W), (float)wave.getY(), (float)wave.getBottom()); g.drawVerticalLine(wave.getX() + (int)(en * (W - 1)), (float)wave.getY(), (float)wave.getBottom());
        g.setColour(col::cream); g.setFont(font(11.f, true)); g.drawText(juce::String(shown->name) + "  " + juce::String(n / shown->rate, 2) + " s", wave.reduced(5, 2), juce::Justification::topLeft, true);
    }
    void resized() override {
        auto r = getLocalBounds().reduced(8); r.removeFromTop(18);
        auto row = r.removeFromTop(24); load.setBounds(row.removeFromLeft(70)); row.removeFromLeft(4); clear.setBounds(row.removeFromLeft(60)); row.removeFromLeft(10);
        rev.setBounds(row.removeFromLeft(80)); row.removeFromLeft(4); loop.setBounds(row.removeFromLeft(60));
        r.removeFromTop(6); wave = r.removeFromTop(64); r.removeFromTop(4);
        int kw = r.getWidth() / 5, kh = r.getHeight() / 2;
        for (size_t i = 0; i < knobs.size(); ++i) knobs[i]->setBounds(r.getX() + (int)(i % 5) * kw, r.getY() + (int)(i / 5) * kh, kw, kh);
        repaint();
    }
private:
    Ctx& ctx; juce::TextButton load{"LOAD"}, clear{"CLEAR"}; Toggle rev{"REVERSE"}, loop{"LOOP"};
    std::vector<int> keys; std::vector<juce::String> labels; std::vector<std::unique_ptr<Knob>> knobs;
    juce::Rectangle<int> wave; const SampleData* shownPtr = nullptr; bool hover = false; std::unique_ptr<juce::FileChooser> chooser;
};

// =====================================================================================================
class DesignPage : public juce::Component {
public:
    explicit DesignPage(Ctx& c) : ctx(c), wave(c), samplePanel(c), fx1("VOICE FX 1"), fx2("VOICE FX 2") {
        for (int i = 0; i < kChannels; ++i) { slots.push_back(std::make_unique<ChannelSlot>(ctx, i)); addAndMakeVisible(*slots.back()); }
        for (int e = 0; e < E_COUNT; ++e) {
            auto b = std::make_unique<juce::TextButton>(juce::String(engineName(e)).toUpperCase()); b->setClickingTogglesState(false); b->setTooltip(engineBlurb(e));
            b->onClick = [this, e] { ctx.p.setEngine(ctx.channel(), e); ctx.status(juce::String(engineName(e)) + " module loaded into " + channelName(ctx.channel())); bindChannel(); ctx.p.audition(ctx.channel()); };
            addAndMakeVisible(*b); engines.push_back(std::move(b));
        }
        addAndMakeVisible(wave); addAndMakeVisible(samplePanel); addAndMakeVisible(fx1); addAndMakeVisible(fx2);
        for (int i = 0; i < 4; ++i) { out[(size_t)i] = std::make_unique<Knob>(); addAndMakeVisible(*out[(size_t)i]); }
        for (int i = 0; i < 8; ++i) { macros[(size_t)i] = std::make_unique<Knob>(); addAndMakeVisible(*macros[(size_t)i]); }
        for (int s = 0; s < SEC_COUNT; ++s) for (int i = 0; i < 9; ++i) { auto k = std::make_unique<Knob>(); addChildComponent(*k); sec[(size_t)s].push_back(std::move(k)); }
        audition.onClick = [this] { ctx.p.audition(ctx.channel()); }; audition.setTooltip("Play the selected sound"); addAndMakeVisible(audition);
        rnd.onClick = [this] { ctx.p.randomSound(ctx.channel()); bindChannel(); ctx.p.audition(ctx.channel()); ctx.status("New random sound on " + juce::String(channelName(ctx.channel()))); };
        rnd.setTooltip("Random sound for this channel using the Random mode and intensity at the top. Locked knobs stay."); addAndMakeVisible(rnd);
        noteReadout.setFont(font(16.f, true)); noteReadout.setColour(juce::Label::textColourId, col::cream);
        noteReadout.setJustificationType(juce::Justification::centredLeft);
        noteReadout.setTooltip("Settled body pitch of this drum (concert pitch, A4 = 440 Hz, C4 = middle C). The sweep click is ignored.");
        addAndMakeVisible(noteReadout);
        keyBox.bind(&ctx.p, gp(G_KEY), "Song key. Keep in key uses this for kicks, toms and impacts.");
        scaleBox.bind(&ctx.p, gp(G_SCALE), "Scale for Keep in key (Major, Minor, Chromatic, pentatonic).");
        snapBox.bind(&ctx.p, gp(G_SNAPMODE), "Nearest = closest scale note. Root = every pitched drum goes to the key's root (e.g. all to E in E minor).");
        keepBtn.bind(&ctx.p, gp(G_KEEPKEY), "When on, kicks, toms and impacts snap to the scale (or to the root). Sweep punch is left alone.");
        addAndMakeVisible(keyBox); addAndMakeVisible(scaleBox); addAndMakeVisible(snapBox); addAndMakeVisible(keepBtn);
        for (int m = 0; m < 8; ++m) macros[(size_t)m]->bind(&ctx.p, gp(G_MACRO1 + m), "Macro " + juce::String(m + 1), "Macro knob: right-click any knob and choose Link to Macro, then this knob moves it.");
        bindChannel();
    }
    void bindChannel() {
        int ch = ctx.channel(), e = (int)ctx.p.value(cp(ch, V_ENGINE)); boundEngine = e; boundChannel = ch;
        auto ctl = engineControls(e); std::array<int, SEC_COUNT> used{};
        for (auto& s : sec) for (auto& k : s) k->setVisible(false);
        for (auto& c : ctl) { int s = c.section; if (used[(size_t)s] >= 9) continue; auto& k = *sec[(size_t)s][(size_t)used[(size_t)s]++]; k.bind(&ctx.p, cp(ch, c.key), c.label, c.tip); k.setVisible(true); }
        secUsed = used;
        const std::pair<int, const char*> o[] = {{V_LEVEL, "Level"}, {V_VSENS, "Vel Sens"}, {V_NOTE, "MIDI Note"}, {V_CHOKE, "Choke"}};
        const char* tips[] = {"Voice level before the mixer (auto-set when you pick a preset so sounds sit at a similar loudness).", "How much MIDI/step velocity changes the volume.", "MIDI note that plays this sound (C1 = 36 is the kick).", "Choke group: sounds in the same group cut each other off."};
        for (int i = 0; i < 4; ++i) out[(size_t)i]->bind(&ctx.p, cp(ch, o[i].first), o[i].second, tips[i]);
        samplePanel.bind(); fx1.bind(ctx.p, cp(ch, F1_TYPE)); fx2.bind(ctx.p, cp(ch, F2_TYPE));
        wave.refresh(true); resized(); repaint();
    }
    void refresh() {
        int ch = ctx.channel();
        if (ch != boundChannel || (int)ctx.p.value(cp(ch, V_ENGINE)) != boundEngine) bindChannel();
        for (int i = 0; i < kChannels; ++i) slots[(size_t)i]->refresh(i == ch);
        for (int e = 0; e < E_COUNT; ++e) engines[(size_t)e]->setToggleState(e == boundEngine, juce::dontSendNotification);
        for (auto& s : sec) for (auto& k : s) if (k->isVisible()) k->refresh();
        for (auto& k : out) k->refresh(); for (auto& k : macros) k->refresh();
        samplePanel.refresh(); fx1.refresh(); fx2.refresh(); wave.refresh();
        keyBox.refresh(); scaleBox.refresh(); snapBox.refresh(); keepBtn.refresh();
        int e = boundEngine;
        if (engineShowsKey(e)) {
            float hz = settledHz((float)ctx.p.value(cp(ch, V_FREQ)), (float)ctx.p.value(cp(ch, V_TUNE)));
            noteReadout.setText(juce::String(formatNoteCents(noteCentsFromHz(hz))), juce::dontSendNotification);
            noteReadout.setColour(juce::Label::textColourId, col::cream);
        } else {
            noteReadout.setText("-", juce::dontSendNotification);
            noteReadout.setColour(juce::Label::textColourId, col::creamDim);
        }
    }
    void paint(juce::Graphics& g) override {
        drawModule(g, browserArea, "MODULE  -  " + juce::String(channelName(ctx.channel())).toUpperCase());
        drawModule(g, waveArea, "SOUND");
        g.setColour(col::creamDim); g.setFont(font(11.f)); g.drawFittedText(engineBlurb(boundEngine), blurbArea, juce::Justification::topLeft, 1);
        g.setColour(col::creamDim); g.setFont(font(9.5f, true));
        g.drawText("NOTE", 244, 188, 80, 12, juce::Justification::centredLeft);
        g.drawText("KEY", 428, 188, 58, 12, juce::Justification::centredLeft);
        g.drawText("SCALE", 490, 188, 80, 12, juce::Justification::centredLeft);
        g.drawText("SNAP", 678, 188, 78, 12, juce::Justification::centredLeft);
        for (int s = 0; s < SEC_COUNT; ++s) drawModule(g, secArea[(size_t)s], sectionName(s), secUsed[(size_t)s] ? col::orange : col::rail);
        drawModule(g, macroArea, "MACROS");
    }
    void resized() override {
        for (int i = 0; i < kChannels; ++i) slots[(size_t)i]->setBounds(8, 8 + i * 63, 220, 60);
        browserArea = {236, 8, 692, 84};
        for (int e = 0; e < E_COUNT; ++e) engines[(size_t)e]->setBounds(244 + (e % 7) * 97, 32 + (e / 7) * 29, 93, 26);
        waveArea = {236, 98, 692, 130}; wave.setBounds(244, 118, 400, 68); blurbArea = {650, 100, 270, 16};
        for (int i = 0; i < 4; ++i) out[(size_t)i]->setBounds(650 + i * 64, 114, 64, 72);
        noteReadout.setBounds(244, 200, 180, 26);
        keyBox.setBounds(428, 200, 58, 26); scaleBox.setBounds(490, 200, 80, 26);
        keepBtn.setBounds(574, 200, 100, 26); snapBox.setBounds(678, 200, 78, 26);
        audition.setBounds(760, 200, 70, 26); rnd.setBounds(834, 200, 86, 26);
        for (int s = 0; s < SEC_COUNT; ++s) {
            auto a = juce::Rectangle<int>(236 + (s % 3) * 233, 234 + (s / 3) * 276, 226, 270); secArea[(size_t)s] = a;
            for (int i = 0; i < 9; ++i) sec[(size_t)s][(size_t)i]->setBounds(a.getX() + 6 + (i % 3) * 72, a.getY() + 24 + (i / 3) * 81, 70, 80);
        }
        samplePanel.setBounds(936, 8, 416, 290); fx1.setBounds(936, 304, 416, 150); fx2.setBounds(936, 460, 416, 150);
        macroArea = {936, 616, 416, 166};
        for (int m = 0; m < 8; ++m) macros[(size_t)m]->setBounds(944 + (m % 4) * 101, 638 + (m / 4) * 70, 98, 70);
    }
    SamplePanel& samples() { return samplePanel; }
    juce::String noteText() const { return noteReadout.getText(); }
private:
    Ctx& ctx; std::vector<std::unique_ptr<ChannelSlot>> slots; std::vector<std::unique_ptr<juce::TextButton>> engines;
    WaveView wave; SamplePanel samplePanel; FxPanel fx1, fx2; std::array<std::unique_ptr<Knob>, 4> out; std::array<std::unique_ptr<Knob>, 8> macros;
    std::array<std::vector<std::unique_ptr<Knob>>, SEC_COUNT> sec; std::array<int, SEC_COUNT> secUsed{}; std::array<juce::Rectangle<int>, SEC_COUNT> secArea;
    juce::TextButton audition{"PLAY SOUND"}, rnd{"RANDOM SOUND"};
    juce::Label noteReadout; Choice keyBox, scaleBox, snapBox; Toggle keepBtn{"KEEP IN KEY"};
    juce::Rectangle<int> browserArea, waveArea, blurbArea, macroArea; int boundEngine = -1, boundChannel = -1;
};

// =====================================================================================================
/// The step grid: 12 tracks x 32 visible steps. Click = on/off (drag paints), Shift = accent, Alt or right-click = ratchet.
class StepGrid : public juce::Component {
public:
    explicit StepGrid(Ctx& c) : ctx(c) {}
    int page = 0; static constexpr int kVis = 32, kLabel = 0;
    int stepAt(int x) const { return juce::jlimit(0, kVis - 1, x * kVis / std::max(1, getWidth())); }
    int rowAt(int y) const { return juce::jlimit(0, kChannels - 1, y * kChannels / std::max(1, getHeight())); }
    void paint(juce::Graphics& g) override {
        auto& b = ctx.p.bank(); int pat = ctx.p.currentPattern(), len = b.length(pat);
        float cw = getWidth() / (float)kVis, rh = getHeight() / (float)kChannels; int ps = ctx.p.machine().playStep.load(), pp = ctx.p.machine().playPattern.load();
        g.fillAll(col::black);
        for (int t = 0; t < kChannels; ++t) for (int v = 0; v < kVis; ++v) {
            int s = page * kVis + v; auto st = b.get(pat, t, s); auto r = juce::Rectangle<float>(v * cw, t * rh, cw, rh).reduced(2.f, 2.5f);
            bool inLen = s < len, beat = (s / 4) % 2 == 0;
            juce::Colour off = inLen ? (beat ? juce::Colour(0xff34363d) : juce::Colour(0xff26282d)) : juce::Colour(0xff151518);
            if (t == ctx.channel()) off = off.brighter(0.12f);
            g.setColour(off); g.fillRoundedRectangle(r, 3);
            if (st.on) {
                float a = 0.45f + 0.55f * st.vel / 127.f; auto c = (st.accent ? col::orange : col::cream).withAlpha(inLen ? a : 0.25f);
                g.setColour(c); g.fillRoundedRectangle(r, 3);
                if (st.prob < 100) { g.setColour(col::black.withAlpha(0.55f)); g.fillRect(r.withTrimmedTop(r.getHeight() * st.prob / 100.f)); }
                if (st.ratchet > 1) { g.setColour(col::black); for (int k = 0; k < st.ratchet; ++k) g.fillEllipse(r.getX() + 3 + k * 6.f, r.getY() + 3, 4, 4); }
            }
            if (s == ps && pp == pat) { g.setColour(col::green.withAlpha(0.9f)); g.drawRoundedRectangle(r.expanded(1), 3, 2); }
        }
    }
    void mouseDown(const juce::MouseEvent& e) override {
        int t = rowAt(e.y), s = page * kVis + stepAt(e.x), pat = ctx.p.currentPattern(); auto& b = ctx.p.bank(); auto st = b.get(pat, t, s);
        ctx.selectChannel(t);
        if (e.mods.isPopupMenu() || e.mods.isAltDown()) { st.on = 1; st.ratchet = (uint8_t)(st.ratchet % 4 + 1); b.set(pat, t, s, st); mode = -1; repaint(); return; }
        if (e.mods.isShiftDown()) { st.accent = st.accent ? 0 : 1; st.on = 1; b.set(pat, t, s, st); mode = -1; repaint(); return; }
        mode = st.on ? 0 : 1; row = t; st.on = (uint8_t)mode; b.set(pat, t, s, st); last = s; repaint();
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (mode < 0) return; int s = page * kVis + stepAt(e.x); if (s == last) return; last = s;
        auto& b = ctx.p.bank(); int pat = ctx.p.currentPattern(); auto st = b.get(pat, row, s); st.on = (uint8_t)mode; b.set(pat, row, s, st); repaint();
    }
private:
    Ctx& ctx; int mode = -1, row = 0, last = -1;
};

/// Velocity / probability / ratchet / accent lanes for the selected track.
class Lanes : public juce::Component {
public:
    explicit Lanes(Ctx& c) : ctx(c) {}
    int page = 0;
    static constexpr const char* names[4] = {"VELOCITY", "PROBABILITY", "RATCHET", "ACCENT"};
    juce::Rectangle<float> lane(int i) const { const float h[4] = {0.38f, 0.30f, 0.17f, 0.15f}; float y = 0; for (int k = 0; k < i; ++k) y += h[k]; return {0, y * getHeight(), (float)getWidth(), h[i] * getHeight()}; }
    void paint(juce::Graphics& g) override {
        auto& b = ctx.p.bank(); int pat = ctx.p.currentPattern(), t = ctx.channel(), len = b.length(pat); float cw = getWidth() / 32.f;
        g.fillAll(col::black);
        for (int l = 0; l < 4; ++l) {
            auto L = lane(l); g.setColour(col::line); g.drawHorizontalLine((int)L.getBottom() - 1, 0, (float)getWidth());
            for (int v = 0; v < 32; ++v) {
                int s = page * 32 + v; auto st = b.get(pat, t, s); auto r = juce::Rectangle<float>(v * cw, L.getY(), cw, L.getHeight()).reduced(3, 3);
                float frac = l == 0 ? st.vel / 127.f : l == 1 ? st.prob / 100.f : l == 2 ? st.ratchet / 4.f : (st.accent ? 1.f : 0.f);
                g.setColour(juce::Colour(0xff222429)); g.fillRect(r);
                auto c = (l == 3 ? col::orange : col::cream).withAlpha(st.on && s < len ? 1.f : 0.25f);
                g.setColour(c); g.fillRect(r.withTrimmedTop(r.getHeight() * (1 - frac)));
                if (l == 2 && st.ratchet > 1) { g.setColour(col::black); g.setFont(font(10.f, true)); g.drawText(juce::String(st.ratchet), r.toNearestInt(), juce::Justification::centred); }
            }
        }
    }
    void mouseDown(const juce::MouseEvent& e) override { for (int l = 0; l < 4; ++l) if (lane(l).contains(e.position)) active = l; edit(e, true); }
    void mouseDrag(const juce::MouseEvent& e) override { edit(e, false); }
private:
    void edit(const juce::MouseEvent& e, bool first) {
        if (active < 0) return; auto L = lane(active); int v = juce::jlimit(0, 31, (int)(e.x * 32 / std::max(1, getWidth()))), s = page * 32 + v;
        float frac = juce::jlimit(0.f, 1.f, 1.f - (e.position.y - L.getY()) / L.getHeight());
        auto& b = ctx.p.bank(); int pat = ctx.p.currentPattern(), t = ctx.channel(); auto st = b.get(pat, t, s);
        if (active == 0) st.vel = (uint8_t)juce::jlimit(1, 127, (int)std::lround(frac * 127));
        else if (active == 1) st.prob = (uint8_t)juce::jlimit(0, 100, (int)std::lround(frac * 100));
        else if (active == 2) st.ratchet = (uint8_t)juce::jlimit(1, 4, 1 + (int)(frac * 3.99f));
        else { if (first || s != lastAcc) st.accent = frac > 0.5f ? 1 : 0; lastAcc = s; }
        b.set(pat, t, s, st); repaint();
    }
    Ctx& ctx; int active = -1, lastAcc = -1;
};

class SequencerPage : public juce::Component {
public:
    explicit SequencerPage(Ctx& c) : ctx(c), grid(c), lanes(c) {
        for (int i = 0; i < kPatterns; ++i) {
            auto b = std::make_unique<juce::TextButton>(juce::String(i + 1)); b->setTooltip("Pattern " + juce::String(i + 1) + ". Patterns switch at the next step while playing.");
            b->onClick = [this, i] { ctx.p.setValue(gp(G_PATTERN), (float)(i + 1)); }; addAndMakeVisible(*b); slots.push_back(std::move(b));
        }
        for (int l = 1; l <= kMaxSteps; ++l) length.addItem(juce::String(l) + " steps", l);
        length.onChange = [this] { ctx.p.bank().setLength(ctx.p.currentPattern(), length.getSelectedId()); };
        length.setTooltip("Pattern length. Different lengths on different patterns + odd lengths make polyrhythms."); addAndMakeVisible(length);
        rate.bind(&ctx.p, gp(G_RATE), "Step length: 1/16 is standard; 1/16T = triplets."); mode.bind(&ctx.p, gp(G_SEQ), "Follow host = plays when FL plays. Host + internal = also plays from the PLAY button when FL is stopped. Off = MIDI only.");
        addAndMakeVisible(rate); addAndMakeVisible(mode);
        lengthLabel.setText("LENGTH", juce::dontSendNotification); rateLabel.setText("RATE", juce::dontSendNotification); modeLabel.setText("MODE", juce::dontSendNotification);
        for (auto* l : {&lengthLabel, &rateLabel, &modeLabel}) {
            l->setFont(font(9.5f, true)); l->setColour(juce::Label::textColourId, col::creamDim);
            l->setJustificationType(juce::Justification::centredLeft); l->setInterceptsMouseClicks(false, false); addAndMakeVisible(*l);
        }
        modeLabel.setTooltip("How the pattern plays: Follow host, Host + internal, or Off (MIDI only).");
        swing.bind(&ctx.p, gp(G_SWING), "Swing", "Delays every second step. 50 % = straight, 58-66 % = groovy."); human.bind(&ctx.p, gp(G_HUMAN), "Humanize", "Small random timing and velocity changes.");
        tempo.bind(&ctx.p, gp(G_TEMPO), "Tempo", "Internal tempo, used only when the host gives no tempo.");
        for (auto* k : {&swing, &human, &tempo}) addAndMakeVisible(*k);
        for (auto* b : {&copyB, &pasteB, &dupB, &varB, &rndB, &clearB, &page1, &page2}) addAndMakeVisible(*b);
        addAndMakeVisible(dragB); addAndMakeVisible(saveB);
        copyB.setTooltip("Copy this pattern"); pasteB.setTooltip("Paste the copied pattern here"); dupB.setTooltip("Copy this pattern into the next slot and switch to it");
        varB.setTooltip("Make a variation of this pattern in the next slot (fills, ghost notes, ratchets)"); rndB.setTooltip("Random pattern using the Random mode at the top (locked tracks stay)");
        clearB.setTooltip("Clear all steps of this pattern"); dragB.setTooltip("Click and DRAG this onto the FL playlist or piano roll to get the pattern as MIDI"); saveB.setTooltip("Save this pattern as a .mid file");
        copyB.onClick = [this] { clip = ctx.p.bank().serialise(ctx.p.currentPattern()); clipName = ctx.p.bank().names[ctx.p.currentPattern()]; ctx.status("Pattern copied"); };
        pasteB.onClick = [this] { if (clip.empty()) { ctx.status("Copy a pattern first"); return; } ctx.p.bank().deserialise(ctx.p.currentPattern(), clip); ctx.p.bank().names[ctx.p.currentPattern()] = clipName; ctx.status("Pattern pasted"); };
        dupB.onClick = [this] { int f = ctx.p.currentPattern(), t = (f + 1) % kPatterns; ctx.p.bank().copy(f, t); ctx.p.bank().names[t] = ctx.p.bank().names[f]; ctx.p.setValue(gp(G_PATTERN), (float)(t + 1)); ctx.status("Duplicated into pattern " + juce::String(t + 1)); };
        varB.onClick = [this] { ctx.p.patternVariation(); ctx.status("Variation written to pattern " + juce::String(ctx.p.currentPattern() + 1)); };
        rndB.onClick = [this] { ctx.p.randomPattern(); ctx.status("Random pattern"); };
        clearB.onClick = [this] { ctx.p.bank().clear(ctx.p.currentPattern()); ctx.p.bank().names[ctx.p.currentPattern()] = ""; ctx.status("Pattern cleared"); };
        page1.setClickingTogglesState(true); page2.setClickingTogglesState(true); page1.setRadioGroupId(77); page2.setRadioGroupId(77); page1.setToggleState(true, juce::dontSendNotification);
        page1.onClick = [this] { setPage(0); }; page2.onClick = [this] { setPage(1); };
        dragB.beginDrag = [this] {
            auto f = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("BAKERY Pattern " + juce::String(ctx.p.currentPattern() + 1) + ".mid");
            if (ctx.p.saveMidi(ctx.p.currentPattern(), f)) { juce::DragAndDropContainer::performExternalDragDropOfFiles({f.getFullPathName()}, false, &dragB); ctx.status("Drop the MIDI onto the FL playlist or a piano roll"); }
            else ctx.status("Could not prepare the MIDI file");
        };
        dragB.onClick = [this] { ctx.status("Click and drag this button onto FL to drop the pattern as MIDI"); };
        saveB.onClick = [this] {
            chooser = std::make_unique<juce::FileChooser>("Save pattern as MIDI", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("BAKERY Pattern.mid"), "*.mid");
            chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [safe = juce::Component::SafePointer<SequencerPage>(this)](const juce::FileChooser& fc) {
                if (safe && fc.getResult() != juce::File()) safe->ctx.status(safe->ctx.p.saveMidi(safe->ctx.p.currentPattern(), fc.getResult().withFileExtension("mid")) ? "MIDI saved" : "Could not save MIDI"); });
        };
        for (int t = 0; t < kChannels; ++t) {
            auto& r = rows[(size_t)t]; r.name = std::make_unique<juce::TextButton>(channelName(t)); r.name->onClick = [this, t] { ctx.selectChannel(t); }; r.name->setTooltip("Select this track (its lanes show below). Right-click a step for ratchets.");
            r.name->setColour(juce::TextButton::buttonOnColourId, col::orange);
            r.mute = std::make_unique<Toggle>("M", col::red); r.solo = std::make_unique<Toggle>("S", col::green); r.lock = std::make_unique<juce::TextButton>("L");
            r.mute->bind(ctx.p, cp(t, M_MUTE), "Mute"); r.solo->bind(ctx.p, cp(t, M_SOLO), "Solo");
            r.lock->setClickingTogglesState(true); r.lock->setColour(juce::TextButton::buttonOnColourId, col::blue); r.lock->setTooltip("Lock this track: Random Pattern keeps its steps");
            r.lock->onClick = [this, t] { ctx.p.locks.track[t] = rows[(size_t)t].lock->getToggleState(); };
            for (juce::Component* comp : {(juce::Component*)r.name.get(), (juce::Component*)r.mute.get(), (juce::Component*)r.solo.get(), (juce::Component*)r.lock.get()}) addAndMakeVisible(comp);
        }
        addAndMakeVisible(grid); addAndMakeVisible(lanes);
    }
    void setPage(int p) { grid.page = lanes.page = p; grid.repaint(); lanes.repaint(); ctx.p.uiStepPage = p; }
    juce::String modeCaption() const { return modeLabel.getText(); }
    void refresh() {
        int pat = ctx.p.currentPattern();
        for (int i = 0; i < kPatterns; ++i) { slots[(size_t)i]->setToggleState(i == pat, juce::dontSendNotification); slots[(size_t)i]->setColour(juce::TextButton::buttonColourId, ctx.p.bank().hits(i) ? col::panelHi.brighter(0.15f) : col::panelHi); }
        if (!length.isPopupActive() && length.getSelectedId() != ctx.p.bank().length(pat)) length.setSelectedId(ctx.p.bank().length(pat), juce::dontSendNotification);
        rate.refresh(); mode.refresh(); swing.refresh(); human.refresh(); tempo.refresh();
        for (int t = 0; t < kChannels; ++t) { auto& r = rows[(size_t)t]; r.mute->refresh(); r.solo->refresh(); r.lock->setToggleState(ctx.p.locks.track[t], juce::dontSendNotification); r.name->setToggleState(t == ctx.channel(), juce::dontSendNotification); }
        if (ctx.p.uiStepPage != grid.page) { setPage(ctx.p.uiStepPage); (ctx.p.uiStepPage ? page2 : page1).setToggleState(true, juce::dontSendNotification); }
        juce::String nm = ctx.p.bank().names[pat]; if (nm != shownName) { shownName = nm; repaint(); }
        grid.repaint(); lanes.repaint();
    }
    void paint(juce::Graphics& g) override {
        drawModule(g, {8, 8, 1344, 132}, "PATTERN"); drawModule(g, {8, 146, 1344, 420}, "STEPS"); drawModule(g, {8, 572, 1344, 210}, "TRACK LANES  -  " + juce::String(channelName(ctx.channel())).toUpperCase());
        g.setColour(col::cream); g.setFont(font(13.f, true)); g.drawText("Pattern " + juce::String(ctx.p.currentPattern() + 1) + (shownName.isNotEmpty() ? ": " + shownName : juce::String()), 16, 70, 210, 28, juce::Justification::centredLeft, true);
        g.setColour(col::creamDim); g.setFont(font(11.f)); g.drawText("Click = on/off (drag to paint)   Shift+click = accent   Right-click/Alt = ratchet (x2, x3, x4)", 16, 104, 700, 24, juce::Justification::centredLeft);
        float cw = 1080 / 32.f; g.setFont(font(10.f, true));
        for (int v = 0; v < 32; ++v) { int s = grid.page * 32 + v; g.setColour(s % 4 == 0 ? col::cream : col::rail); g.drawText(juce::String(s + 1), 264 + (int)(v * cw), 150, (int)cw, 14, juce::Justification::centred); }
        const char* ln[4] = {"VELOCITY", "PROBABILITY", "RATCHET", "ACCENT"}; const float lh[4] = {0.38f, 0.30f, 0.17f, 0.15f}; float y = 600;
        g.setFont(font(11.f, true)); for (int l = 0; l < 4; ++l) { g.setColour(col::cream); g.drawText(ln[l], 20, (int)y, 200, (int)(lh[l] * 172), juce::Justification::centredLeft); y += lh[l] * 172; }
    }
    void resized() override {
        for (int i = 0; i < kPatterns; ++i) slots[(size_t)i]->setBounds(16 + i * 54, 34, 50, 28);
        lengthLabel.setBounds(900, 18, 100, 14); rateLabel.setBounds(1006, 18, 80, 14); modeLabel.setBounds(1092, 18, 150, 14);
        length.setBounds(900, 34, 100, 26); rate.setBounds(1006, 34, 80, 26); mode.setBounds(1092, 34, 150, 26); page1.setBounds(1250, 34, 46, 26); page2.setBounds(1300, 34, 46, 26);
        int x = 232; for (auto* b : {&copyB, &pasteB, &dupB, &varB, &rndB, &clearB}) { b->setBounds(x, 70, 84, 28); x += 88; }
        dragB.setBounds(x + 6, 70, 110, 28); saveB.setBounds(x + 122, 70, 96, 28);
        swing.setBounds(1060, 62, 70, 76); human.setBounds(1134, 62, 70, 76); tempo.setBounds(1208, 62, 70, 76);
        for (int t = 0; t < kChannels; ++t) { auto& r = rows[(size_t)t]; int y = 168 + t * 32; r.name->setBounds(16, y + 2, 140, 28); r.mute->setBounds(160, y + 2, 30, 28); r.solo->setBounds(193, y + 2, 30, 28); r.lock->setBounds(226, y + 2, 30, 28); }
        grid.setBounds(264, 168, 1080, 384); lanes.setBounds(264, 600, 1080, 172);
    }
private:
    struct Row { std::unique_ptr<juce::TextButton> name, lock; std::unique_ptr<Toggle> mute, solo; };
    Ctx& ctx; StepGrid grid; Lanes lanes; std::vector<std::unique_ptr<juce::TextButton>> slots; juce::ComboBox length; Choice rate, mode; Knob swing, human, tempo;
    juce::Label lengthLabel, rateLabel, modeLabel;
    juce::TextButton copyB{"COPY"}, pasteB{"PASTE"}, dupB{"DUPLICATE"}, varB{"VARIATION"}, rndB{"RANDOM"}, clearB{"CLEAR"}, page1{"1-32"}, page2{"33-64"}, saveB{"SAVE .MID"};
    MidiDragButton dragB{"DRAG MIDI"}; std::array<Row, kChannels> rows; std::string clip, clipName; juce::String shownName; std::unique_ptr<juce::FileChooser> chooser;
};

// =====================================================================================================
class Fader : public juce::Component {
public:
    Fader() {
        s.setSliderStyle(juce::Slider::LinearVertical); s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 16); s.setRange(0, 1);
        s.onDragStart = [this] { if (p) p->beginChangeGesture(); drag = true; }; s.onDragEnd = [this] { if (p) p->endChangeGesture(); drag = false; };
        s.onValueChange = [this] { if (p && std::abs(p->getValue() - (float)s.getValue()) > 1e-7f) { if (!drag) p->beginChangeGesture(); p->setValueNotifyingHost((float)s.getValue()); if (!drag) p->endChangeGesture(); } };
        s.textFromValueFunction = [this](double v) { return p ? p->getText((float)v, 16) : juce::String(); };
        s.valueFromTextFunction = [this](const juce::String& t) { return p ? (double)p->getValueForText(t) : 0.0; };
        addAndMakeVisible(s); addAndMakeVisible(meter);
    }
    void bind(BakeryProcessor& P, int index, const juce::String& tip) { p = P.param(index); s.setDoubleClickReturnValue(true, p->getDefaultValue()); s.setTooltip(tip); s.setValue(p->getValue(), juce::dontSendNotification); s.updateText(); }
    void refresh(float level) { if (p && !drag && std::abs(s.getValue() - p->getValue()) > 1e-6) s.setValue(p->getValue(), juce::dontSendNotification); s.updateText(); meter.set(level); }
    void resized() override { auto r = getLocalBounds(); meter.setBounds(r.removeFromRight(10).withTrimmedBottom(18).reduced(0, 6)); s.setBounds(r); }
    juce::Slider s; Meter meter;
private:
    juce::RangedAudioParameter* p = nullptr; bool drag = false;
};

class MixerStrip : public juce::Component {
public:
    MixerStrip(Ctx& c, int index) : ctx(c), ch(index) {
        fader.bind(ctx.p, cp(ch, M_VOL), "Channel volume"); pan.bind(&ctx.p, cp(ch, M_PAN), "Pan", "Left/right position");
        sa.bind(&ctx.p, cp(ch, M_SENDA), "Send A", "Amount sent to the Send A effect (reverb by default)"); sb.bind(&ctx.p, cp(ch, M_SENDB), "Send B", "Amount sent to the Send B effect (delay by default)");
        out.bind(&ctx.p, cp(ch, M_OUT), "Where this channel goes: Main, or its own stereo output (Out 2-13) for a separate FL mixer track");
        mute.bind(ctx.p, cp(ch, M_MUTE), "Mute"); solo.bind(ctx.p, cp(ch, M_SOLO), "Solo"); phase.bind(ctx.p, cp(ch, M_PHASE), "Flip polarity (phase). Useful when layering two kicks.");
        for (juce::Component* comp : {(juce::Component*)&fader, (juce::Component*)&pan, (juce::Component*)&sa, (juce::Component*)&sb, (juce::Component*)&out, (juce::Component*)&mute, (juce::Component*)&solo, (juce::Component*)&phase}) addAndMakeVisible(comp);
        pad.onClick = [this] { ctx.p.audition(ch); ctx.selectChannel(ch); }; pad.setTooltip("Play this sound"); addAndMakeVisible(pad);
    }
    void refresh() {
        fader.refresh(ctx.p.machine().meter[ch].load()); pan.refresh(); sa.refresh(); sb.refresh(); out.refresh(); mute.refresh(); solo.refresh(); phase.refresh();
        bool s = ctx.channel() == ch; if (s != sel) { sel = s; repaint(); }
    }
    void paint(juce::Graphics& g) override {
        auto r = getLocalBounds(); drawModule(g, r, {}, col::orange); if (sel) { g.setColour(col::orange); g.drawRoundedRectangle(r.toFloat().reduced(1), 5, 2); }
        g.setColour(col::cream); g.setFont(font(12.f, true)); g.drawFittedText(juce::String(ch + 1) + " " + channelName(ch), r.reduced(4, 4).removeFromTop(16), juce::Justification::centred, 1);
        g.setColour(col::creamDim); g.setFont(font(10.5f)); g.drawFittedText(engineName((int)ctx.p.value(cp(ch, V_ENGINE))), r.reduced(4, 20).removeFromTop(14), juce::Justification::centred, 1);
    }
    void resized() override {
        auto r = getLocalBounds().reduced(5); r.removeFromTop(34); out.setBounds(r.removeFromTop(22)); r.removeFromTop(4);
        auto sends = r.removeFromTop(64); sa.setBounds(sends.removeFromLeft(sends.getWidth() / 2)); sb.setBounds(sends);
        pan.setBounds(r.removeFromTop(64)); auto bottom = r.removeFromBottom(24); int w = bottom.getWidth() / 3;
        mute.setBounds(bottom.removeFromLeft(w).reduced(1, 0)); solo.setBounds(bottom.removeFromLeft(w).reduced(1, 0)); phase.setBounds(bottom.reduced(1, 0));
        r.removeFromBottom(4); pad.setBounds(r.removeFromBottom(22)); r.removeFromBottom(4); fader.setBounds(r);
    }
private:
    Ctx& ctx; int ch; Fader fader; Knob pan, sa, sb; Choice out; Toggle mute{"M", col::red}, solo{"S", col::green}, phase{juce::String(juce::CharPointer_UTF8("\xc3\x98"))}; juce::TextButton pad{"PLAY"}; bool sel = false;
};

class MixerPage : public juce::Component {
public:
    explicit MixerPage(Ctx& c) : ctx(c), sendA("SEND A FX"), sendB("SEND B FX") {
        for (int i = 0; i < kChannels; ++i) { strips.push_back(std::make_unique<MixerStrip>(ctx, i)); addAndMakeVisible(*strips.back()); }
        for (int s = 0; s < kMasterSlots; ++s) { masterFx.push_back(std::make_unique<FxPanel>("MASTER FX " + juce::String(s + 1))); masterFx.back()->bind(ctx.p, mp(s, 0)); addAndMakeVisible(*masterFx.back()); }
        sendA.bind(ctx.p, sp(0, 0)); sendB.bind(ctx.p, sp(1, 0)); addAndMakeVisible(sendA); addAndMakeVisible(sendB);
        master.bind(ctx.p, gp(G_MVOL), "Master volume (after the master effects, before the limiter)"); addAndMakeVisible(master);
        ceiling.bind(&ctx.p, gp(G_CEIL), "Ceiling", "Limiter ceiling: the loudest the main output can get. Prevents clipping in FL.");
        retA.bind(&ctx.p, gp(G_RETA), "Return A", "Level of the Send A effect coming back into the main mix"); retB.bind(&ctx.p, gp(G_RETB), "Return B", "Level of the Send B effect coming back into the main mix");
        for (auto* k : {&ceiling, &retA, &retB}) addAndMakeVisible(*k);
    }
    void refresh() {
        for (auto& s : strips) s->refresh(); for (auto& f : masterFx) f->refresh(); sendA.refresh(); sendB.refresh();
        auto& m = ctx.p.machine(); master.refresh(std::max(m.masterMeter[0].load(), m.masterMeter[1].load())); ceiling.refresh(); retA.refresh(); retB.refresh();
        float gr = m.limiterGr.load(), cgr = m.masterGr.load(); if (std::abs(gr - shownGr) > 0.05f || std::abs(cgr - shownCgr) > 0.05f) { shownGr = gr; shownCgr = cgr; repaint(masterArea); }
    }
    void paint(juce::Graphics& g) override {
        drawModule(g, masterArea, "MASTER");
        g.setColour(col::cream); g.setFont(font(11.5f, true));
        g.drawText("Comp GR  " + juce::String(shownCgr, 1) + " dB", masterArea.getX() + 110, masterArea.getY() + 320, 150, 18, juce::Justification::centredLeft);
        g.drawText("Limiter GR  " + juce::String(shownGr, 1) + " dB", masterArea.getX() + 110, masterArea.getY() + 340, 150, 18, juce::Justification::centredLeft);
        g.setColour(col::creamDim); g.setFont(font(10.5f));
        g.drawFittedText("Output: Main = stereo out 1. Out 2-13 = separate stereo outputs. In FL: plugin gear > Processing > Connections > Auto map outputs (see README).", masterArea.getX() + 110, masterArea.getY() + 364, 150, 90, juce::Justification::topLeft, 7);
    }
    void resized() override {
        for (int i = 0; i < kChannels; ++i) strips[(size_t)i]->setBounds(8 + i * 89, 8, 86, 470);
        masterArea = {1080, 8, 272, 470}; master.setBounds(1092, 36, 90, 430);
        ceiling.setBounds(1196, 36, 70, 80); retA.setBounds(1196, 124, 70, 80); retB.setBounds(1196, 212, 70, 80);
        for (int s = 0; s < kMasterSlots; ++s) masterFx[(size_t)s]->setBounds(8 + s * 224, 486, 220, 296);
        sendA.setBounds(8 + 4 * 224, 486, 220, 296); sendB.setBounds(8 + 5 * 224, 486, 220, 296);
    }
private:
    Ctx& ctx; std::vector<std::unique_ptr<MixerStrip>> strips; std::vector<std::unique_ptr<FxPanel>> masterFx; FxPanel sendA, sendB; Fader master; Knob ceiling, retA, retB;
    juce::Rectangle<int> masterArea; float shownGr = 0, shownCgr = 0;
};

// =====================================================================================================
class PresetBrowser : public juce::Component, private juce::ListBoxModel {
public:
    explicit PresetBrowser(Ctx& c) : ctx(c) {
        for (int i = 0; i < C_COUNT; ++i) {
            auto b = std::make_unique<juce::TextButton>(categoryName(i)); b->setClickingTogglesState(true); b->setRadioGroupId(91);
            b->onClick = [this, i] { setCategory(i); }; addAndMakeVisible(*b); cats.push_back(std::move(b));
        }
        list.setModel(this); list.setRowHeight(26); list.setColour(juce::ListBox::outlineColourId, col::line); list.setOutlineThickness(1); addAndMakeVisible(list);
        for (auto* b : {&loadB, &favB, &saveB, &renameB, &deleteB, &closeB}) addAndMakeVisible(*b);
        name.setTextToShowWhenEmpty("Preset name", col::rail); name.setFont(font(14.f)); addAndMakeVisible(name);
        loadB.onClick = [this] { loadSelected(); };
        favB.onClick = [this] { if (auto* p = selected()) { ctx.p.toggleFavourite(*p); rebuild(); } };
        saveB.onClick = [this] { juce::String err; auto nm = name.getText().isNotEmpty() ? name.getText() : ctx.p.lastPresetName; if (ctx.p.saveUserPreset(nm, category, ctx.channel(), err)) { ctx.status("Saved \"" + nm + "\" in " + categoryName(category)); rebuild(); } else ctx.status(err); };
        renameB.onClick = [this] { auto* p = selected(); if (!p || p->factory) { ctx.status("Only your own presets can be renamed"); return; } if (ctx.p.renameUserPreset(*p, name.getText())) { ctx.status("Renamed"); rebuild(); } else ctx.status("Could not rename (name empty or taken?)"); };
        deleteB.onClick = [this] { auto* p = selected(); if (!p || p->factory) { ctx.status("Factory presets cannot be deleted"); return; } if (ctx.p.deleteUserPreset(*p)) { ctx.status("Deleted " + p->name); rebuild(); } };
        closeB.onClick = [this] { setVisible(false); };
        loadB.setTooltip("Load the selected preset (double-click works too)"); favB.setTooltip("Mark/unmark as favourite. Favourites are listed first.");
        saveB.setTooltip("Save the current sound/kit/pattern under the name in the box"); renameB.setTooltip("Rename the selected user preset to the name in the box");
        cats[(size_t)C_KIT]->setToggleState(true, juce::dontSendNotification); setCategory(C_KIT);
    }
    void setCategory(int c) { category = c; cats[(size_t)c]->setToggleState(true, juce::dontSendNotification); rebuild(); }
    void rebuild() { items = ctx.p.presets(category); list.updateContent(); list.repaint(); repaint(); }
    const BakeryProcessor::PresetInfo* selected() const { int r = list.getSelectedRow(); return r >= 0 && r < (int)items.size() ? &items[(size_t)r] : nullptr; }
    void loadSelected() {
        auto* p = selected(); if (!p) { ctx.status("Select a preset first"); return; }
        if (ctx.p.loadPreset(*p, ctx.channel())) { ctx.status("Loaded " + p->name + (category < C_KIT ? " into " + juce::String(channelName(ctx.channel())) : juce::String())); if (category < C_KIT) ctx.p.audition(ctx.channel()); }
        else ctx.status("Could not load " + p->name);
    }
    int getNumRows() override { return (int)items.size(); }
    void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool sel) override {
        if (row < 0 || row >= (int)items.size()) return; auto& it = items[(size_t)row];
        g.fillAll(sel ? col::orange : (row % 2 ? col::black : juce::Colour(0xff15161a)));
        g.setColour(sel ? col::black : (ctx.p.isFavourite(it) ? col::orange : col::rail)); g.setFont(font(15.f, true)); g.drawText(ctx.p.isFavourite(it) ? juce::String::charToString(0x2605) : juce::String::charToString(0x2606), 6, 0, 22, h, juce::Justification::centred);
        g.setColour(sel ? col::black : col::cream); g.setFont(font(14.f, true)); g.drawText(it.name, 32, 0, w - 140, h, juce::Justification::centredLeft, true);
        g.setColour(sel ? col::black : col::creamDim); g.setFont(font(11.f)); g.drawText(it.factory ? "FACTORY" : "USER", w - 100, 0, 90, h, juce::Justification::centredRight);
    }
    void listBoxItemDoubleClicked(int, const juce::MouseEvent&) override { loadSelected(); }
    void selectedRowsChanged(int r) override { if (r >= 0 && r < (int)items.size() && !items[(size_t)r].factory) name.setText(items[(size_t)r].name, false); }
    void paint(juce::Graphics& g) override {
        g.fillAll(col::bg.withAlpha(0.96f)); drawModule(g, getLocalBounds().reduced(8), "PRESET BROWSER");
        g.setColour(col::creamDim); g.setFont(font(12.f));
        juce::String target = category == C_KIT ? "Kits replace all 12 sounds, the mix and the current pattern." : category == C_PATTERN ? "Patterns load into the current pattern slot (" + juce::String(ctx.p.currentPattern() + 1) + ")." : "Sounds load into the selected channel: " + juce::String(channelName(ctx.channel())) + ".";
        g.drawFittedText(target + "\nYour presets are saved in:\n" + ctx.p.presetRoot().getFullPathName(), 1000, 360, 340, 90, juce::Justification::topLeft, 6);
    }
    void resized() override {
        for (int i = 0; i < C_COUNT; ++i) cats[(size_t)i]->setBounds(24, 40 + i * 36, 230, 32);
        list.setBounds(270, 40, 714, getHeight() - 64);
        loadB.setBounds(1000, 40, 160, 36); favB.setBounds(1170, 40, 160, 36); name.setBounds(1000, 100, 330, 30);
        saveB.setBounds(1000, 140, 160, 32); renameB.setBounds(1170, 140, 160, 32); deleteB.setBounds(1000, 182, 160, 32); closeB.setBounds(1170, getHeight() - 60, 160, 36);
    }
private:
    Ctx& ctx; std::vector<std::unique_ptr<juce::TextButton>> cats; juce::ListBox list; juce::TextEditor name; std::vector<BakeryProcessor::PresetInfo> items; int category = C_KIT;
    juce::TextButton loadB{"LOAD"}, favB{"FAVOURITE"}, saveB{"SAVE AS NEW"}, renameB{"RENAME"}, deleteB{"DELETE"}, closeB{"CLOSE"};
};
} // namespace ui
