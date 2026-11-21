#include "PluginEditor.h"

using namespace icewave;
namespace C = icewave::colours;

// ------------------------------------------------------------ LookAndFeel
IceLookAndFeel::IceLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, C::bg);
    setColour (juce::ComboBox::textColourId, C::text);
    setColour (juce::ComboBox::outlineColourId, C::border);
    setColour (juce::ComboBox::arrowColourId, C::accent);
    setColour (juce::PopupMenu::backgroundColourId, C::panel);
    setColour (juce::PopupMenu::textColourId, C::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, C::accent.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::Label::textColourId, C::dim);
    setColour (juce::BubbleComponent::backgroundColourId, C::panel);
    setColour (juce::BubbleComponent::outlineColourId, C::accent);
    setColour (juce::TooltipWindow::textColourId, C::text);
    setColour (juce::TextButton::buttonColourId, C::bg);
    setColour (juce::TextButton::textColourOffId, C::accent);
}

void IceLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                       float start, float end, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = start + pos * (end - start);
    const float lineW = 3.0f;
    const float arcR = radius - lineW * 0.5f;

    juce::Path bgArc;
    bgArc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, start, end, true);
    g.setColour (C::border);
    g.strokePath (bgArc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? (start + end) * 0.5f : start;
    juce::Path valArc;
    valArc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, std::min (from, angle), std::max (from, angle), true);
    juce::ColourGradient grad (C::accent2, centre.x - radius, centre.y + radius, C::accent, centre.x + radius, centre.y - radius, false);
    g.setGradientFill (grad);
    g.strokePath (valArc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float knobR = radius * 0.68f;
    g.setColour (C::bg);
    g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2, knobR * 2);
    g.setColour (C::border.brighter (0.2f));
    g.drawEllipse (centre.x - knobR, centre.y - knobR, knobR * 2, knobR * 2, 1.0f);

    juce::Path ptr;
    ptr.addRoundedRectangle (-1.5f, -knobR + 2.0f, 3.0f, knobR * 0.55f, 1.5f);
    g.setColour (C::text);
    g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (centre));
}

void IceLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (3.0f);
    const float d = std::min (r.getWidth(), r.getHeight());
    auto box = juce::Rectangle<float> (d, d).withCentre (r.getCentre());
    const bool on = b.getToggleState();
    g.setColour (on ? C::accent.withAlpha (0.25f) : C::bg);
    g.fillRoundedRectangle (box, 3.0f);
    g.setColour (on ? C::accent : C::border.brighter (0.3f));
    g.drawRoundedRectangle (box, 3.0f, 1.2f);
    if (on)
        g.fillEllipse (box.reduced (d * 0.3f));
}

void IceLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (C::bg);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (C::border.brighter (0.2f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (C::accent);
    g.fillPath (arrow);
}

juce::Font IceLookAndFeel::getComboBoxFont (juce::ComboBox&) { return juce::Font (juce::FontOptions (13.0f)); }
juce::Font IceLookAndFeel::getPopupMenuFont() { return juce::Font (juce::FontOptions (14.0f)); }

// ------------------------------------------------------------ Wavetable display
WavetableDisplay::WavetableDisplay (juce::AudioProcessorValueTreeState& a, const juce::String& p)
    : on (a.getRawParameterValue (p + "on")),
      table (a.getRawParameterValue (p + "table")),
      pos (a.getRawParameterValue (p + "pos"))
{
    startTimerHz (30);
}

void WavetableDisplay::timerCallback()
{
    const float p = pos->load(), t = table->load(), o = on->load();
    if (! juce::exactlyEqual (p, lastPos) || ! juce::exactlyEqual (t, lastTable) || ! juce::exactlyEqual (o, lastOn))
    {
        lastPos = p; lastTable = t; lastOn = o;
        repaint();
    }
}

void WavetableDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (C::bg);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (C::border);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

    const auto& bank = WavetableBank::get();
    const int t = juce::jlimit (0, kNumTables - 1, (int) table->load());
    const float p = pos->load();
    const bool active = on->load() > 0.5f;
    const int mip = 3;
    const int points = 160;

    auto area = r.reduced (12.0f, 10.0f);
    const int layers = 14;
    const float dx = area.getWidth() * 0.18f / layers;
    const float dy = area.getHeight() * 0.32f / layers;
    const float waveW = area.getWidth() - dx * layers;
    const float waveH = area.getHeight() - dy * layers;

    auto drawFrame = [&] (float framePos, float ox, float oy, float h) {
        const float fp = framePos * (kFrames - 1);
        const int f0 = std::min ((int) fp, kFrames - 1);
        const int f1 = std::min (f0 + 1, kFrames - 1);
        const float fr = fp - (float) f0;
        const float* a = bank.frame (t, f0, mip);
        const float* b = bank.frame (t, f1, mip);
        juce::Path path;
        for (int i = 0; i <= points; ++i)
        {
            const int idx = std::min (kTableSize - 1, i * kTableSize / points);
            const float s = a[idx] + (b[idx] - a[idx]) * fr;
            const float x = ox + waveW * (float) i / points;
            const float y = oy + h * 0.5f - s * h * 0.45f;
            if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
        }
        return path;
    };

    for (int l = layers - 1; l >= 0; --l)
    {
        const float fpos = (float) l / (layers - 1);
        const float ox = area.getX() + dx * l;
        const float oy = area.getBottom() - waveH - dy * l;
        auto path = drawFrame (fpos, ox, oy, waveH);
        const float near = 1.0f - std::abs (fpos - p) * 3.0f;
        g.setColour (C::dim.withAlpha (active ? 0.12f + 0.18f * std::max (0.0f, near) : 0.07f));
        g.strokePath (path, juce::PathStrokeType (1.0f));
    }

    const float ox = area.getX() + dx * p * (layers - 1);
    const float oy = area.getBottom() - waveH - dy * p * (layers - 1);
    auto main = drawFrame (p, ox, oy, waveH);
    const auto col = active ? C::accent : C::dim.withAlpha (0.4f);
    g.setColour (col.withAlpha (0.25f));
    g.strokePath (main, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (col);
    g.strokePath (main, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (C::dim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("Frame " + juce::String (juce::roundToInt (p * (kFrames - 1)) + 1) + " / " + juce::String (kFrames),
                r.reduced (8, 4), juce::Justification::topRight);
}

// ------------------------------------------------------------ Editor
IceWaveEditor::IceWaveEditor (IceWaveProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      displayA (p.apvts, "oscA_"), displayB (p.apvts, "oscB_"),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);
    panels.reserve (16);

    auto& oA = panel ("OSC A", { 10, 60, 460, 232 });
    oA.toggle = toggle ("oscA_on");
    oA.combo = combo ("oscA_table");
    oA.display = &displayA; oA.displayHeight = 112;
    addAndMakeVisible (displayA);
    oA.knobs = { knob ("oscA_pos", "WT POS"), knob ("oscA_semi", "SEMI"), knob ("oscA_fine", "FINE"),
                 knob ("oscA_uni", "UNISON"), knob ("oscA_detune", "DETUNE"), knob ("oscA_level", "LEVEL") };

    auto& oB = panel ("OSC B", { 480, 60, 460, 232 });
    oB.toggle = toggle ("oscB_on");
    oB.combo = combo ("oscB_table");
    oB.display = &displayB; oB.displayHeight = 112;
    addAndMakeVisible (displayB);
    oB.knobs = { knob ("oscB_pos", "WT POS"), knob ("oscB_semi", "SEMI"), knob ("oscB_fine", "FINE"),
                 knob ("oscB_uni", "UNISON"), knob ("oscB_detune", "DETUNE"), knob ("oscB_level", "LEVEL") };

    auto& sn = panel ("SUB / NOISE", { 950, 60, 220, 232 });
    sn.knobs = { knob ("sub_level", "SUB"), knob ("noise_level", "NOISE") };

    auto& f = panel ("FILTER", { 10, 300, 350, 130 });
    f.combo = combo ("flt_type");
    f.knobs = { knob ("flt_cutoff", "CUTOFF"), knob ("flt_reso", "RES"), knob ("flt_drive", "DRIVE"),
                knob ("flt_env", "ENV 2"), knob ("flt_key", "KEY") };

    auto& e1 = panel ("ENV 1  (AMP)", { 370, 300, 240, 130 });
    e1.knobs = { knob ("env1_a", "A"), knob ("env1_d", "D"), knob ("env1_s", "S"), knob ("env1_r", "R") };

    auto& e2 = panel ("ENV 2  (MOD)", { 620, 300, 290, 130 });
    e2.knobs = { knob ("env2_a", "A"), knob ("env2_d", "D"), knob ("env2_s", "S"), knob ("env2_r", "R"), knob ("env2_pos", "> WT POS") };

    auto& lf = panel ("LFO", { 920, 300, 250, 130 });
    lf.combo = combo ("lfo_shape");
    lf.knobs = { knob ("lfo_rate", "RATE"), knob ("lfo_pos", "> WT"), knob ("lfo_cut", "> CUT"), knob ("lfo_pitch", "> PITCH") };

    auto& d = panel ("DISTORTION", { 10, 438, 220, 130 });
    d.toggle = toggle ("dist_on");
    d.knobs = { knob ("dist_drive", "DRIVE"), knob ("dist_mix", "MIX") };

    auto& ch = panel ("CHORUS", { 240, 438, 300, 130 });
    ch.toggle = toggle ("cho_on");
    ch.knobs = { knob ("cho_rate", "RATE"), knob ("cho_depth", "DEPTH"), knob ("cho_mix", "MIX") };

    auto& dl = panel ("DELAY", { 550, 438, 300, 130 });
    dl.toggle = toggle ("dly_on");
    dl.knobs = { knob ("dly_time", "TIME"), knob ("dly_fb", "FEEDBACK"), knob ("dly_mix", "MIX") };

    auto& rv = panel ("REVERB", { 860, 438, 310, 130 });
    rv.toggle = toggle ("rev_on");
    rv.knobs = { knob ("rev_size", "SIZE"), knob ("rev_damp", "DAMP"), knob ("rev_mix", "MIX") };

    masterKnob = knob ("master", "MASTER");

    presetBox.addItemList (IceWaveProcessor::getPresetNames(), 1);
    presetBox.setSelectedItemIndex (proc.getCurrentPreset(), juce::dontSendNotification);
    presetBox.onChange = [this] {
        const int i = presetBox.getSelectedItemIndex();
        if (i >= 0) proc.loadPreset (i);
    };
    addAndMakeVisible (presetBox);

    auto step = [this] (int dir) {
        const int n = presetBox.getNumItems();
        presetBox.setSelectedItemIndex ((presetBox.getSelectedItemIndex() + dir + n) % n, juce::sendNotification);
    };
    prevPreset.onClick = [step] { step (-1); };
    nextPreset.onClick = [step] { step (1); };
    addAndMakeVisible (prevPreset);
    addAndMakeVisible (nextPreset);

    keyboard.setAvailableRange (24, 108);
    keyboard.setLowestVisibleKey (24);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, C::accent.withAlpha (0.6f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, C::accent.withAlpha (0.2f));
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffdfe9f5));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff141c28));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff7d8ca3));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (keyboard);

    setSize (1180, 700);
}

IceWaveEditor::~IceWaveEditor() { setLookAndFeel (nullptr); }

IceWaveEditor::Knob* IceWaveEditor::knob (const juce::String& id, const juce::String& label)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    k->slider.setPopupDisplayEnabled (true, true, this);
    k->slider.setDoubleClickReturnValue (true, (double) proc.apvts.getParameter (id)->convertFrom0to1 (proc.apvts.getParameter (id)->getDefaultValue()));
    k->att = std::make_unique<SliderAtt> (proc.apvts, id, k->slider);
    k->label.setText (label, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    k->label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    knobs.push_back (std::move (k));
    return knobs.back().get();
}

juce::ToggleButton* IceWaveEditor::toggle (const juce::String& id)
{
    auto b = std::make_unique<juce::ToggleButton>();
    buttonAtts.push_back (std::make_unique<ButtonAtt> (proc.apvts, id, *b));
    addAndMakeVisible (*b);
    toggles.push_back (std::move (b));
    return toggles.back().get();
}

juce::ComboBox* IceWaveEditor::combo (const juce::String& id)
{
    auto c = std::make_unique<juce::ComboBox>();
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
        c->addItemList (choice->choices, 1);
    comboAtts.push_back (std::make_unique<ComboAtt> (proc.apvts, id, *c));
    addAndMakeVisible (*c);
    combos.push_back (std::move (c));
    return combos.back().get();
}

IceWaveEditor::Panel& IceWaveEditor::panel (const juce::String& title, juce::Rectangle<int> bounds)
{
    Panel p;
    p.title = title;
    p.bounds = bounds;
    panels.push_back (p);
    return panels.back();
}

void IceWaveEditor::layoutPanel (Panel& p)
{
    auto r = p.bounds.reduced (10, 6);
    auto top = r.removeFromTop (24);
    if (p.toggle != nullptr) p.toggle->setBounds (top.removeFromLeft (24));
    if (p.combo != nullptr) p.combo->setBounds (top.removeFromRight (p.display != nullptr ? 150 : 110).reduced (0, 1));
    r.removeFromTop (4);

    if (p.display != nullptr)
    {
        p.display->setBounds (r.removeFromTop (p.displayHeight));
        r.removeFromTop (6);
    }

    if (p.knobs.empty()) return;
    const int n = (int) p.knobs.size();
    const int colW = r.getWidth() / n;
    const int knobSize = std::min (54, std::min (colW - 4, r.getHeight() - 18));
    for (int i = 0; i < n; ++i)
    {
        auto col = juce::Rectangle<int> (r.getX() + i * colW, r.getY(), colW, r.getHeight());
        auto k = p.knobs[(size_t) i];
        const int totalH = knobSize + 16;
        auto area = col.withSizeKeepingCentre (colW, totalH);
        k->slider.setBounds (area.removeFromTop (knobSize).withSizeKeepingCentre (knobSize, knobSize));
        k->label.setBounds (area);
    }
}

void IceWaveEditor::paint (juce::Graphics& g)
{
    g.fillAll (C::bg);

    // header
    juce::ColourGradient hg (C::panel, 0, 0, C::bg, 0, 52, false);
    g.setGradientFill (hg);
    g.fillRect (0, 0, getWidth(), 52);
    g.setColour (C::border);
    g.drawHorizontalLine (52, 0.0f, (float) getWidth());

    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    juce::ColourGradient tg (C::accent, 18, 0, C::accent2, 180, 0, false);
    g.setGradientFill (tg);
    g.drawText ("ICE WAVE", 18, 8, 200, 36, juce::Justification::centredLeft);
    g.setColour (C::dim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("wavetable synthesizer", 150, 8, 200, 36, juce::Justification::centredLeft);

    for (auto& p : panels)
    {
        auto r = p.bounds.toFloat();
        g.setColour (C::panel);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (C::border);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

        auto titleArea = p.bounds.reduced (10, 6).removeFromTop (24);
        if (p.toggle != nullptr) titleArea.removeFromLeft (28);
        g.setColour (C::text);
        g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
        g.drawText (p.title, titleArea, juce::Justification::centredLeft);
    }
}

void IceWaveEditor::resized()
{
    prevPreset.setBounds (430, 13, 26, 26);
    presetBox.setBounds (460, 13, 260, 26);
    nextPreset.setBounds (724, 13, 26, 26);

    masterKnob->slider.setBounds (1080, 4, 44, 44);
    masterKnob->label.setBounds (1010, 4, 70, 44);
    masterKnob->label.setJustificationType (juce::Justification::centredRight);

    for (auto& p : panels)
        layoutPanel (p);

    keyboard.setBounds (10, 576, getWidth() - 20, 114);
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 50.0f);
}
