#pragma once
// HolyVox DSP engine - pure C++ (no JUCE dependency) so it can be unit tested.
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace holyvox
{
constexpr int kBands = 16;

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    inline float process (float xf)
    {
        const double x = xf;
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }
    void setBandpass (double sr, double f, double q);
    void setHighpass (double sr, double f);
    void setLowShelf (double sr, double f, double gainDb);
    void setHighShelf (double sr, double f, double gainDb);
};

struct Rng
{
    uint32_t s = 22222u;
    float next() { s = s * 1664525u + 1013904223u; return (float) (s >> 8) * (1.0f / 16777216.0f); }
    float bi() { return next() * 2.0f - 1.0f; }
};

struct EngineParams
{
    bool  noteSourceMidi = false;   // false = Auto (pitch tracking)
    int   minNote = 40;
    float tolCents = 50.0f, tolTimeMs = 60.0f;
    float gateDb = -60.0f, inGainDb = 0.0f, drive = 0.2f;
    bool  arpOn = false;
    int   arpRate = 1;              // 0=1/4 1=1/8 2=1/16 3=1/32
    bool  humanize = false;
    int   key = 0, scale = 0, harmony = 0;
    bool  v1On = true, v2On = true;
    float v1Tune = 0, v1Formant = 0, v1GainDb = 0;
    float v2Tune = 0, v2Formant = 0, v2GainDb = -3;
    float mods = 0.2f;
    bool  exc = false;
    float sibilance = 0.5f, correction = 0.8f;
    float voiceMix = 0.0f, synthMix = 0.15f, ovoxMix = 0.8f, tone = 0.0f;
    double bpm = 120.0;
};

class Engine
{
public:
    void prepare (double sampleRate);
    void noteOn (int note);
    void noteOff (int note);
    // inR / outR may be nullptr (mono)
    void process (const float* inL, const float* inR, float* outL, float* outR,
                  int numSamples, const EngineParams& p);

    int   getCurrentNote() const { return baseNote; }
    bool  isVoiced() const { return voiced; }
    float getDetectedMidi() const { return detMidi; }

private:
    struct Human
    {
        float pitchT = 0, pitch = 0, formT = 0, form = 0, ampT = 0, amp = 0;
        float vibRateT = 5.5f, vibRate = 5.5f, vibPh = 0, act = 0;
        int counter = 0;
        void tick (double sr, bool active, Rng& r, float humK, float actK);
        float pitchCents() const;
        float formSemis() const { return act * form; }
        float ampMul() const { return 1.0f + act * amp; }
    };
    struct Voice
    {
        double ph1 = 0, ph2 = 0;
        float curNote = 60, carrierEnv = 0, gain = 0, formCur = 0, appliedForm = 1000.0f;
        Biquad bands[kBands];
        Human hum;
    };

    bool inTrackScale (int n) const;
    bool inHarmScale (int n) const;
    int  stepScale (int n, int steps) const;
    int  quantize (float midi) const;
    void computeTargets();
    void trackPitch();
    void updateSlow (const EngineParams& p);

    double sr = 44100.0;
    EngineParams cp;
    Rng rng;
    double bandFreq[kBands] {};
    Biquad analysis[kBands];
    float  env[kBands] {};
    Voice  voice[2];
    Biquad sibHp, toneLo[2], toneHi[2];
    float  appliedTone = 1000.0f;

    // smoothing
    float smK = 0.01f, gAtk = 0.1f, gRel = 0.001f, gGate = 0.01f, aAtk = 0.01f, aRel = 0.001f;
    float humK = 0.0003f, actK = 0.0004f;
    float inGainCur = 1, voiceCur = 0, synthCur = 0, ovoxCur = 0, sibCur = 0, driveCur = 0, toneCur = 0;
    float inEnv = 0, gateGain = 0, gateLin = 0.001f;
    bool  gateOpen = false;
    float glideCoef = 1.0f;

    // notes
    int held[16] {}, heldCount = 0;
    int baseNote = 60, candNote = 60, candCounter = 0;
    int tgt[2] { 60, 67 };
    int trackMask = 0xFFF, harmMask = 0xAB5;
    int keyIdx = 0;
    int arpIdx = 0; double arpCounter = 0;
    double modPh = 0;
    int slowCounter = 0;

    // pitch detection
    std::vector<float> dec, pdBuf, nsdf;
    int decWrite = 0, decCnt = 0, decMask = 16383;
    float decAcc = 0;
    int pdMinLag = 15, pdMaxLag = 171, pdWindow = 427, hopSize = 512, hopCnt = 0;
    bool voiced = false;
    float detMidi = 60;
};
} // namespace holyvox
