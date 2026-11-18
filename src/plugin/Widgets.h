// BAKERY — look and feel + parameter-bound widgets. Strong contrast: charcoal panels, cream labels, orange values.
#pragma once
#include "PluginProcessor.h"

namespace ui {
namespace col {
const juce::Colour bg{0xff111113}, panel{0xff1d1e22}, panelHi{0xff2a2c32}, rail{0xff8d9097}, alu{0xffc3c6cc}, cream{0xfff4e7c9},
    creamDim{0xffbfb49b}, orange{0xffff8a1f}, orangeDim{0xff7a4310}, red{0xffff4a3d}, green{0xff5fe37f}, line{0xff3d4048}, black{0xff07070a}, blue{0xff58b7ff};
}
inline juce::Font font(float h, bool bold = false) { return juce::Font(juce::FontOptions(h, bold ? juce::Font::bold : juce::Font::plain)); }

/// Eurorack-style module plate: brushed charcoal, aluminium rails with screws, cream title.
inline void drawModule(juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, juce::Colour accent = col::orange) {
    auto f = r.toFloat();
    g.setColour(col::black); g.fillRoundedRectangle(f.translated(0, 2), 5);
    juce::ColourGradient grad(col::panelHi, f.getX(), f.getY(), col::panel, f.getX(), f.getBottom(), false); g.setGradientFill(grad); g.fillRoundedRectangle(f, 5);
    g.setColour(col::line); g.drawRoundedRectangle(f.reduced(0.5f), 5, 1);
    if (title.isNotEmpty()) {
        auto t = f.removeFromTop(22).reduced(8, 0);
        g.setColour(accent); g.fillRect(t.getX(), t.getBottom() - 2, 18.f, 2.f);
        g.setColour(col::cream); g.setFont(font(12.5f, true)); g.drawText(title, t.toNearestInt(), juce::Justification::centredLeft);
        for (float x : {f.getRight() - 12.f}) { g.setColour(col::rail); g.fillEllipse(x - 3, r.getY() + 8.f, 6, 6); g.setColour(col::black); g.drawLine(x - 2, r.getY() + 11.f, x + 2, r.getY() + 11.f, 1); }
    }
}

class Look : public juce::LookAndFeel_V4 {
public:
    Look() {
        setColour(juce::ResizableWindow::backgroundColourId, col::bg);
        setColour(juce::Slider::textBoxTextColourId, col::orange); setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack); setColour(juce::Slider::textBoxHighlightColourId, col::orangeDim);
        setColour(juce::TextButton::buttonColourId, col::panelHi); setColour(juce::TextButton::buttonOnColourId, col::orange);
        setColour(juce::TextButton::textColourOffId, col::cream); setColour(juce::TextButton::textColourOnId, col::black);
        setColour(juce::ComboBox::backgroundColourId, col::black); setColour(juce::ComboBox::textColourId, col::cream); setColour(juce::ComboBox::outlineColourId, col::line);
        setColour(juce::ComboBox::arrowColourId, col::orange);
        setColour(juce::PopupMenu::backgroundColourId, col::panel); setColour(juce::PopupMenu::textColourId, col::cream);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, col::orange); setColour(juce::PopupMenu::highlightedTextColourId, col::black);
        setColour(juce::Label::textColourId, col::cream); setColour(juce::TooltipWindow::backgroundColourId, col::cream); setColour(juce::TooltipWindow::textColourId, col::black);
        setColour(juce::TooltipWindow::outlineColourId, col::orange);
        setColour(juce::ListBox::backgroundColourId, col::black); setColour(juce::TextEditor::backgroundColourId, col::black); setColour(juce::TextEditor::textColourId, col::cream);
        setColour(juce::TextEditor::outlineColourId, col::line); setColour(juce::TextEditor::focusedOutlineColourId, col::orange); setColour(juce::CaretComponent::caretColourId, col::orange);
        setColour(juce::ScrollBar::thumbColourId, col::rail);
    }
    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override {
        auto r = juce::Rectangle<float>((float)x, (float)y, (float)w, (float)h).reduced(3); float d = std::min(r.getWidth(), r.getHeight()); auto c = r.withSizeKeepingCentre(d, d);
        float ang = a0 + pos * (a1 - a0); bool bip = s.getProperties()["bipolar"];
        // track + value arc
        juce::Path track; track.addCentredArc(c.getCentreX(), c.getCentreY(), d * 0.47f, d * 0.47f, 0, a0, a1, true);
        g.setColour(col::black); g.strokePath(track, juce::PathStrokeType(4.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        juce::Path arc; float from = bip ? (a0 + a1) * 0.5f : a0; arc.addCentredArc(c.getCentreX(), c.getCentreY(), d * 0.47f, d * 0.47f, 0, std::min(from, ang), std::max(from, ang), true);
        g.setColour(s.isEnabled() ? col::orange : col::rail); g.strokePath(arc, juce::PathStrokeType(4.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        // knob cap
        auto cap = c.reduced(d * 0.13f);
        g.setColour(col::black); g.fillEllipse(cap.translated(0, 1.5f));
        juce::ColourGradient grad(juce::Colour(0xff4a4d55), cap.getCentreX(), cap.getY(), juce::Colour(0xff17181b), cap.getCentreX(), cap.getBottom(), false);
        g.setGradientFill(grad); g.fillEllipse(cap);
        g.setColour(juce::Colour(0xff5c6069)); g.drawEllipse(cap, 1.f);
        juce::Path ptr; float pr = cap.getWidth() * 0.5f; ptr.addRoundedRectangle(-1.6f, -pr + 2, 3.2f, pr * 0.62f, 1.5f);
        g.setColour(col::cream); g.fillPath(ptr, juce::AffineTransform::rotation(ang).translated(cap.getCentreX(), cap.getCentreY()));
    }
    void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle st, juce::Slider& s) override {
        if (st != juce::Slider::LinearVertical) { LookAndFeel_V4::drawLinearSlider(g, x, y, w, h, pos, 0, 0, st, s); return; }
        float cx = x + w * 0.5f; g.setColour(col::black); g.fillRoundedRectangle(cx - 3, (float)y, 6, (float)h, 3);
        g.setColour(col::orangeDim); g.fillRoundedRectangle(cx - 3, pos, 6, y + h - pos, 3);
        for (int i = 0; i <= 10; ++i) { float ty = y + h * i / 10.f; g.setColour(col::line); g.drawLine(cx - 12, ty, cx - 7, ty, 1); }
        auto cap = juce::Rectangle<float>(cx - 13, pos - 8, 26, 16);
        juce::ColourGradient grad(col::alu, cap.getX(), cap.getY(), juce::Colour(0xff6d7078), cap.getX(), cap.getBottom(), false); g.setGradientFill(grad); g.fillRoundedRectangle(cap, 3);
        g.setColour(col::black); g.drawLine(cap.getX() + 3, pos, cap.getRight() - 3, pos, 2);
        juce::ignoreUnused(s);
    }
    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour& bgc, bool over, bool down) override {
        auto r = b.getLocalBounds().toFloat().reduced(0.5f); bool on = b.getToggleState();
        auto c = on ? b.findColour(juce::TextButton::buttonOnColourId) : bgc; if (over) c = c.brighter(0.12f); if (down) c = c.darker(0.2f);
        g.setColour(c); g.fillRoundedRectangle(r, 4); g.setColour(on ? c.brighter(0.3f) : col::line); g.drawRoundedRectangle(r, 4, 1);
    }
    juce::Font getTextButtonFont(juce::TextButton&, int h) override { return font(std::min(13.f, h * 0.55f), true); }
    juce::Font getComboBoxFont(juce::ComboBox&) override { return font(13.f, true); }
    juce::Font getPopupMenuFont() override { return font(14.f); }
    juce::Label* createSliderTextBox(juce::Slider& s) override { auto* l = LookAndFeel_V4::createSliderTextBox(s); l->setFont(font(11.5f, true)); l->setJustificationType(juce::Justification::centred); return l; }
};

struct KSlider : juce::Slider {
    std::function<void()> onMenu;
    void mouseDown(const juce::MouseEvent& e) override { if (e.mods.isPopupMenu()) { if (onMenu) onMenu(); return; } Slider::mouseDown(e); }
    void mouseDrag(const juce::MouseEvent& e) override { if (!e.mods.isPopupMenu()) Slider::mouseDrag(e); }
    void mouseUp(const juce::MouseEvent& e) override { if (!e.mods.isPopupMenu()) Slider::mouseUp(e); }
};

/// A knob bound to one plugin parameter by index. Rebinding is cheap (used when the selected channel changes).
class Knob : public juce::Component, public juce::SettableTooltipClient {
public:
    Knob() {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag); s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 15);
        s.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
        s.setRange(0, 1); s.setVelocityBasedMode(false); s.setMouseDragSensitivity(220);
        s.onDragStart = [this] { if (p) p->beginChangeGesture(); dragging = true; };
        s.onDragEnd = [this] { if (p) p->endChangeGesture(); dragging = false; };
        s.onValueChange = [this] { if (p && std::abs(p->getValue() - (float)s.getValue()) > 1e-7f) { if (!dragging) p->beginChangeGesture(); p->setValueNotifyingHost((float)s.getValue()); if (!dragging) p->endChangeGesture(); } };
        s.textFromValueFunction = [this](double v) { return text((float)v); };
        s.valueFromTextFunction = [this](const juce::String& t) { return p ? (double)p->getValueForText(t) : 0.0; };
        s.onMenu = [this] { showMenu(); }; addAndMakeVisible(s);
    }
    void bind(BakeryProcessor* proc, int index, const juce::String& lbl, const juce::String& tip = {}) {
        P = proc; idx = index; p = proc && index >= 0 ? proc->param(index) : nullptr; label = lbl; setTooltip(tip); s.setTooltip(tip);
        setVisible(p != nullptr); if (!p) return;
        int steps = p->getNumSteps(); bool discrete = steps > 1 && steps < 200;
        s.setRange(0, 1, discrete ? 1.0 / (steps - 1) : 0.0);
        s.setDoubleClickReturnValue(true, p->getDefaultValue());
        const auto& d = bk::registry()[(size_t)index]; s.getProperties().set("bipolar", d.lo < 0 && d.hi > 0 && std::abs(d.lo + d.hi) < 1e-3f);
        s.setValue(p->getValue(), juce::dontSendNotification); s.updateText(); repaint();
    }
    /// For FX-slot knobs: show the real unit of the selected effect type.
    void setFx(int typeParamIndex, int knobIndex) { fxType = typeParamIndex; fxKnob = knobIndex; }
    int index() const { return idx; }
    void refresh() {
        if (!p) return;
        if (!dragging && std::abs(s.getValue() - p->getValue()) > 1e-6) s.setValue(p->getValue(), juce::dontSendNotification);
        int cc = P->ccFor(idx); bool lk = P->locks.isLocked(idx), lrn = P->learning() == idx; int ln = 0; for (auto& m : P->macroLinks) for (auto& l : m) ln += l.param == idx;
        juce::String flags = (lrn ? "LEARN " : "") + (cc >= 0 ? "CC" + juce::String(cc) + " " : juce::String()) + (lk ? "LOCK " : "") + (ln ? "M " : "");
        if (flags != status) { status = flags; repaint(); }
        s.updateText();
    }
    juce::String text(float v) const {
        if (!p) return {};
        if (fxType >= 0 && fxKnob >= 0) return bk::fxText((int)P->value(fxType), fxKnob, v);
        return p->getText(v, 32);
    }
    void resized() override { auto r = getLocalBounds(); r.removeFromTop(14); s.setBounds(r); }
    void paint(juce::Graphics& g) override {
        g.setColour(col::cream); g.setFont(font(11.5f, true)); g.drawFittedText(label, getLocalBounds().removeFromTop(14), juce::Justification::centred, 1, 0.8f);
        if (status.isNotEmpty()) { g.setColour(P->learning() == idx ? col::red : col::blue); g.setFont(font(9.f, true)); g.drawText(status.trim(), getLocalBounds().removeFromTop(26).removeFromBottom(10), juce::Justification::centredRight); }
    }
    void mouseDown(const juce::MouseEvent& e) override { if (e.mods.isPopupMenu()) showMenu(); }
    void showMenu() {
        if (!p) return;
        juce::PopupMenu m, mac; int cc = P->ccFor(idx);
        m.addSectionHeader(p->getName(64));
        m.addItem(1, P->learning() == idx ? "Cancel MIDI Learn" : "MIDI Learn (move a knob on your controller)");
        if (cc >= 0) m.addItem(2, "Clear MIDI Learn (CC " + juce::String(cc) + ")");
        m.addItem(3, "Lock (Random leaves this alone)", true, P->locks.isLocked(idx));
        bool isMacro = idx >= bk::gp(bk::G_MACRO1) && idx <= bk::gp(bk::G_MACRO8);
        if (!isMacro) { for (int i = 0; i < 8; ++i) mac.addItem(10 + i, "Macro " + juce::String(i + 1)); m.addSubMenu("Link to Macro", mac); m.addItem(4, "Unlink from macros"); }
        m.addItem(5, "Reset to default");
        auto safe = juce::Component::SafePointer<Knob>(this);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this), [safe](int r) {
            if (!safe || r == 0) return; auto& k = *safe; auto* P = k.P;
            if (r == 1) P->startLearn(P->learning() == k.idx ? -1 : k.idx);
            else if (r == 2) P->clearLearn(k.idx);
            else if (r == 3) { if (P->locks.isLocked(k.idx)) P->locks.params.erase(k.idx); else P->locks.params.insert(k.idx); }
            else if (r == 4) P->unlinkMacros(k.idx);
            else if (r == 5) { k.p->beginChangeGesture(); k.p->setValueNotifyingHost(k.p->getDefaultValue()); k.p->endChangeGesture(); }
            else if (r >= 10) P->linkMacro(r - 10, k.idx);
            k.refresh(); });
    }
    KSlider s;
private:
    BakeryProcessor* P = nullptr; juce::RangedAudioParameter* p = nullptr; int idx = -1, fxType = -1, fxKnob = -1; bool dragging = false; juce::String label, status;
};

/// Choice parameter as a combo box.
class Choice : public juce::ComboBox {
public:
    Choice() { onChange = [this] { if (!p || getSelectedId() <= 0) return; float n = p->convertTo0to1((float)(getSelectedId() - 1) + lo); if (std::abs(n - p->getValue()) > 1e-6f) { p->beginChangeGesture(); p->setValueNotifyingHost(n); p->endChangeGesture(); } }; }
    void bind(BakeryProcessor* proc, int index, const juce::String& tip = {}) {
        P = proc; idx = index; p = proc->param(index); const auto& d = bk::registry()[(size_t)index]; lo = d.lo; clear(juce::dontSendNotification);
        if (!d.choices.empty()) { int i = 1; for (auto& c : d.choices) addItem(c, i++); }
        else for (int v = (int)d.lo; v <= (int)d.hi; ++v) addItem(juce::String(v), v - (int)d.lo + 1);
        setTooltip(tip); refresh();
    }
    void refresh() { if (p && !isPopupActive()) { int id = (int)std::lround(P->value(idx) - lo) + 1; if (id != getSelectedId()) setSelectedId(id, juce::dontSendNotification); } }
    int index() const { return idx; }
private:
    BakeryProcessor* P = nullptr; juce::RangedAudioParameter* p = nullptr; int idx = -1; float lo = 0;
};

/// Toggle parameter as a lit button.
class Toggle : public juce::TextButton {
public:
    Toggle(const juce::String& t = {}, juce::Colour on = col::orange) : TextButton(t) { setColour(buttonOnColourId, on); onClick = [this] { if (!p) return; p->beginChangeGesture(); p->setValueNotifyingHost(p->getValue() > 0.5f ? 0.f : 1.f); p->endChangeGesture(); refresh(); }; }
    void bind(BakeryProcessor* proc, int index, const juce::String& tip = {}) { P = proc; idx = index; p = proc->param(index); setTooltip(tip); refresh(); }
    void bind(BakeryProcessor& proc, int index, const juce::String& tip = {}) { bind(&proc, index, tip); }
    void refresh() { if (p) setToggleState(p->getValue() > 0.5f, juce::dontSendNotification); }
private:
    BakeryProcessor* P = nullptr; juce::RangedAudioParameter* p = nullptr; int idx = -1;
};

inline void styleButton(juce::TextButton& b, juce::Colour on = col::orange) { b.setColour(juce::TextButton::buttonOnColourId, on); }

/// Small LED that flashes when its channel triggers.
class Led : public juce::Component {
public:
    void hit() { level = 1.f; repaint(); }
    void decay() { if (level > 0) { level = std::max(0.f, level - 0.18f); repaint(); } }
    juce::Colour colour = col::orange;
    void paint(juce::Graphics& g) override {
        auto r = getLocalBounds().toFloat().reduced(1); g.setColour(col::black); g.fillEllipse(r);
        g.setColour(colour.withAlpha(0.18f + 0.82f * level)); g.fillEllipse(r.reduced(1.5f));
        if (level > 0.2f) { g.setColour(colour.withAlpha(0.3f * level)); g.fillEllipse(r.expanded(3)); }
    }
private:
    float level = 0;
};

/// Stereo peak meter fed from the engine's atomics.
class Meter : public juce::Component {
public:
    void set(float v) { v = juce::jlimit(0.f, 2.f, v); float db = juce::Decibels::gainToDecibels(v, -60.f); float n = juce::jmap(db, -60.f, 6.f, 0.f, 1.f); level = std::max(n, level - 0.04f); repaint(); }
    void paint(juce::Graphics& g) override {
        auto r = getLocalBounds().toFloat(); g.setColour(col::black); g.fillRoundedRectangle(r, 2);
        auto f = r.reduced(2); float h = f.getHeight() * level; auto lit = f.removeFromBottom(h);
        juce::ColourGradient grad(col::green, 0, r.getBottom(), col::red, 0, r.getY(), false); grad.addColour(0.75, col::orange); g.setGradientFill(grad); g.fillRect(lit);
        float zero = r.getBottom() - 2 - (r.getHeight() - 4) * juce::jmap(0.f, -60.f, 6.f, 0.f, 1.f); g.setColour(col::cream.withAlpha(0.6f)); g.drawLine(r.getX(), zero, r.getRight(), zero, 1);
    }
private:
    float level = 0;
};
} // namespace ui
