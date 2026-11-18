#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Key.h"

using namespace bk;

static juce::String num(float v, int dec) { return dec <= 0 ? juce::String((int)std::lround(v)) : juce::String(v, dec); }
static juce::String fmtValue(const ParamDef& d, float v) {
    if (!d.choices.empty()) return juce::String(d.choices[(size_t)juce::jlimit(0, (int)d.choices.size() - 1, (int)std::lround(v))]);
    if (d.toggle) return v > 0.5f ? "On" : "Off";
    if (d.integer) return juce::String((int)std::lround(v));
    if (d.unit == "Hz") return v >= 1000 ? juce::String(v / 1000.f, 2) + " kHz" : num(v, v < 100 ? 1 : 0) + " Hz";
    if (d.unit == "ms") return v >= 1000 ? juce::String(v / 1000.f, 2) + " s" : num(v, v < 10 ? 1 : 0) + " ms";
    if (d.unit == "dB") return v <= -59.9f ? juce::String("-inf dB") : juce::String(v, 1) + " dB";
    if (d.unit == "st") return juce::String(v, 1) + " st";
    if (d.unit == "x") return juce::String(v, 2) + "x";
    if (d.unit == "%") return juce::String(v, 1) + " %";
    if (d.unit == "BPM") return juce::String(v, 1) + " BPM";
    if (d.lo == -1 && d.hi == 1) return juce::String((int)std::lround(v * 100)) + (d.id.find("pan") != std::string::npos ? (v < -0.005f ? " L" : (v > 0.005f ? " R" : " C")) : " %");
    return juce::String((int)std::lround(v * 100)) + " %";
}

BakeryProcessor::BusesProperties BakeryProcessor::buses() {
    auto b = BusesProperties().withOutput("Main", juce::AudioChannelSet::stereo(), true);
    for (int c = 0; c < kChannels; ++c) b = b.withOutput(channelBusName(c), juce::AudioChannelSet::stereo(), true);
    return b;
}

juce::AudioProcessorValueTreeState::ParameterLayout BakeryProcessor::layout() {
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    const auto& reg = registry();
    for (size_t i = 0; i < reg.size(); ++i) {
        const auto& d = reg[i]; juce::ParameterID pid{juce::String(d.id), 1};
        if (!d.choices.empty()) { juce::StringArray c; for (auto& s : d.choices) c.add(s); l.add(std::make_unique<juce::AudioParameterChoice>(pid, d.name, c, (int)d.def)); }
        else if (d.toggle) l.add(std::make_unique<juce::AudioParameterBool>(pid, d.name, d.def > 0.5f));
        else if (d.integer) l.add(std::make_unique<juce::AudioParameterInt>(pid, d.name, (int)d.lo, (int)d.hi, (int)d.def));
        else {
            juce::NormalisableRange<float> r(d.lo, d.hi);
            if (d.centre > d.lo && d.centre < d.hi && d.centre != 0) r.setSkewForCentre(d.centre);
            auto attrs = juce::AudioParameterFloatAttributes().withLabel(d.unit).withStringFromValueFunction([d](float v, int) { return fmtValue(d, v); })
                .withValueFromStringFunction([d](const juce::String& t) {
                    auto x = t.trim(); float v = x.getFloatValue();
                    if (d.unit == "Hz" && x.containsIgnoreCase("k")) v *= 1000.f;
                    if (d.unit == "ms" && x.endsWithIgnoreCase("s") && !x.endsWithIgnoreCase("ms")) v *= 1000.f;
                    if (d.unit.empty() && (x.endsWith("%") || v > d.hi)) v /= 100.f;
                    if (d.unit.empty() && d.lo == -1 && d.hi == 1) { v = x.getFloatValue() / 100.f; if (x.endsWithIgnoreCase("L")) v = -std::abs(v); }
                    return juce::jlimit(d.lo, d.hi, v); });
            l.add(std::make_unique<juce::AudioParameterFloat>(pid, d.name, r, d.def, attrs));
        }
    }
    return l;
}

BakeryProcessor::BakeryProcessor() : AudioProcessor(buses()), apvts(*this, nullptr, "BAKERY", layout()), engine(std::make_unique<DrumMachine>()) {
    const auto& reg = registry();
    for (auto& d : reg) { auto* p = apvts.getParameter(juce::String(d.id)); jassert(p != nullptr); params.push_back(p); raw.push_back(apvts.getRawParameterValue(juce::String(d.id))); }
    midiScratch.reserve(4096);
    presetDir = juce::File();
    loadFavourites();
    // start with the 909 House Kit groove in pattern 1 so the first play makes sound
    std::vector<float> v((size_t)kNumParams); snapshot(v.data());
    applyKit(v.data(), &engine->bank, 0, factoryKits()[1]); applyValues(v.data()); lastPresetName = factoryKits()[1].name; program = 1;
    for (int m = 0; m < 8; ++m) lastMacro[(size_t)m] = value(gp(G_MACRO1 + m));
    startTimerHz(30);
}
BakeryProcessor::~BakeryProcessor() { stopTimer(); }

void BakeryProcessor::prepareToPlay(double sr, int block) {
    engine->prepare(sr, block);
    midiScratch.reserve(4096);
}

bool BakeryProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.inputBuses.size() > 0) for (auto& b : layouts.inputBuses) if (!b.isDisabled()) return false;
    if (layouts.outputBuses.isEmpty() || layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    for (int i = 1; i < layouts.outputBuses.size(); ++i) { auto s = layouts.outputBuses[i]; if (!s.isDisabled() && s != juce::AudioChannelSet::stereo()) return false; }
    return true;
}

void BakeryProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    for (int i = 0; i < kNumParams; ++i) engine->params[i] = raw[(size_t)i]->load(std::memory_order_relaxed);
    HostInfo info;
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) {
        info.playing = pos->getIsPlaying();
        if (auto ppq = pos->getPpqPosition()) { info.hasPpq = true; info.ppq = *ppq; }
        if (auto bpm = pos->getBpm()) { info.hasBpm = *bpm > 1.0; info.bpm = *bpm; }
    }
    midiScratch.clear();
    for (const auto m : midi) {
        if (midiScratch.size() >= midiScratch.capacity()) break;
        auto msg = m.getMessage(); if (msg.getRawDataSize() < 1 || msg.getRawDataSize() > 3) continue;
        const auto* d = msg.getRawData();
        midiScratch.push_back({juce::jlimit(0, juce::jmax(0, buffer.getNumSamples() - 1), m.samplePosition), d[0], msg.getRawDataSize() > 1 ? d[1] : (uint8_t)0, msg.getRawDataSize() > 2 ? d[2] : (uint8_t)0});
    }
    std::array<float*, 2 + 2 * kChannels> outs{};
    for (int b = 0; b < getBusCount(false) && b <= kChannels; ++b) {
        auto* bus = getBus(false, b); if (!bus || !bus->isEnabled() || bus->getNumberOfChannels() < 2) continue;
        int l = getChannelIndexInProcessBlockBuffer(false, b, 0), r = getChannelIndexInProcessBlockBuffer(false, b, 1);
        if (l < buffer.getNumChannels() && r < buffer.getNumChannels()) { outs[(size_t)(2 * b)] = buffer.getWritePointer(l); outs[(size_t)(2 * b + 1)] = buffer.getWritePointer(r); }
    }
    for (int c = 0; c < buffer.getNumChannels(); ++c) { bool used = false; for (auto* o : outs) used |= o == buffer.getWritePointer(c); if (!used) buffer.clear(c, 0, buffer.getNumSamples()); }
    engine->process(outs.data(), (int)outs.size(), buffer.getNumSamples(), info, midiScratch.data(), (int)midiScratch.size());
}

juce::AudioProcessorEditor* BakeryProcessor::createEditor() { return new BakeryEditor(*this); }

int BakeryProcessor::getNumPrograms() { return (int)factoryKits().size(); }
const juce::String BakeryProcessor::getProgramName(int i) { return i >= 0 && i < getNumPrograms() ? juce::String(factoryKits()[(size_t)i].name) : juce::String(); }
void BakeryProcessor::setCurrentProgram(int i) {
    if (i < 0 || i >= getNumPrograms()) return;
    program = i; std::vector<float> v((size_t)kNumParams); snapshot(v.data());
    applyKit(v.data(), &engine->bank, currentPattern(), factoryKits()[(size_t)i]); applyValues(v.data()); lastPresetName = factoryKits()[(size_t)i].name;
    for (int c = 0; c < kChannels; ++c) clearSample(c);
}


void BakeryProcessor::syncKeepInKey() {
    if (snappingKeys) return;
    std::vector<float> v((size_t)kNumParams); snapshot(v.data());
    if (!applyKeepInKey(v.data())) return;
    snappingKeys = true;
    applyValues(v.data());
    snappingKeys = false;
}

void BakeryProcessor::setValue(int i, float realValue) {
    auto* p = params[(size_t)i]; float n = p->convertTo0to1(realValue);
    if (std::abs(p->getValue() - n) < 1e-7f) return;
    p->beginChangeGesture(); p->setValueNotifyingHost(n); p->endChangeGesture();
}
void BakeryProcessor::applyValues(const float* v) {
    for (int i = 0; i < kNumParams; ++i) if (std::abs(value(i) - v[i]) > 1e-6f * std::max(1.f, std::abs(v[i]))) setValue(i, v[i]);
}

void BakeryProcessor::setEngine(int ch, int e) {
    std::vector<float> v((size_t)kNumParams); snapshot(v.data()); applyEngineDefaults(v.data(), ch, e);
    if (e == E_CHAT || e == E_OHAT) v[(size_t)cp(ch, V_CHOKE)] = 1;
    normaliseChannel(v.data(), ch); applyValues(v.data()); syncKeepInKey();
}
void BakeryProcessor::randomSound(int ch) {
    std::vector<float> v((size_t)kNumParams); snapshot(v.data());
    bk::randomSound(v.data(), ch, (int)value(gp(G_RNDMODE)), value(gp(G_RNDINT)), rng, locks); applyValues(v.data()); syncKeepInKey();
}
void BakeryProcessor::randomKit() {
    std::vector<float> v((size_t)kNumParams); snapshot(v.data());
    bk::randomKit(v.data(), (int)value(gp(G_RNDMODE)), value(gp(G_RNDINT)), rng, locks); applyValues(v.data()); syncKeepInKey(); lastPresetName = juce::String("Random ") + randomModeName((int)value(gp(G_RNDMODE))) + " Kit";
}
void BakeryProcessor::randomPattern() { bk::randomPattern(engine->bank, currentPattern(), (int)value(gp(G_RNDMODE)), value(gp(G_RNDINT)), rng, locks); }
void BakeryProcessor::patternVariation() {
    int from = currentPattern(), to = (from + 1) % kPatterns;
    bk::patternVariation(engine->bank, from, to, value(gp(G_RNDINT)), rng, locks); setValue(gp(G_PATTERN), (float)(to + 1));
}

// ---------------- samples ----------------
static constexpr double kMaxSampleSeconds = 10.0;
bool BakeryProcessor::loadSample(int ch, const juce::File& f, juce::String& error) {
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(f));
    if (!r) { error = "Could not read " + f.getFileName() + " (use WAV or AIFF)"; return false; }
    if (r->lengthInSamples < 2 || r->sampleRate < 1000) { error = "Empty or invalid audio file"; return false; }
    auto frames = (int)std::min<juce::int64>(r->lengthInSamples, (juce::int64)(r->sampleRate * kMaxSampleSeconds));
    juce::AudioBuffer<float> buf((int)juce::jlimit(1u, 2u, r->numChannels), frames);
    r->read(&buf, 0, frames, 0, true, buf.getNumChannels() > 1);
    auto d = std::make_unique<SampleData>(); d->rate = r->sampleRate; d->name = f.getFileName().toStdString(); d->path = f.getFullPathName().toStdString();
    d->L.assign(buf.getReadPointer(0), buf.getReadPointer(0) + frames);
    d->R.assign(buf.getReadPointer(buf.getNumChannels() > 1 ? 1 : 0), buf.getReadPointer(buf.getNumChannels() > 1 ? 1 : 0) + frames);
    if (r->lengthInSamples > frames) error = "Loaded the first " + juce::String((int)kMaxSampleSeconds) + " seconds only";
    return setSampleData(ch, std::move(d));
}
bool BakeryProcessor::setSampleData(int ch, std::unique_ptr<SampleData> d) {
    if (ch < 0 || ch >= kChannels) return false;
    const juce::ScopedLock sl(sampleLock);
    auto* p = d.get();
    if (samples[(size_t)ch]) graveyard.push_back({ch, std::move(samples[(size_t)ch])});
    samples[(size_t)ch] = std::move(d);
    engine->samplePending[ch].store(p, std::memory_order_release);
    return true;
}
void BakeryProcessor::clearSample(int ch) { setSampleData(ch, nullptr); }
void BakeryProcessor::collectGarbage() {
    const juce::ScopedLock sl(sampleLock);
    for (size_t i = 0; i < graveyard.size();) {
        int ch = graveyard[i].first; auto* old = graveyard[i].second.get();
        auto* cur = engine->samplePending[ch].load(std::memory_order_acquire);
        bool released = cur != old && engine->sampleAck[ch].load(std::memory_order_acquire) == cur; // audio thread has moved on to the newest pointer
        if (released || !engine->running.load()) graveyard.erase(graveyard.begin() + (long)i); else ++i;
    }
}

// ---------------- MIDI export ----------------
std::vector<uint8_t> BakeryProcessor::patternMidi(int p) {
    std::vector<float> v((size_t)kNumParams); snapshot(v.data());
    double bpm = 120; if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) if (auto b = pos->getBpm()) bpm = *b;
    return patternToMidi(engine->bank, p, v.data(), bpm);
}
bool BakeryProcessor::saveMidi(int p, const juce::File& f) { auto b = patternMidi(p); return f.replaceWithData(b.data(), b.size()); }

// ---------------- MIDI learn / macros ----------------
void BakeryProcessor::clearLearn(int paramIndex) { for (auto it = ccMap.begin(); it != ccMap.end();) { if (it->second == paramIndex) it = ccMap.erase(it); else ++it; } }
int BakeryProcessor::ccFor(int paramIndex) const { for (auto& [cc, p] : ccMap) if (p == paramIndex) return cc; return -1; }
void BakeryProcessor::linkMacro(int m, int pi) {
    if (m < 0 || m >= 8 || pi < 0 || pi >= kNumParams || (pi >= gp(G_MACRO1) && pi <= gp(G_MACRO8))) return;
    for (auto& l : macroLinks[(size_t)m]) if (l.param == pi) return;
    macroLinks[(size_t)m].push_back({pi, params[(size_t)pi]->getValue()});
}
void BakeryProcessor::unlinkMacros(int pi) { for (auto& v : macroLinks) v.erase(std::remove_if(v.begin(), v.end(), [pi](const MacroLink& l) { return l.param == pi; }), v.end()); }
void BakeryProcessor::processControlQueue() {
    auto& e = *engine;
    uint32_t w = e.ccWrite.load(std::memory_order_acquire), r = e.ccRead.load(std::memory_order_relaxed);
    while (r != w) {
        uint32_t ev = e.ccRing[r % e.ccRing.size()].load(std::memory_order_relaxed); ++r;
        int cc = (int)(ev >> 8), val = (int)(ev & 0x7F);
        int target = learnTarget.load();
        if (target >= 0) { clearLearn(target); ccMap[cc] = target; learnTarget.store(-1); }
        auto it = ccMap.find(cc);
        if (it != ccMap.end()) { auto* p = params[(size_t)it->second]; p->beginChangeGesture(); p->setValueNotifyingHost(val / 127.f); p->endChangeGesture(); }
    }
    e.ccRead.store(r, std::memory_order_release);
    for (int m = 0; m < 8; ++m) {
        float mv = value(gp(G_MACRO1 + m)); if (std::abs(mv - lastMacro[(size_t)m]) < 1e-6f) continue; lastMacro[(size_t)m] = mv;
        for (auto& l : macroLinks[(size_t)m]) { auto* p = params[(size_t)l.param]; float n = juce::jlimit(0.f, 1.f, l.base + (mv - 0.5f)); p->beginChangeGesture(); p->setValueNotifyingHost(n); p->endChangeGesture(); }
    }
}

// ---------------- state ----------------
static juce::String encodeSample(const SampleData& s) {
    juce::MemoryOutputStream raw; for (size_t i = 0; i < s.L.size(); ++i) { raw.writeFloat(s.L[i]); raw.writeFloat(s.R[i]); }
    juce::MemoryOutputStream z; { juce::GZIPCompressorOutputStream gz(z, 6); gz.write(raw.getData(), raw.getDataSize()); gz.flush(); }
    return juce::Base64::toBase64(z.getData(), z.getDataSize());
}
static std::unique_ptr<SampleData> decodeSample(const juce::ValueTree& t) {
    juce::MemoryOutputStream bin; if (!juce::Base64::convertFromBase64(bin, t["data"].toString())) return nullptr;
    juce::MemoryInputStream zin(bin.getData(), bin.getDataSize(), false); juce::GZIPDecompressorInputStream gz(zin);
    juce::MemoryBlock mb; gz.readIntoMemoryBlock(mb);
    int frames = (int)(mb.getSize() / 8); if (frames < 2) return nullptr;
    auto d = std::make_unique<SampleData>(); d->rate = (double)t["rate"]; d->name = t["name"].toString().toStdString(); d->path = t["path"].toString().toStdString();
    juce::MemoryInputStream in(mb, false); d->L.resize((size_t)frames); d->R.resize((size_t)frames);
    for (int i = 0; i < frames; ++i) { d->L[(size_t)i] = in.readFloat(); d->R[(size_t)i] = in.readFloat(); }
    if (d->rate < 1000) d->rate = 44100;
    return d;
}
juce::ValueTree BakeryProcessor::samplesTree(int only) const {
    juce::ValueTree st("Samples");
    const juce::ScopedLock sl(sampleLock);
    for (int c = 0; c < kChannels; ++c) {
        if (only >= 0 && c != only) continue; auto* s = samples[(size_t)c].get(); if (!s) continue;
        juce::ValueTree t("Sample"); t.setProperty("channel", c, nullptr); t.setProperty("name", juce::String(s->name), nullptr); t.setProperty("path", juce::String(s->path), nullptr);
        t.setProperty("rate", s->rate, nullptr); t.setProperty("data", encodeSample(*s), nullptr); st.appendChild(t, nullptr);
    }
    return st;
}
void BakeryProcessor::restoreSamples(const juce::ValueTree& st, int into) {
    for (auto t : st) {
        int c = into >= 0 ? into : (int)t["channel"]; if (c < 0 || c >= kChannels) continue;
        auto d = decodeSample(t);
        if (!d) { juce::File f(t["path"].toString()); juce::String err; if (f.existsAsFile()) loadSample(c, f, err); continue; } // fallback to the original file
        setSampleData(c, std::move(d));
    }
}

void BakeryProcessor::getStateInformation(juce::MemoryBlock& dest) {
    juce::ValueTree root("BAKERY_STATE"); root.setProperty("version", 1, nullptr);
    root.appendChild(apvts.copyState(), nullptr);
    juce::ValueTree pats("Patterns");
    for (int p = 0; p < kPatterns; ++p) { juce::ValueTree t("Pattern"); t.setProperty("index", p, nullptr); t.setProperty("name", juce::String(engine->bank.names[p]), nullptr); t.setProperty("steps", juce::String(engine->bank.serialise(p)), nullptr); pats.appendChild(t, nullptr); }
    root.appendChild(pats, nullptr);
    root.appendChild(samplesTree(), nullptr);
    juce::ValueTree learn("MidiLearn"); for (auto& [cc, p] : ccMap) { juce::ValueTree t("Map"); t.setProperty("cc", cc, nullptr); t.setProperty("param", juce::String(registry()[(size_t)p].id), nullptr); learn.appendChild(t, nullptr); }
    root.appendChild(learn, nullptr);
    juce::ValueTree mac("Macros"); for (int m = 0; m < 8; ++m) for (auto& l : macroLinks[(size_t)m]) { juce::ValueTree t("Link"); t.setProperty("macro", m, nullptr); t.setProperty("param", juce::String(registry()[(size_t)l.param].id), nullptr); t.setProperty("base", l.base, nullptr); mac.appendChild(t, nullptr); }
    root.appendChild(mac, nullptr);
    juce::ValueTree lk("Locks"); juce::String snd, trk, prm;
    for (int c = 0; c < kChannels; ++c) { snd << (locks.sound[c] ? "1" : "0"); trk << (locks.track[c] ? "1" : "0"); }
    for (int p : locks.params) prm << juce::String(registry()[(size_t)p].id) << " ";
    lk.setProperty("sound", snd, nullptr); lk.setProperty("track", trk, nullptr); lk.setProperty("params", prm.trim(), nullptr); root.appendChild(lk, nullptr);
    juce::ValueTree ui("UI"); ui.setProperty("workspace", uiWorkspace, nullptr); ui.setProperty("channel", uiChannel, nullptr); ui.setProperty("scale", uiScale, nullptr); ui.setProperty("page", uiStepPage, nullptr); ui.setProperty("preset", lastPresetName, nullptr); ui.setProperty("program", program, nullptr);
    root.appendChild(ui, nullptr);
    if (auto xml = root.createXml()) copyXmlToBinary(*xml, dest);
}

void BakeryProcessor::setStateInformation(const void* data, int size) {
    auto xml = getXmlFromBinary(data, size); if (!xml) return;
    auto root = juce::ValueTree::fromXml(*xml); if (!root.hasType("BAKERY_STATE")) return;
    auto params = root.getChildWithName(apvts.state.getType()); if (params.isValid()) apvts.replaceState(params);
    for (auto t : root.getChildWithName("Patterns")) { int p = t["index"]; if (p < 0 || p >= kPatterns) continue; engine->bank.deserialise(p, t["steps"].toString().toStdString()); engine->bank.names[p] = t["name"].toString().toStdString(); }
    for (int c = 0; c < kChannels; ++c) if (samples[(size_t)c]) clearSample(c);
    restoreSamples(root.getChildWithName("Samples"));
    ccMap.clear(); for (auto t : root.getChildWithName("MidiLearn")) { int p = paramIndex(t["param"].toString().toStdString()); if (p >= 0) ccMap[(int)t["cc"]] = p; }
    for (auto& v : macroLinks) v.clear();
    for (auto t : root.getChildWithName("Macros")) { int m = t["macro"], p = paramIndex(t["param"].toString().toStdString()); if (p >= 0 && m >= 0 && m < 8) macroLinks[(size_t)m].push_back({p, (float)(double)t["base"]}); }
    auto lk = root.getChildWithName("Locks"); juce::String snd = lk["sound"].toString(), trk = lk["track"].toString();
    for (int c = 0; c < kChannels; ++c) { locks.sound[c] = snd[c] == '1'; locks.track[c] = trk[c] == '1'; }
    locks.params.clear(); for (auto& id : juce::StringArray::fromTokens(lk["params"].toString(), " ", "")) { int p = paramIndex(id.toStdString()); if (p >= 0) locks.params.insert(p); }
    auto ui = root.getChildWithName("UI"); uiWorkspace = ui.getProperty("workspace", 0); uiChannel = ui.getProperty("channel", 0); uiScale = (float)(double)ui.getProperty("scale", 1.0); uiStepPage = ui.getProperty("page", 0);
    lastPresetName = ui.getProperty("preset", "").toString(); program = ui.getProperty("program", 0);
    for (int m = 0; m < 8; ++m) lastMacro[(size_t)m] = value(gp(G_MACRO1 + m));
}

// ---------------- presets ----------------
juce::File BakeryProcessor::presetRoot() const {
    if (presetDir != juce::File()) return presetDir;
#if JUCE_MAC
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Application Support/BAKERY/Presets");
#else
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("BAKERY/Presets");
#endif
}
static juce::String favKey(const BakeryProcessor::PresetInfo& p) { return juce::String(p.category) + "|" + (p.factory ? "F|" : "U|") + p.name; }
void BakeryProcessor::loadFavourites() { favourites.clear(); for (auto& l : juce::StringArray::fromLines(presetRoot().getChildFile("favourites.txt").loadFileAsString())) if (l.isNotEmpty()) favourites.insert(l); }
void BakeryProcessor::saveFavourites() const { juce::String s; for (auto& f : favourites) s << f << "\n"; presetRoot().createDirectory(); presetRoot().getChildFile("favourites.txt").replaceWithText(s); }
bool BakeryProcessor::isFavourite(const PresetInfo& p) const { return favourites.count(favKey(p)) > 0; }
void BakeryProcessor::toggleFavourite(const PresetInfo& p) { auto k = favKey(p); if (favourites.count(k)) favourites.erase(k); else favourites.insert(k); saveFavourites(); }

std::vector<BakeryProcessor::PresetInfo> BakeryProcessor::presets(int category) const {
    std::vector<PresetInfo> out;
    if (category == C_KIT) { int i = 0; for (auto& k : factoryKits()) out.push_back({k.name, C_KIT, true, {}, i++}); }
    else if (category == C_PATTERN) { int i = 0; for (auto& p : factoryPatterns()) out.push_back({p.name, C_PATTERN, true, {}, i++}); }
    else { int i = 0; for (auto& s : factorySounds()) { if (s.category == category) out.push_back({s.name, category, true, {}, i}); ++i; } }
    auto dir = presetRoot().getChildFile(categoryName(category));
    for (auto& f : dir.findChildFiles(juce::File::findFiles, false, "*.bakery")) out.push_back({f.getFileNameWithoutExtension(), category, false, f, -1});
    std::stable_sort(out.begin(), out.end(), [this](const PresetInfo& a, const PresetInfo& b) { return isFavourite(a) > isFavourite(b); });
    return out;
}

juce::ValueTree BakeryProcessor::presetTree(int category, int ch) const {
    juce::ValueTree t("BAKERY_PRESET"); t.setProperty("category", category, nullptr); t.setProperty("version", 1, nullptr);
    if (category == C_PATTERN) { int p = currentPattern(); t.setProperty("steps", juce::String(engine->bank.serialise(p)), nullptr); t.setProperty("swing", value(gp(G_SWING)), nullptr); t.setProperty("rate", value(gp(G_RATE)), nullptr); return t; }
    juce::ValueTree vals("Values");
    if (category == C_KIT) {
        for (int i = 0; i < kNumParams; ++i) {
            if (i >= kGlobalBase && i < kMasterBase) { int g = i - kGlobalBase; if (g != G_SWING && g != G_RATE && g != G_RETA && g != G_RETB && g != G_HUMAN) continue; }
            vals.setProperty(juce::String(registry()[(size_t)i].id), value(i), nullptr); }
        t.setProperty("steps", juce::String(engine->bank.serialise(currentPattern())), nullptr);
        t.appendChild(samplesTree(), nullptr);
    } else {
        for (int k = 0; k <= S_DEC; ++k) if (k != V_NOTE) vals.setProperty(juce::String(chanKey(k).id), value(cp(ch, k)), nullptr);
        for (int k = F1_TYPE; k < kChanParams; ++k) vals.setProperty(juce::String(chanKey(k).id), value(cp(ch, k)), nullptr);
        t.appendChild(samplesTree(ch), nullptr);
    }
    t.appendChild(vals, nullptr);
    return t;
}
bool BakeryProcessor::applyPresetTree(const juce::ValueTree& t, int ch) {
    if (!t.hasType("BAKERY_PRESET")) return false;
    int category = t["category"];
    std::vector<float> v((size_t)kNumParams); snapshot(v.data());
    if (category == C_PATTERN) { if (!engine->bank.deserialise(currentPattern(), t["steps"].toString().toStdString())) return false; v[(size_t)gp(G_SWING)] = (float)(double)t["swing"]; v[(size_t)gp(G_RATE)] = (float)(double)t["rate"]; applyValues(v.data()); return true; }
    auto vals = t.getChildWithName("Values"); if (!vals.isValid()) return false;
    if (category == C_KIT) {
        for (int i = 0; i < vals.getNumProperties(); ++i) { auto id = vals.getPropertyName(i).toString(); int p = paramIndex(id.toStdString()); if (p >= 0) v[(size_t)p] = clampToDef(p, (float)(double)vals[vals.getPropertyName(i)]); }
        engine->bank.deserialise(currentPattern(), t["steps"].toString().toStdString());
        applyValues(v.data()); for (int c = 0; c < kChannels; ++c) clearSample(c); restoreSamples(t.getChildWithName("Samples"));
        return true;
    }
    for (int i = 0; i < vals.getNumProperties(); ++i) { int k = chanKeyIndex(vals.getPropertyName(i).toString().toStdString()); if (k >= 0) v[(size_t)cp(ch, k)] = clampToDef(cp(ch, k), (float)(double)vals[vals.getPropertyName(i)]); }
    applyValues(v.data()); clearSample(ch); restoreSamples(t.getChildWithName("Samples"), ch);
    return true;
}
bool BakeryProcessor::loadPreset(const PresetInfo& info, int ch) {
    std::vector<float> v((size_t)kNumParams); snapshot(v.data());
    if (info.factory) {
        if (info.category == C_KIT) { applyKit(v.data(), &engine->bank, currentPattern(), factoryKits()[(size_t)info.index]); applyValues(v.data()); for (int c = 0; c < kChannels; ++c) clearSample(c); program = info.index; }
        else if (info.category == C_PATTERN) { auto& p = factoryPatterns()[(size_t)info.index]; applyPattern(engine->bank, currentPattern(), p); v[(size_t)gp(G_SWING)] = p.swing; v[(size_t)gp(G_RATE)] = (float)p.rate; applyValues(v.data()); }
        else { applySound(v.data(), ch, factorySounds()[(size_t)info.index]); applyValues(v.data()); clearSample(ch); }
        lastPresetName = info.name; return true;
    }
    auto xml = juce::XmlDocument::parse(info.file); if (!xml) return false;
    bool ok = applyPresetTree(juce::ValueTree::fromXml(*xml), ch); if (ok) lastPresetName = info.name; return ok;
}
bool BakeryProcessor::saveUserPreset(const juce::String& name, int category, int ch, juce::String& error) {
    auto clean = juce::File::createLegalFileName(name.trim()); if (clean.isEmpty()) { error = "Give the preset a name"; return false; }
    auto dir = presetRoot().getChildFile(categoryName(category)); if (!dir.createDirectory()) { error = "Could not create " + dir.getFullPathName(); return false; }
    auto xml = presetTree(category, ch).createXml(); if (!xml) { error = "Could not build preset"; return false; }
    auto f = dir.getChildFile(clean + ".bakery"); if (!xml->writeTo(f)) { error = "Could not write " + f.getFullPathName(); return false; }
    lastPresetName = clean; return true;
}
bool BakeryProcessor::renameUserPreset(const PresetInfo& p, const juce::String& n) {
    if (p.factory) return false; auto clean = juce::File::createLegalFileName(n.trim()); if (clean.isEmpty()) return false;
    bool fav = isFavourite(p); auto to = p.file.getSiblingFile(clean + ".bakery"); if (to.exists()) return false;
    if (!p.file.moveFileTo(to)) return false;
    if (fav) { toggleFavourite(p); PresetInfo q = p; q.name = clean; q.file = to; toggleFavourite(q); }
    return true;
}
bool BakeryProcessor::deleteUserPreset(const PresetInfo& p) { if (p.factory) return false; if (isFavourite(p)) toggleFavourite(p); return p.file.deleteFile(); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BakeryProcessor(); }
