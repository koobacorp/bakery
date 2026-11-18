#include "PluginEditor.h"
using namespace ui;

BakeryEditor::BakeryEditor(BakeryProcessor& p)
    : AudioProcessorEditor(p), proc(p), ctx{p, [this](int c) { proc.uiChannel = c; }, [this](const juce::String& s) { status(s); }} {
    setLookAndFeel(&look); content.ed = this; content.setLookAndFeel(&look); addAndMakeVisible(content);
    designPage = std::make_unique<DesignPage>(ctx); seqPage = std::make_unique<SequencerPage>(ctx); mixPage = std::make_unique<MixerPage>(ctx); browser = std::make_unique<PresetBrowser>(ctx);
    for (juce::Component* pg : {(juce::Component*)designPage.get(), (juce::Component*)seqPage.get(), (juce::Component*)mixPage.get()}) { content.addChildComponent(pg); pg->setBounds(0, kTop, kW, kH - kTop); }
    content.addChildComponent(*browser); browser->setBounds(0, kTop, kW, kH - kTop);
    const char* names[3] = {"DRUM DESIGN", "SEQUENCER", "KIT MIXER"};
    const char* tipsT[3] = {"Build sounds: pick a module, shape it with the knobs, add a sample and effects.", "Program patterns, swing, probability and ratchets. Drag the pattern to FL as MIDI.", "Levels, panning, sends, master effects and separate outputs."};
    for (int i = 0; i < 3; ++i) { auto& t = tabs[i]; t.setButtonText(names[i]); t.setClickingTogglesState(true); t.setRadioGroupId(5); t.setTooltip(tipsT[i]); t.onClick = [this, i] { showWorkspace(i); }; content.addAndMakeVisible(t); }
    prevB.onClick = [this] { proc.setCurrentProgram((proc.getCurrentProgram() + proc.getNumPrograms() - 1) % proc.getNumPrograms()); status("Kit: " + proc.lastPresetName); };
    nextB.onClick = [this] { proc.setCurrentProgram((proc.getCurrentProgram() + 1) % proc.getNumPrograms()); status("Kit: " + proc.lastPresetName); };
    prevB.setTooltip("Previous factory kit"); nextB.setTooltip("Next factory kit");
    presetsB.onClick = [this] { showPresets(!browser->isVisible()); }; presetsB.setTooltip("Open the preset browser: sounds, kits and patterns, save your own");
    rndMode.bind(&proc, bk::gp(bk::G_RNDMODE), "Random style: Classic, Analog, Experimental, Industrial, Minimal, Complex or Ambient");
    rndInt.bind(&proc, bk::gp(bk::G_RNDINT), "Amount", "How far Random moves away from a sensible starting point (left = subtle, right = wild)");
    rndSound.setTooltip("Random sound for the selected channel"); rndKit.setTooltip("Random kit: all 12 sounds that fit together (locked sounds stay)"); rndPat.setTooltip("Random pattern in the current slot (locked tracks stay)");
    rndSound.onClick = [this] { proc.randomSound(proc.uiChannel); proc.audition(proc.uiChannel); status("Random " + juce::String(bk::randomModeName((int)proc.value(bk::gp(bk::G_RNDMODE)))) + " sound on " + bk::channelName(proc.uiChannel)); };
    rndKit.onClick = [this] { proc.randomKit(); status("New random kit (" + juce::String(bk::randomModeName((int)proc.value(bk::gp(bk::G_RNDMODE)))) + ")"); };
    rndPat.onClick = [this] { proc.randomPattern(); status("New random pattern in slot " + juce::String(proc.currentPattern() + 1)); };
    play.bind(proc, bk::gp(bk::G_RUN), "Play the sequencer on its own (when FL is stopped). Needs Sequencer mode 'Host + internal'.");
    play.setColour(juce::TextButton::buttonOnColourId, col::green);
        for (auto* z : {"75%", "90%", "100%", "125%", "150%"}) zoom.addItem(z, zoom.getNumItems() + 1);
    zoom.setTooltip("Window size. You can also drag the bottom-right corner.");
    zoom.onChange = [this] { const float z[] = {0.75f, 0.9f, 1.f, 1.25f, 1.5f}; int i = zoom.getSelectedId() - 1; if (i >= 0) setSize((int)(kW * z[i]), (int)(kH * z[i])); };
    for (juce::Component* c : std::initializer_list<juce::Component*>{&prevB, &nextB, &presetsB, &rndMode, &rndInt, &rndSound, &rndKit, &rndPat, &play, &zoom}) content.addAndMakeVisible(c);
    content.setBounds(0, 0, kW, kH);
    tips = std::make_unique<juce::TooltipWindow>(this, 600);
    float sc = juce::jlimit(0.5f, 2.f, proc.uiScale);
    setResizable(true, true); setResizeLimits(kW / 2, kH / 2, kW * 2, kH * 2); getConstrainer()->setFixedAspectRatio((double)kW / kH);
    setSize((int)std::lround(kW * sc), (int)std::lround(kH * sc));
    ready = true; resized();
    showWorkspace(juce::jlimit(0, 2, proc.uiWorkspace));
    startTimerHz(30);
}
BakeryEditor::~BakeryEditor() { stopTimer(); content.setLookAndFeel(nullptr); setLookAndFeel(nullptr); }

void BakeryEditor::showWorkspace(int w) {
    ws = juce::jlimit(0, 2, w); proc.uiWorkspace = ws;
    for (int i = 0; i < 3; ++i) tabs[i].setToggleState(i == ws, juce::dontSendNotification);
    designPage->setVisible(ws == 0); seqPage->setVisible(ws == 1); mixPage->setVisible(ws == 2);
    browser->setVisible(false); timerCallback();
}
void BakeryEditor::showPresets(bool show) { if (show) browser->rebuild(); browser->setVisible(show); browser->toFront(false); presetsB.setToggleState(show, juce::dontSendNotification); }

void BakeryEditor::resized() {
    if (getWidth() <= 0) return; float s = getWidth() / (float)kW; content.setTransform(juce::AffineTransform::scale(s)); if (ready) proc.uiScale = s;
    int w = 0; const float z[] = {0.75f, 0.9f, 1.f, 1.25f, 1.5f}; for (int i = 0; i < 5; ++i) if (std::abs(s - z[i]) < 0.01f) w = i + 1; zoom.setSelectedId(w, juce::dontSendNotification);
    for (int i = 0; i < 3; ++i) tabs[i].setBounds(190 + i * 116, 22, 112, 32);
    prevB.setBounds(546, 22, 26, 32); nextB.setBounds(774, 22, 26, 32); presetsB.setBounds(804, 22, 74, 32);
    rndMode.setBounds(892, 28, 98, 26); rndInt.setBounds(992, 4, 54, 64);
    rndSound.setBounds(1048, 28, 54, 26); rndKit.setBounds(1105, 28, 40, 26); rndPat.setBounds(1148, 28, 66, 26);
    play.setBounds(1222, 22, 54, 32); zoom.setBounds(1282, 26, 72, 26);
}
void BakeryEditor::paint(juce::Graphics& g) { g.fillAll(col::bg); }
void BakeryEditor::paintContent(juce::Graphics& g) {
    g.fillAll(col::bg);
    auto top = juce::Rectangle<int>(0, 0, kW, kTop);
    juce::ColourGradient grad(juce::Colour(0xff2b2c31), 0, 0, juce::Colour(0xff17181b), 0, (float)kTop, false); g.setGradientFill(grad); g.fillRect(top);
    g.setColour(col::orange); g.fillRect(0, kTop - 2, kW, 2);
    // logo: cream "loaf" badge + wordmark
    auto badge = juce::Rectangle<float>(12, 14, 40, 40); g.setColour(col::cream); g.fillRoundedRectangle(badge, 10);
    g.setColour(col::orange); for (int i = 0; i < 3; ++i) g.drawLine(20.f + i * 9, 24, 26.f + i * 9, 44, 3);
    g.setColour(col::cream); g.setFont(font(30.f, true)); g.drawText("BAKERY", 60, 10, 130, 34, juce::Justification::centredLeft);
    g.setColour(col::orange); g.setFont(font(9.f, true)); g.drawText("DRUM SYNTH | KOOBACORP", 61, 42, 128, 14, juce::Justification::centredLeft);
    // preset display
    auto pr = juce::Rectangle<int>(574, 22, 198, 32); g.setColour(col::black); g.fillRoundedRectangle(pr.toFloat(), 4); g.setColour(col::line); g.drawRoundedRectangle(pr.toFloat(), 4, 1);
    g.setColour(col::orange); g.setFont(font(14.f, true)); g.drawFittedText(proc.lastPresetName.isNotEmpty() ? proc.lastPresetName : juce::String("Init"), pr.reduced(6, 0), juce::Justification::centred, 1);
    g.setColour(col::creamDim); g.setFont(font(9.5f, true)); g.drawText("KIT / PRESET", 574, 6, 198, 14, juce::Justification::centred);
    g.drawText("RANDOM", 892, 8, 98, 16, juce::Justification::centredLeft); g.drawText("ZOOM", 1282, 8, 72, 16, juce::Justification::centredLeft);
    bool fresh = juce::Time::getMillisecondCounter() - statusTime < 6000;
    if (fresh && statusMsg.isNotEmpty()) { g.setColour(col::cream); g.setFont(font(11.f, true)); g.drawText(statusMsg, 546, 56, 670, 13, juce::Justification::centredLeft, true); }
    else { g.setColour(col::rail); g.setFont(font(10.5f)); g.drawText("Hover any control for help.  Right-click a knob: MIDI Learn, Lock, Link to Macro.", 546, 56, 670, 13, juce::Justification::centredLeft, true); }
}

void BakeryEditor::timerCallback() {
    rndMode.refresh(); rndInt.refresh(); play.refresh();
    if (ws == 0) designPage->refresh(); else if (ws == 1) seqPage->refresh(); else mixPage->refresh();
    juce::String nm = proc.lastPresetName; if (nm != shownPreset) { shownPreset = nm; content.repaint(0, 0, kW, kTop); }
    if (statusMsg.isNotEmpty() && juce::Time::getMillisecondCounter() - statusTime > 6000 && juce::Time::getMillisecondCounter() - statusTime < 6100) content.repaint(0, 0, kW, kTop);
}
