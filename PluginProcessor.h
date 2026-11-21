#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "VocoderEngine.h"

class HolyVoxAudioProcessor : public juce::AudioProcessor
{
public:
    HolyVoxAudioProcessor();
    ~HolyVoxAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "HolyVox"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> meterLevel { 0.0f };
    std::atomic<int>   displayNote { -1 };

private:
    float get (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    holyvox::Engine engine;
    juce::Reverb reverb;
    double currentSampleRate = 44100.0;
    float outGainCur = 1.0f;
    float meterSmooth = 0.0f;
    std::vector<float> scratchL, scratchR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HolyVoxAudioProcessor)
};
