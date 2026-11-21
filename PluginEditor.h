#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class HolyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    HolyLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
};

class HolyVoxEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit HolyVoxEditor (HolyVoxAudioProcessor&);
    ~HolyVoxEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob  { juce::Slider slider { juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox };
                   juce::String text; std::unique_ptr<SliderAtt> att; };
    struct Combo { juce::ComboBox box; std::unique_ptr<ComboAtt> att; };
    struct Toggle{ juce::TextButton button; std::unique_ptr<ButtonAtt> att; };

    Knob& addKnob (const char* id, const char* text, juce::Colour c, const char* suffix = "");
    Combo& addCombo (const char* id);
    Toggle& addToggle (const char* id, const char* text, juce::Colour on);
    void timerCallback() override { repaint (296, 196, 228, 170); }

    HolyVoxAudioProcessor& proc;
    HolyLookAndFeel laf;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Combo>> combos;
    std::vector<std::unique_ptr<Toggle>> toggles;

    Knob *minNote, *tolCents, *tolTime, *gate, *inGain, *drive;
    Knob *v1Tune, *v1Formant, *v1Gain, *v2Tune, *v2Formant, *v2Gain;
    Knob *mods, *effects, *sibilance, *correction, *voice, *synth, *ovox, *tone, *out;
    Combo *noteSource, *arpRate, *key, *scale, *harmony;
    Toggle *arpOn, *humanize, *v1On, *v2On, *exc, *limiter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HolyVoxEditor)
};
