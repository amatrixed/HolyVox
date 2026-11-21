#include "PluginProcessor.h"
#include "PluginEditor.h"

using APF = juce::AudioParameterFloat;
using APB = juce::AudioParameterBool;
using APC = juce::AudioParameterChoice;
using APInt = juce::AudioParameterInt;
using PID = juce::ParameterID;

juce::AudioProcessorValueTreeState::ParameterLayout HolyVoxAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    auto f = [&] (const char* id, const char* name, float lo, float hi, float def, float step = 0.0f)
    { l.add (std::make_unique<APF> (PID { id, 1 }, name, juce::NormalisableRange<float> (lo, hi, step), def)); };
    auto b = [&] (const char* id, const char* name, bool def)
    { l.add (std::make_unique<APB> (PID { id, 1 }, name, def)); };
    auto c = [&] (const char* id, const char* name, juce::StringArray items, int def)
    { l.add (std::make_unique<APC> (PID { id, 1 }, name, items, def)); };

    // top bar
    c ("noteSource", "Note Source", { "Auto", "MIDI" }, 0);
    l.add (std::make_unique<APInt> (PID { "minNote", 1 }, "Min Note", 24, 72, 40));
    f ("tolCents", "Tolerance Cents", 0.0f, 100.0f, 50.0f, 1.0f);
    f ("tolTime", "Tolerance Time", 0.0f, 400.0f, 60.0f, 1.0f);
    f ("gate", "Gate", -90.0f, -10.0f, -60.0f, 0.1f);
    f ("inGain", "Input Gain", -24.0f, 24.0f, 0.0f, 0.1f);
    f ("drive", "Drive", 0.0f, 1.0f, 0.2f);
    b ("arpOn", "Arpeggiator", false);
    c ("arpRate", "Arp Rate", { "1/4", "1/8", "1/16", "1/32" }, 1);
    b ("humanize", "Humanize", false);

    // note mapper
    c ("key", "Key", { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0);
    c ("scale", "Scale", { "Chromatic", "Major", "Minor", "Pentatonic" }, 1);
    c ("harmony", "Harmony", { "1st and 5th", "1st and 3rd", "1st and 8th", "3rd and 5th",
                               "Unison", "5th and 8th", "1st and 6th" }, 0);

    // voices
    b ("v1On", "Voice 1 On", true);
    f ("v1Tune", "Voice 1 Tune", -12.0f, 12.0f, 0.0f, 0.01f);
    f ("v1Formant", "Voice 1 Formant", -12.0f, 12.0f, 0.0f, 0.01f);
    f ("v1Gain", "Voice 1 Gain", -24.0f, 6.0f, 0.0f, 0.1f);
    b ("v2On", "Voice 2 On", true);
    f ("v2Tune", "Voice 2 Tune", -12.0f, 12.0f, 0.0f, 0.01f);
    f ("v2Formant", "Voice 2 Formant", -12.0f, 12.0f, 0.0f, 0.01f);
    f ("v2Gain", "Voice 2 Gain", -24.0f, 6.0f, -3.0f, 0.1f);

    // bottom row
    f ("mods", "Mods", 0.0f, 1.0f, 0.2f);
    f ("effects", "Effects", 0.0f, 1.0f, 0.25f);
    b ("exc", "Exc", false);
    f ("sibilance", "Sibilance", 0.0f, 1.0f, 0.5f);
    f ("correction", "Correction", 0.0f, 1.0f, 0.8f);
    f ("voice", "Voice", 0.0f, 1.0f, 0.0f);
    f ("synth", "Synth", 0.0f, 1.0f, 0.15f);
    f ("ovox", "HolyVox", 0.0f, 1.0f, 0.8f);
    f ("tone", "Tone", -1.0f, 1.0f, 0.0f);
    f ("out", "Out", -24.0f, 12.0f, 0.0f, 0.1f);
    b ("limiter", "Limiter", true);
    return l;
}

HolyVoxAudioProcessor::HolyVoxAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HOLYVOX", createLayout())
{
}

bool HolyVoxAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void HolyVoxAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    engine.prepare (sampleRate);
    reverb.setSampleRate (sampleRate);
    reverb.reset();
    outGainCur = 1.0f;
    scratchL.assign ((size_t) std::max (samplesPerBlock, 16), 0.0f);
    scratchR.assign ((size_t) std::max (samplesPerBlock, 16), 0.0f);
}

void HolyVoxAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    if (n == 0 || numCh == 0) return;
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch) buffer.clear (ch, 0, n);

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn()) engine.noteOn (m.getNoteNumber());
        else if (m.isNoteOff()) engine.noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) for (int k = 0; k < 128; ++k) engine.noteOff (k);
    }

    holyvox::EngineParams p;
    p.noteSourceMidi = (int) get ("noteSource") == 1;
    p.minNote = (int) get ("minNote");
    p.tolCents = get ("tolCents"); p.tolTimeMs = get ("tolTime");
    p.gateDb = get ("gate"); p.inGainDb = get ("inGain"); p.drive = get ("drive");
    p.arpOn = get ("arpOn") > 0.5f; p.arpRate = (int) get ("arpRate");
    p.humanize = get ("humanize") > 0.5f;
    p.key = (int) get ("key"); p.scale = (int) get ("scale"); p.harmony = (int) get ("harmony");
    p.v1On = get ("v1On") > 0.5f; p.v1Tune = get ("v1Tune"); p.v1Formant = get ("v1Formant"); p.v1GainDb = get ("v1Gain");
    p.v2On = get ("v2On") > 0.5f; p.v2Tune = get ("v2Tune"); p.v2Formant = get ("v2Formant"); p.v2GainDb = get ("v2Gain");
    p.mods = get ("mods"); p.exc = get ("exc") > 0.5f; p.sibilance = get ("sibilance");
    p.correction = get ("correction"); p.voiceMix = get ("voice"); p.synthMix = get ("synth");
    p.ovoxMix = get ("ovox"); p.tone = get ("tone");
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm()) p.bpm = *bpm;

    float* L = buffer.getWritePointer (0);
    float* R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    // The engine reads its input while writing its output, so copy the input first (no allocation here).
    if ((int) scratchL.size() < n) { scratchL.resize ((size_t) n); scratchR.resize ((size_t) n); }
    std::copy (L, L + n, scratchL.begin());
    if (R) std::copy (R, R + n, scratchR.begin());
    engine.process (scratchL.data(), R ? scratchR.data() : nullptr, L, R, n, p);

    // Effects (reverb)
    const float fx = get ("effects");
    if (fx > 0.001f)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.55f + 0.35f * fx; rp.damping = 0.45f; rp.width = 1.0f;
        rp.wetLevel = 0.35f * fx; rp.dryLevel = 1.0f - 0.25f * fx; rp.freezeMode = 0.0f;
        reverb.setParameters (rp);
        if (R) reverb.processStereo (L, R, n); else reverb.processMono (L, n);
    }

    // Output gain + limiter
    const float outT = juce::Decibels::decibelsToGain (get ("out"));
    const bool lim = get ("limiter") > 0.5f;
    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        outGainCur += (outT - outGainCur) * 0.01f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* d = buffer.getWritePointer (ch);
            float y = d[i] * outGainCur;
            if (lim)
            {
                const float a = std::fabs (y);
                if (a > 0.7f) y = std::copysign (0.7f + 0.3f * std::tanh ((a - 0.7f) / 0.3f), y);
            }
            if (! std::isfinite (y)) y = 0.0f;
            d[i] = y;
            peak = std::max (peak, std::fabs (y));
        }
    }
    meterSmooth = std::max (peak, meterSmooth * 0.9f);
    meterLevel.store (std::min (1.0f, meterSmooth));
    displayNote.store (engine.getCurrentNote());
}

juce::AudioProcessorEditor* HolyVoxAudioProcessor::createEditor() { return new HolyVoxEditor (*this); }

void HolyVoxAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, destData);
}
void HolyVoxAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HolyVoxAudioProcessor(); }
