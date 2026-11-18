#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <set>
#include "EngineControls.h"
#include "Machine.h"
#include "Random.h"

class BakeryProcessor : public juce::AudioProcessor, private juce::Timer {
public:
    BakeryProcessor();
    ~BakeryProcessor() override;
    static BusesProperties buses();
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();

    // --- AudioProcessor ---
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "BAKERY"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    int getNumPrograms() override;
    int getCurrentProgram() override { return program; }
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // --- model access (message thread) ---
    juce::AudioProcessorValueTreeState apvts;
    juce::RangedAudioParameter* param(int i) const { return params[(size_t)i]; }
    float value(int i) const { return raw[(size_t)i]->load(std::memory_order_relaxed); }
    void setValue(int i, float realValue);
    void syncKeepInKey(); // snap pitched drums to the chosen key when Keep in key is on
    void snapshot(float* v) const { for (int i = 0; i < bk::kNumParams; ++i) v[i] = value(i); }
    void applyValues(const float* v); // writes every changed parameter (with host notification)
    bk::DrumMachine& machine() { return *engine; }
    bk::PatternBank& bank() { return engine->bank; }
    int currentPattern() const { return juce::jlimit(0, bk::kPatterns - 1, (int)value(bk::gp(bk::G_PATTERN)) - 1); }

    // engines / random / presets
    void setEngine(int ch, int engine);
    void randomSound(int ch);
    void randomKit();
    void randomPattern();
    void patternVariation();
    void audition(int ch, int vel = 110) { engine->audition[ch].store(vel); }
    bk::Locks locks;

    // samples
    bool loadSample(int ch, const juce::File& f, juce::String& error);
    bool setSampleData(int ch, std::unique_ptr<bk::SampleData> d);
    void clearSample(int ch);
    const bk::SampleData* sample(int ch) const { const juce::ScopedLock sl(sampleLock); return samples[(size_t)ch].get(); }

    // MIDI export
    std::vector<uint8_t> patternMidi(int pattern);
    bool saveMidi(int pattern, const juce::File& f);

    // MIDI learn and macros
    void startLearn(int paramIndex) { learnTarget.store(paramIndex); }
    int learning() const { return learnTarget.load(); }
    void clearLearn(int paramIndex);
    int ccFor(int paramIndex) const;
    std::map<int, int> ccMap; // cc -> param index
    struct MacroLink { int param; float base; };
    std::array<std::vector<MacroLink>, 8> macroLinks;
    void linkMacro(int macro, int paramIndex);
    void unlinkMacros(int paramIndex);
    void processControlQueue(); // also called by tests
    void collectGarbage();

    // presets
    struct PresetInfo { juce::String name; int category; bool factory; juce::File file; int index; };
    std::vector<PresetInfo> presets(int category) const;
    bool loadPreset(const PresetInfo&, int targetChannel);
    bool saveUserPreset(const juce::String& name, int category, int channel, juce::String& error);
    bool renameUserPreset(const PresetInfo&, const juce::String& newName);
    bool deleteUserPreset(const PresetInfo&);
    bool isFavourite(const PresetInfo&) const;
    void toggleFavourite(const PresetInfo&);
    juce::File presetRoot() const;
    void setPresetRoot(const juce::File& f) { presetDir = f; loadFavourites(); }
    juce::String lastPresetName;

    // editor state saved with the project
    int uiWorkspace = 0, uiChannel = 0, uiStepPage = 0; float uiScale = 1.0f;

    static juce::String channelBusName(int c) { return juce::String("Out ") + juce::String(c + 2) + " " + bk::channelName(c); }

private:
    void timerCallback() override { processControlQueue(); syncKeepInKey(); collectGarbage(); }
    bool snappingKeys = false;
    juce::ValueTree presetTree(int category, int channel) const;
    bool applyPresetTree(const juce::ValueTree&, int channel);
    juce::ValueTree samplesTree(int onlyChannel = -1) const;
    void restoreSamples(const juce::ValueTree&, int intoChannel = -1);
    void loadFavourites(); void saveFavourites() const;

    std::unique_ptr<bk::DrumMachine> engine;
    std::vector<juce::RangedAudioParameter*> params;
    std::vector<std::atomic<float>*> raw;
    std::vector<bk::MidiEvent> midiScratch;
    std::array<std::unique_ptr<bk::SampleData>, bk::kChannels> samples;
    std::vector<std::pair<int, std::unique_ptr<bk::SampleData>>> graveyard; // waiting for the audio thread to let go
    juce::CriticalSection sampleLock;
    std::atomic<int> learnTarget{-1};
    std::array<float, 8> lastMacro{};
    int program = 0;
    juce::File presetDir;
    std::set<juce::String> favourites;
    bk::Rng rng{(uint64_t)juce::Time::getMillisecondCounterHiRes()};
};
