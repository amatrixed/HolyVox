#include "PluginEditor.h"

namespace
{
    const juce::Colour kPink   (0xfff0647c);
    const juce::Colour kPurple (0xffd46bd8);
    const juce::Colour kTeal   (0xff1fb5b0);
    const juce::Colour kGrey   (0xffd8d8e0);
}

HolyLookAndFeel::HolyLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff101016));
    setColour (juce::ComboBox::textColourId, juce::Colours::white);
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff55556a));
    setColour (juce::ComboBox::arrowColourId, juce::Colours::white);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1b1b22));
    setColour (juce::PopupMenu::textColourId, juce::Colours::white);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kTeal);
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff33333d));
    setColour (juce::TextButton::textColourOffId, juce::Colour (0xffb0b0bc));
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
}

void HolyLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                        float startAngle, float endAngle, juce::Slider& s)
{
    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const auto col = s.findColour (juce::Slider::rotarySliderFillColourId);

    g.setColour (juce::Colour (0xff0b0b10));
    g.fillEllipse (centre.x - radius - 3, centre.y - radius - 3, (radius + 3) * 2, (radius + 3) * 2);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius + 1.5f, radius + 1.5f, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff3a3a46));
    g.strokePath (track, juce::PathStrokeType (2.5f));

    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, radius + 1.5f, radius + 1.5f, 0.0f, startAngle, angle, true);
    g.setColour (col.brighter (0.5f));
    g.strokePath (arc, juce::PathStrokeType (2.5f));

    g.setGradientFill (juce::ColourGradient (col.brighter (0.35f), centre.x, centre.y - radius,
                                             col.darker (0.6f), centre.x, centre.y + radius, false));
    g.fillEllipse (centre.x - radius + 3, centre.y - radius + 3, (radius - 3) * 2, (radius - 3) * 2);

    juce::Path p;
    p.addRoundedRectangle (-1.8f, -(radius - 4.0f), 3.6f, radius * 0.45f, 1.5f);
    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

HolyVoxEditor::Knob& HolyVoxEditor::addKnob (const char* id, const char* text, juce::Colour c, const char* suffix)
{
    auto k = std::make_unique<Knob>();
    k->text = text;
    k->slider.setLookAndFeel (&laf);
    k->slider.setColour (juce::Slider::rotarySliderFillColourId, c);
    k->slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    k->slider.setTextValueSuffix (suffix);
    k->slider.setPopupDisplayEnabled (true, true, this);
    addAndMakeVisible (k->slider);
    k->att = std::make_unique<SliderAtt> (proc.apvts, id, k->slider);
    knobs.push_back (std::move (k));
    return *knobs.back();
}

HolyVoxEditor::Combo& HolyVoxEditor::addCombo (const char* id)
{
    auto c = std::make_unique<Combo>();
    c->box.setLookAndFeel (&laf);
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
        c->box.addItemList (choice->choices, 1);
    addAndMakeVisible (c->box);
    c->att = std::make_unique<ComboAtt> (proc.apvts, id, c->box);
    combos.push_back (std::move (c));
    return *combos.back();
}

HolyVoxEditor::Toggle& HolyVoxEditor::addToggle (const char* id, const char* text, juce::Colour on)
{
    auto t = std::make_unique<Toggle>();
    t->button.setLookAndFeel (&laf);
    t->button.setButtonText (text);
    t->button.setClickingTogglesState (true);
    t->button.setColour (juce::TextButton::buttonOnColourId, on);
    addAndMakeVisible (t->button);
    t->att = std::make_unique<ButtonAtt> (proc.apvts, id, t->button);
    toggles.push_back (std::move (t));
    return *toggles.back();
}

HolyVoxEditor::HolyVoxEditor (HolyVoxAudioProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setSize (820, 504);

    noteSource = &addCombo ("noteSource");
    minNote   = &addKnob ("minNote",  "Min Note",  kGrey);
    minNote->slider.textFromValueFunction = [] (double v) { return juce::MidiMessage::getMidiNoteName ((int) v, true, true, 3); };
    tolCents  = &addKnob ("tolCents", "Cents",     kGrey, " ct");
    tolTime   = &addKnob ("tolTime",  "Time",      kGrey, " ms");
    gate      = &addKnob ("gate",     "Gate",      kGrey, " dB");
    inGain    = &addKnob ("inGain",   "Gain",      kGrey, " dB");
    drive     = &addKnob ("drive",    "Drive",     kGrey);
    arpOn     = &addToggle ("arpOn", "ARP", kTeal);
    arpRate   = &addCombo ("arpRate");
    humanize  = &addToggle ("humanize", "HUMANIZE", kTeal);

    key = &addCombo ("key"); scale = &addCombo ("scale"); harmony = &addCombo ("harmony");

    v1On = &addToggle ("v1On", "HOLY 1", kPink);
    v1Tune = &addKnob ("v1Tune", "Tune", kPink, " st"); v1Formant = &addKnob ("v1Formant", "Formant", kPink, " st");
    v1Gain = &addKnob ("v1Gain", "Gain", kPink, " dB");
    v2On = &addToggle ("v2On", "HOLY 2", kPurple);
    v2Tune = &addKnob ("v2Tune", "Tune", kPurple, " st"); v2Formant = &addKnob ("v2Formant", "Formant", kPurple, " st");
    v2Gain = &addKnob ("v2Gain", "Gain", kPurple, " dB");

    mods = &addKnob ("mods", "Mods", kTeal); effects = &addKnob ("effects", "Effects", kTeal);
    exc = &addToggle ("exc", "EXC", kTeal);
    sibilance = &addKnob ("sibilance", "Sibilance", kGrey);
    correction = &addKnob ("correction", "Correction", kGrey);
    voice = &addKnob ("voice", "Voice", kGrey);
    synth = &addKnob ("synth", "Synth", kGrey);
    ovox = &addKnob ("ovox", "HolyVox", kGrey);
    tone = &addKnob ("tone", "Tone", kGrey);
    out = &addKnob ("out", "Out", kGrey, " dB");
    limiter = &addToggle ("limiter", "LIMIT", juce::Colour (0xffe8c547));

    startTimerHz (30);
}

HolyVoxEditor::~HolyVoxEditor()
{
    stopTimer();
    for (auto& k : knobs) k->slider.setLookAndFeel (nullptr);
    for (auto& c : combos) c->box.setLookAndFeel (nullptr);
    for (auto& t : toggles) t->button.setLookAndFeel (nullptr);
}

void HolyVoxEditor::resized()
{
    auto place = [] (Knob* k, int cx, int cy, int size) { k->slider.setBounds (cx - size / 2, cy - size / 2, size, size); };

    // top bar
    noteSource->box.setBounds (18, 34, 112, 26);
    place (minNote, 168, 46, 44); place (tolCents, 240, 46, 44); place (tolTime, 312, 46, 44);
    place (gate, 384, 46, 44);    place (inGain, 456, 46, 44);   place (drive, 528, 46, 44);
    arpOn->button.setBounds (578, 30, 70, 26);
    arpRate->box.setBounds (578, 62, 70, 24);
    humanize->button.setBounds (668, 26, 134, 60);

    // voice panels
    v1On->button.setBounds (36, 128, 110, 28);
    v2On->button.setBounds (544, 128, 110, 28);
    const int cxs1[3] = { 60, 156, 252 }, cxs2[3] = { 568, 664, 760 };
    place (v1Tune, cxs1[0], 250, 72); place (v1Formant, cxs1[1], 250, 72); place (v1Gain, cxs1[2], 250, 72);
    place (v2Tune, cxs2[0], 250, 72); place (v2Formant, cxs2[1], 250, 72); place (v2Gain, cxs2[2], 250, 72);

    // note mapper
    key->box.setBounds (326, 138, 76, 26);
    scale->box.setBounds (408, 138, 86, 26);
    harmony->box.setBounds (326, 170, 168, 26);

    // bottom row (11 cells)
    auto cx = [] (int i) { return 14 + 36 + 73 * i; };
    place (mods, cx (0), 414, 58);       place (effects, cx (1), 414, 58);
    exc->button.setBounds (cx (2) - 28, 401, 56, 26);
    place (sibilance, cx (3), 414, 58);  place (correction, cx (4), 414, 58);
    place (voice, cx (5), 414, 58);      place (synth, cx (6), 414, 58);
    place (ovox, cx (7), 414, 58);       place (tone, cx (8), 414, 58);
    place (out, cx (9), 414, 58);
    limiter->button.setBounds (cx (10) - 28, 401, 56, 26);
}

void HolyVoxEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff15151b));

    auto panel = [&] (juce::Rectangle<float> r, juce::Colour edge, float round = 12.0f)
    {
        g.setColour (juce::Colour (0xff23232c)); g.fillRoundedRectangle (r, round);
        g.setColour (edge); g.drawRoundedRectangle (r, round, 1.6f);
    };
    panel ({ 8, 6, 804, 100 }, juce::Colour (0xff44445a));
    panel ({ 12, 116, 288, 236 }, kPink.withAlpha (0.7f), 16.0f);
    panel ({ 520, 116, 288, 236 }, kPurple.withAlpha (0.7f), 16.0f);
    panel ({ 8, 362, 804, 134 }, juce::Colour (0xff44445a));

    // centre: note mapper + synthwave backdrop
    {
        juce::Rectangle<float> c (312, 116, 196, 236);
        g.setColour (juce::Colour (0xff0d0b1a)); g.fillRoundedRectangle (c, 12.0f);
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (312, 232, 196, 120);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a0f4a), 410, 232, juce::Colour (0xff0f6f78), 410, 352, false));
        g.fillRect (312, 232, 196, 120);
        g.setColour (kTeal.withAlpha (0.55f));
        for (int i = 0; i < 7; ++i) { const float yy = 290.0f + i * i * 1.6f; g.drawLine (312, yy, 508, yy, 1.0f); }
        for (int i = -6; i <= 6; ++i) g.drawLine (410 + i * 6.0f, 290, 410 + i * 34.0f, 352, 1.0f);
    }

    g.setFont (10.0f);
    g.setColour (juce::Colour (0xff9a9aae));
    g.drawText ("NOTE MAPPER", 312, 120, 196, 14, juce::Justification::centred);
    g.drawText ("NOTE SOURCE", 18, 18, 112, 14, juce::Justification::centredLeft);
    g.drawText ("TOLERANCE", 216, 14, 120, 12, juce::Justification::centred);
    g.drawText ("ARPEGGIATOR", 570, 14, 90, 12, juce::Justification::centred);

    // knob labels
    g.setFont (11.0f);
    g.setColour (juce::Colour (0xffc8c8d4));
    for (auto& k : knobs)
    {
        auto b = k->slider.getBounds();
        g.drawText (k->text, b.getX() - 12, b.getBottom() + 1, b.getWidth() + 24, 14, juce::Justification::centred);
    }

    // logo ring reacting to output level
    const float lvl = proc.meterLevel.load();
    const float cxl = 410.0f, cyl = 286.0f, r = 44.0f + lvl * 10.0f;
    g.setGradientFill (juce::ColourGradient (kPink, cxl - r, cyl, kTeal, cxl + r, cyl, false));
    g.drawEllipse (cxl - r, cyl - r, r * 2, r * 2, 5.0f);
    g.setColour (juce::Colour (0xff0b0b10));
    g.fillEllipse (cxl - 38, cyl - 38, 76, 76);
    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawText ("HOLYVOX", (int) cxl - 38, (int) cyl - 12, 76, 20, juce::Justification::centred);
    const int note = proc.displayNote.load();
    g.setFont (11.0f);
    g.setColour (kTeal);
    if (note >= 0)
        g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 3), (int) cxl - 38, (int) cyl + 8, 76, 14, juce::Justification::centred);
}
