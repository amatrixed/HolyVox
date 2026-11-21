#include "VocoderEngine.h"

namespace holyvox
{
static constexpr double PI = 3.14159265358979323846;
static inline float dbToLin (float db) { return std::pow (10.0f, db * 0.05f); }

// ---------------------------------------------------------------- Biquad
void Biquad::setBandpass (double sr, double f, double q)
{
    f = std::min (std::max (f, 20.0), sr * 0.45);
    const double w = 2 * PI * f / sr, s = std::sin (w), c = std::cos (w);
    const double alpha = s / (2 * q), a0 = 1 + alpha;
    b0 = alpha / a0; b1 = 0; b2 = -alpha / a0; a1 = -2 * c / a0; a2 = (1 - alpha) / a0;
}
void Biquad::setHighpass (double sr, double f)
{
    const double w = 2 * PI * f / sr, s = std::sin (w), c = std::cos (w);
    const double alpha = s / (2 * 0.7071), a0 = 1 + alpha;
    b0 = (1 + c) * 0.5 / a0; b1 = -(1 + c) / a0; b2 = (1 + c) * 0.5 / a0;
    a1 = -2 * c / a0; a2 = (1 - alpha) / a0;
}
void Biquad::setLowShelf (double sr, double f, double gainDb)
{
    const double A = std::pow (10.0, gainDb / 40.0), w = 2 * PI * f / sr;
    const double s = std::sin (w), c = std::cos (w);
    const double sq = 2 * std::sqrt (A) * (s / 2.0 * std::sqrt (2.0));
    const double a0 = (A + 1) + (A - 1) * c + sq;
    b0 = A * ((A + 1) - (A - 1) * c + sq) / a0;
    b1 = 2 * A * ((A - 1) - (A + 1) * c) / a0;
    b2 = A * ((A + 1) - (A - 1) * c - sq) / a0;
    a1 = -2 * ((A - 1) + (A + 1) * c) / a0;
    a2 = ((A + 1) + (A - 1) * c - sq) / a0;
}
void Biquad::setHighShelf (double sr, double f, double gainDb)
{
    const double A = std::pow (10.0, gainDb / 40.0), w = 2 * PI * f / sr;
    const double s = std::sin (w), c = std::cos (w);
    const double sq = 2 * std::sqrt (A) * (s / 2.0 * std::sqrt (2.0));
    const double a0 = (A + 1) - (A - 1) * c + sq;
    b0 = A * ((A + 1) + (A - 1) * c + sq) / a0;
    b1 = -2 * A * ((A - 1) + (A + 1) * c) / a0;
    b2 = A * ((A + 1) + (A - 1) * c - sq) / a0;
    a1 = 2 * ((A - 1) - (A + 1) * c) / a0;
    a2 = ((A + 1) - (A - 1) * c - sq) / a0;
}

// ---------------------------------------------------------------- Humanizer
void Engine::Human::tick (double sr, bool active, Rng& r, float humK, float actK)
{
    act += ((active ? 1.0f : 0.0f) - act) * actK;
    if (--counter <= 0)
    {
        counter = (int) (sr * (0.06 + 0.12 * r.next()));
        pitchT = r.bi() * 12.0f;          // +-12 cents drift
        formT  = r.bi() * 0.35f;          // +-0.35 semitone formant wobble
        ampT   = r.bi() * 0.12f;          // +-12 % level variation
        vibRateT = 4.8f + 1.4f * r.next();
    }
    pitch += (pitchT - pitch) * humK;
    form  += (formT - form) * humK;
    amp   += (ampT - amp) * humK;
    vibRate += (vibRateT - vibRate) * humK;
    vibPh += (float) (2.0 * PI * vibRate / sr);
    if (vibPh > 6.2831853f) vibPh -= 6.2831853f;
}
float Engine::Human::pitchCents() const { return act * (pitch + 5.0f * std::sin (vibPh)); }

// ---------------------------------------------------------------- Setup
void Engine::prepare (double sampleRate)
{
    sr = sampleRate;
    for (int b = 0; b < kBands; ++b)
    {
        bandFreq[b] = 110.0 * std::pow (8500.0 / 110.0, b / double (kBands - 1));
        analysis[b].setBandpass (sr, bandFreq[b], 5.0);
        analysis[b].reset();
        env[b] = 0;
    }
    for (auto& v : voice)
    {
        for (int b = 0; b < kBands; ++b) { v.bands[b].setBandpass (sr, bandFreq[b], 5.0); v.bands[b].reset(); }
        v.appliedForm = 0; v.formCur = 0; v.carrierEnv = 0; v.gain = 0; v.ph1 = v.ph2 = 0;
        v.hum = Human();
    }
    voice[1].hum.counter = 700;
    sibHp.setHighpass (sr, 5500.0); sibHp.reset();
    for (int c = 0; c < 2; ++c) { toneLo[c].reset(); toneHi[c].reset(); }
    appliedTone = 1000.0f;

    auto coef = [this] (double seconds) { return (float) (1.0 - std::exp (-1.0 / (sr * seconds))); };
    smK = coef (0.02); gAtk = coef (0.001); gRel = coef (0.05); gGate = coef (0.005);
    aAtk = coef (0.002); aRel = coef (0.015); humK = coef (0.08); actK = coef (0.05);

    const double dsr = sr / 4.0;
    pdMinLag = std::max (2, (int) std::ceil (dsr / 800.0));
    pdMaxLag = (int) (dsr / 70.0);
    pdWindow = (int) (2.5 * pdMaxLag);
    pdBuf.assign ((size_t) (pdWindow + pdMaxLag + 4), 0.0f);
    nsdf.assign ((size_t) (pdMaxLag + 4), 0.0f);
    dec.assign ((size_t) (decMask + 1), 0.0f);
    decWrite = decCnt = hopCnt = 0; decAcc = 0;
    hopSize = std::max (64, (int) (sr * 0.0116));

    inEnv = 0; gateGain = 0; gateOpen = false; voiced = false;
    inGainCur = 1; voiceCur = synthCur = ovoxCur = sibCur = driveCur = toneCur = 0;
    heldCount = 0; baseNote = candNote = 60; candCounter = 0; arpIdx = 0; arpCounter = 0; slowCounter = 0;
    voice[0].curNote = voice[1].curNote = 60;
}

void Engine::noteOn (int note)
{
    for (int i = 0; i < heldCount; ++i)
        if (held[i] == note) { for (int j = i; j < heldCount - 1; ++j) held[j] = held[j + 1]; --heldCount; break; }
    if (heldCount == 16) { for (int j = 0; j < 15; ++j) held[j] = held[j + 1]; --heldCount; }
    held[heldCount++] = note;
}
void Engine::noteOff (int note)
{
    for (int i = 0; i < heldCount; ++i)
        if (held[i] == note) { for (int j = i; j < heldCount - 1; ++j) held[j] = held[j + 1]; --heldCount; break; }
}

// ---------------------------------------------------------------- Scales
bool Engine::inTrackScale (int n) const { return (trackMask >> (((n - keyIdx) % 12 + 12) % 12)) & 1; }
bool Engine::inHarmScale (int n) const  { return (harmMask  >> (((n - keyIdx) % 12 + 12) % 12)) & 1; }

int Engine::stepScale (int n, int steps) const
{
    for (int s = 0; s < steps; ++s)
    {
        int guard = 0;
        do { ++n; } while (! inHarmScale (n) && ++guard < 12);
    }
    return n;
}
int Engine::quantize (float m) const
{
    const int r = (int) std::lround (m);
    int best = r; float bd = 1e9f;
    for (int k = -2; k <= 2; ++k)
    {
        const int n = r + k;
        if (inTrackScale (n)) { const float d = std::fabs ((float) n - m); if (d < bd) { bd = d; best = n; } }
    }
    return best;
}
void Engine::computeTargets()
{
    static const int kHarm[][2] = { {0,4}, {0,2}, {0,7}, {2,4}, {0,0}, {4,7}, {0,5} };
    static const int kArp[4] = { 0, 2, 4, 7 };
    const int h = std::min (std::max (cp.harmony, 0), 6);
    const int a = cp.arpOn ? kArp[arpIdx & 3] : 0;
    tgt[0] = stepScale (baseNote, kHarm[h][0] + a);
    tgt[1] = stepScale (baseNote, kHarm[h][1] + a);
}

// ---------------------------------------------------------------- Pitch tracking (NSDF / McLeod style)
void Engine::trackPitch()
{
    const int W = pdWindow, minL = pdMinLag, maxL = pdMaxLag, total = W + maxL;
    for (int k = 0; k < total; ++k) pdBuf[(size_t) k] = dec[(size_t) ((decWrite - 1 - k) & decMask)];

    double e0 = 0;
    for (int k = 0; k < W; ++k) e0 += (double) pdBuf[(size_t) k] * pdBuf[(size_t) k];
    if (e0 < 1e-7 * W || inEnv < gateLin) { voiced = false; return; }

    double e1 = 0;
    for (int k = 0; k < W; ++k) e1 += (double) pdBuf[(size_t) (k + minL)] * pdBuf[(size_t) (k + minL)];
    for (int lag = minL; lag <= maxL; ++lag)
    {
        double r = 0;
        const float* a = pdBuf.data();
        const float* b = pdBuf.data() + lag;
        for (int k = 0; k < W; ++k) r += (double) a[k] * b[k];
        nsdf[(size_t) lag] = (float) (2.0 * r / (e0 + e1 + 1e-12));
        e1 += (double) pdBuf[(size_t) (lag + W)] * pdBuf[(size_t) (lag + W)]
            - (double) pdBuf[(size_t) lag] * pdBuf[(size_t) lag];
        if (e1 < 0) e1 = 0;
    }
    float best = 0;
    for (int l = minL + 1; l < maxL; ++l)
        if (nsdf[(size_t) l] > nsdf[(size_t) l - 1] && nsdf[(size_t) l] >= nsdf[(size_t) l + 1])
            best = std::max (best, nsdf[(size_t) l]);
    if (best < 0.7f) { voiced = false; return; }
    int pick = -1;
    for (int l = minL + 1; l < maxL; ++l)
        if (nsdf[(size_t) l] > nsdf[(size_t) l - 1] && nsdf[(size_t) l] >= nsdf[(size_t) l + 1]
            && nsdf[(size_t) l] >= 0.9f * best) { pick = l; break; }
    if (pick < 0) { voiced = false; return; }

    const float a = nsdf[(size_t) pick - 1], b = nsdf[(size_t) pick], c = nsdf[(size_t) pick + 1];
    const float den = a - 2 * b + c;
    const float shift = std::fabs (den) > 1e-9f ? 0.5f * (a - c) / den : 0.0f;
    const double lagF = pick + std::min (std::max (shift, -1.0f), 1.0f);
    const double freq = (sr / 4.0) / lagF;
    detMidi = (float) (69.0 + 12.0 * std::log2 (freq / 440.0));
    voiced = true;

    if (detMidi < (float) cp.minNote) return;
    const int cand = quantize (detMidi);
    if (cand == baseNote) { candCounter = 0; candNote = cand; return; }
    if (cand != candNote) { candNote = cand; candCounter = 0; }
    const float devCents = std::fabs (detMidi - (float) baseNote) * 100.0f;
    if (devCents >= cp.tolCents)
    {
        candCounter += hopSize;
        if (candCounter >= (int) (cp.tolTimeMs * 0.001 * sr)) { baseNote = cand; candCounter = 0; computeTargets(); }
    }
    else candCounter = 0;
}

// ---------------------------------------------------------------- Slow (per 64 samples) updates
void Engine::updateSlow (const EngineParams& p)
{
    for (int v = 0; v < 2; ++v)
    {
        Voice& vc = voice[v];
        const float target = v == 0 ? p.v1Formant : p.v2Formant;
        vc.formCur += (target - vc.formCur) * 0.25f;
        const float semi = vc.formCur + vc.hum.formSemis();
        if (std::fabs (semi - vc.appliedForm) > 0.003f)
        {
            const double ratio = std::exp2 ((double) semi / 12.0);
            for (int b = 0; b < kBands; ++b) vc.bands[b].setBandpass (sr, bandFreq[b] * ratio, 5.0);
            vc.appliedForm = semi;
        }
    }
    if (std::fabs (p.tone - appliedTone) > 0.001f)
    {
        for (int c = 0; c < 2; ++c)
        {
            toneLo[c].setLowShelf (sr, 300.0, -p.tone * 6.0);
            toneHi[c].setHighShelf (sr, 3000.0, p.tone * 6.0);
        }
        appliedTone = p.tone;
    }
}

static inline float polyBlep (double t, double dt)
{
    if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
    return 0.0f;
}

// ---------------------------------------------------------------- Main process
void Engine::process (const float* inL, const float* inR, float* outL, float* outR,
                      int n, const EngineParams& p)
{
    cp = p;
    keyIdx = ((p.key % 12) + 12) % 12;
    static const int kMasks[4] = { 0xFFF, 0xAB5, 0x5AD, 0x295 };   // chromatic, major, minor, pentatonic
    static const int kHarmMasks[4] = { 0xAB5, 0xAB5, 0x5AD, 0x295 };
    const int sc = std::min (std::max (p.scale, 0), 3);
    trackMask = kMasks[sc]; harmMask = kHarmMasks[sc];

    float carrierTarget = 1.0f;
    if (p.noteSourceMidi)
    {
        if (heldCount > 0) { baseNote = held[heldCount - 1]; carrierTarget = 1.0f; }
        else carrierTarget = 0.0f;
    }
    computeTargets();

    const float corr = std::min (std::max (p.correction, 0.0f), 1.0f);
    const double glideMs = 200.0 * (1.0 - corr) * (1.0 - corr);
    glideCoef = glideMs < 0.3 ? 1.0f : (float) (1.0 - std::exp (-1.0 / (sr * glideMs * 0.001)));
    gateLin = dbToLin (p.gateDb);

    const float inGainT = dbToLin (p.inGainDb);
    const float voiceT = p.voiceMix, synthT = p.synthMix, ovoxT = p.ovoxMix * 1.5f;
    const float sibT = p.sibilance, driveT = p.drive;
    const float vgT[2] = { p.v1On ? dbToLin (p.v1GainDb) : 0.0f, p.v2On ? dbToLin (p.v2GainDb) : 0.0f };
    const float tuneT[2] = { p.v1Tune, p.v2Tune };

    static const double kBeats[4] = { 1.0, 0.5, 0.25, 0.125 };
    const double stepSamples = sr * 60.0 / std::max (20.0, p.bpm) * kBeats[std::min (std::max (p.arpRate, 0), 3)];

    for (int i = 0; i < n; ++i)
    {
        if ((slowCounter++ & 63) == 0) updateSlow (p);

        float x = inR ? 0.5f * (inL[i] + inR[i]) : inL[i];
        inGainCur += (inGainT - inGainCur) * smK;
        x *= inGainCur;

        const float ax = std::fabs (x);
        inEnv += (ax > inEnv ? gAtk : gRel) * (ax - inEnv);
        gateOpen = gateOpen ? (inEnv > gateLin * 0.7f) : (inEnv > gateLin);
        gateGain += ((gateOpen ? 1.0f : 0.0f) - gateGain) * gGate;

        decAcc += x;
        if (++decCnt == 4) { dec[(size_t) decWrite] = decAcc * 0.25f; decWrite = (decWrite + 1) & decMask; decAcc = 0; decCnt = 0; }
        if (++hopCnt >= hopSize) { hopCnt = 0; if (! p.noteSourceMidi) trackPitch(); }

        const float mod = x * gateGain;

        for (int b = 0; b < kBands; ++b)
        {
            const float r = std::fabs (analysis[b].process (mod));
            env[b] += (r > env[b] ? aAtk : aRel) * (r - env[b]);
        }

        if (p.arpOn)
        {
            arpCounter += 1.0;
            if (arpCounter >= stepSamples) { arpCounter -= stepSamples; ++arpIdx; computeTargets(); }
        }
        else if (arpIdx != 0) { arpIdx = 0; computeTargets(); }

        modPh += 2.0 * PI * 5.3 / sr; if (modPh > 2.0 * PI) modPh -= 2.0 * PI;

        voiceCur += (voiceT - voiceCur) * smK; synthCur += (synthT - synthCur) * smK;
        ovoxCur += (ovoxT - ovoxCur) * smK; sibCur += (sibT - sibCur) * smK;
        driveCur += (driveT - driveCur) * smK;

        float voc[2], raw[2];
        for (int v = 0; v < 2; ++v)
        {
            Voice& vc = voice[v];
            vc.hum.tick (sr, p.humanize, rng, humK, actK);
            vc.curNote += ((float) tgt[v] - vc.curNote) * glideCoef;
            vc.carrierEnv += (carrierTarget - vc.carrierEnv) * smK;
            vc.gain += (vgT[v] - vc.gain) * smK;

            const float vib = p.mods * 35.0f * (float) std::sin (modPh + v * 1.3);
            const float noteF = vc.curNote + tuneT[v] + (vc.hum.pitchCents() + vib) * 0.01f;
            double dt = 440.0 * std::exp2 (((double) noteF - 69.0) / 12.0) / sr;
            dt = std::min (std::max (dt, 1e-5), 0.45);
            const double dt2 = std::min (dt * 1.0059, 0.45);
            vc.ph1 += dt; if (vc.ph1 >= 1.0) vc.ph1 -= 1.0;
            vc.ph2 += dt2; if (vc.ph2 >= 1.0) vc.ph2 -= 1.0;
            const float saw1 = (float) (2.0 * vc.ph1 - 1.0) - polyBlep (vc.ph1, dt);
            const float saw2 = (float) (2.0 * vc.ph2 - 1.0) - polyBlep (vc.ph2, dt2);
            float c = 0.6f * saw1 + 0.4f * saw2;
            if (p.exc) c += 0.3f * rng.bi();
            const float g = 1.0f + driveCur * 6.0f;
            c = std::tanh (c * g) / std::tanh (g);
            c *= vc.carrierEnv;

            float sum = 0;
            for (int b = 0; b < kBands; ++b) sum += vc.bands[b].process (c) * env[b];
            const float am = vc.gain * vc.hum.ampMul();
            voc[v] = sum * 2.5f * am;
            raw[v] = c * 0.25f * am;
        }

        const float sib = sibHp.process (mod) * sibCur * 2.0f;
        const float dry = mod * voiceCur;
        float l = dry + sib + ovoxCur * (voc[0] * 0.85f + voc[1] * 0.45f) + synthCur * (raw[0] * 0.85f + raw[1] * 0.45f);
        float r = dry + sib + ovoxCur * (voc[0] * 0.45f + voc[1] * 0.85f) + synthCur * (raw[0] * 0.45f + raw[1] * 0.85f);
        l = toneHi[0].process (toneLo[0].process (l));
        r = toneHi[1].process (toneLo[1].process (r));
        if (outR) { outL[i] = l; outR[i] = r; }
        else outL[i] = 0.5f * (l + r);
    }
}
} // namespace holyvox
