#pragma once
#include "Pages.h"

class BakeryEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::Timer {
public:
    static constexpr int kW = 1360, kH = 860, kTop = 70;
    explicit BakeryEditor(BakeryProcessor&);
    ~BakeryEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void showWorkspace(int w);
    int workspace() const { return ws; }
    void showPresets(bool show);
    void tick() { timerCallback(); }   // tests
    juce::String statusText() const { return statusMsg; }
    ui::DesignPage& design() { return *designPage; }
    ui::SequencerPage& sequencer() { return *seqPage; }
private:
    void timerCallback() override;
    void status(const juce::String& s) { statusMsg = s; statusTime = juce::Time::getMillisecondCounter(); content.repaint(0, 0, kW, kTop); }
    struct Content : juce::Component { BakeryEditor* ed = nullptr; void paint(juce::Graphics& g) override { ed->paintContent(g); } };
    void paintContent(juce::Graphics&);
    BakeryProcessor& proc; ui::Look look; ui::Ctx ctx; Content content;
    std::unique_ptr<ui::DesignPage> designPage; std::unique_ptr<ui::SequencerPage> seqPage; std::unique_ptr<ui::MixerPage> mixPage; std::unique_ptr<ui::PresetBrowser> browser;
    juce::TextButton tabs[3]; juce::TextButton prevB{"<"}, nextB{">"}, presetsB{"PRESETS"}, rndSound{"SOUND"}, rndKit{"KIT"}, rndPat{"PATTERN"};
    ui::Choice rndMode; ui::Knob rndInt; ui::Toggle play{"PLAY"}; juce::ComboBox zoom;
    std::unique_ptr<juce::TooltipWindow> tips; int ws = 0; bool ready = false; juce::String statusMsg; juce::uint32 statusTime = 0; juce::String shownPreset;
};
