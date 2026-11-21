#pragma once
#include "PluginProcessor.h"

namespace icewave::colours
{
const juce::Colour bg       { 0xff0a111d };
const juce::Colour panel    { 0xff111b2b };
const juce::Colour border   { 0xff22344f };
const juce::Colour accent   { 0xff4fd1ff };
const juce::Colour accent2  { 0xff8f7dff };
const juce::Colour text     { 0xffd3e6ff };
const juce::Colour dim      { 0xff6f88aa };
}

class IceLookAndFeel : public juce::LookAndFeel_V4
{
public:
    IceLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
};

class WavetableDisplay : public juce::Component, private juce::Timer
{
public:
    WavetableDisplay (juce::AudioProcessorValueTreeState&, const juce::String& prefix);
    ~WavetableDisplay() override { stopTimer(); }
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    std::atomic<float>* on;
    std::atomic<float>* table;
    std::atomic<float>* pos;
    float lastPos = -1.0f, lastTable = -1.0f, lastOn = -1.0f;
};

class IceWaveEditor : public juce::AudioProcessorEditor
{
public:
    explicit IceWaveEditor (IceWaveProcessor&);
    ~IceWaveEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAtt> att;
    };

    struct Panel
    {
        juce::String title;
        juce::Rectangle<int> bounds;
        std::vector<Knob*> knobs;
        juce::ToggleButton* toggle = nullptr;
        juce::ComboBox* combo = nullptr;
        juce::Component* display = nullptr;
        int displayHeight = 0;
    };

    Knob* knob (const juce::String& id, const juce::String& label);
    juce::ToggleButton* toggle (const juce::String& id);
    juce::ComboBox* combo (const juce::String& id);
    Panel& panel (const juce::String& title, juce::Rectangle<int> bounds);
    void layoutPanel (Panel&);

    IceWaveProcessor& proc;
    IceLookAndFeel lnf;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<juce::ToggleButton>> toggles;
    std::vector<std::unique_ptr<juce::ComboBox>> combos;
    std::vector<std::unique_ptr<ButtonAtt>> buttonAtts;
    std::vector<std::unique_ptr<ComboAtt>> comboAtts;
    std::vector<Panel> panels;

    WavetableDisplay displayA, displayB;
    juce::ComboBox presetBox;
    juce::TextButton prevPreset { "<" }, nextPreset { ">" };
    Knob* masterKnob = nullptr;
    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IceWaveEditor)
};
