#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr int kKnobSize  = 58;
    constexpr int kLabelH    = 16;
    constexpr int kHeaderH   = 38;
    constexpr int kPad       = 14;
    constexpr int kGap       = 8;
    constexpr int kTabH      = 320;
}

//==============================================================================
// SynthLookAndFeel
//==============================================================================

SynthLookAndFeel::SynthLookAndFeel()
{
    using namespace SynthColors;
    setColour (juce::Slider::textBoxTextColourId,               text);
    setColour (juce::Slider::textBoxOutlineColourId,            juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId,         panel);
    setColour (juce::Label::textColourId,                       subtext);
    setColour (juce::TabbedButtonBar::tabTextColourId,          subtext);
    setColour (juce::TabbedButtonBar::frontTextColourId,        text);
    setColour (juce::TabbedButtonBar::tabOutlineColourId,       border);
    setColour (juce::TabbedButtonBar::frontOutlineColourId,     accent);
    setColour (juce::TabbedComponent::backgroundColourId,       bg);
    setColour (juce::ComboBox::backgroundColourId,              panel);
    setColour (juce::ComboBox::textColourId,                    text);
    setColour (juce::ComboBox::outlineColourId,                 accent.withAlpha (0.5f));
    setColour (juce::ComboBox::arrowColourId,                   accent);
    setColour (juce::PopupMenu::backgroundColourId,             surface);
    setColour (juce::PopupMenu::textColourId,                   text);
    setColour (juce::TextButton::buttonColourId,                accent.withAlpha (0.18f));
    setColour (juce::TextButton::textColourOnId,                accent);
    setColour (juce::TextButton::textColourOffId,               accent);
}

void SynthLookAndFeel::drawRotarySlider (juce::Graphics& g,
    int x, int y, int w, int h,
    float sliderPos, float startAngle, float endAngle, juce::Slider& slider)
{
    using namespace SynthColors;
    const float cx     = x + w * 0.5f;
    const float cy     = y + h * 0.5f;
    const float outerR = juce::jmin (w, h) * 0.42f;
    const float trackW = outerR * 0.17f;
    const float arcR   = outerR - trackW * 0.5f;
    const float innerR = outerR - trackW;

    g.setColour (panel);
    g.fillEllipse (cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f);
    g.setColour (border.withAlpha (0.5f));
    g.drawEllipse (cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f, 0.6f);

    {
        juce::Path trackPath;
        trackPath.addArc (cx - arcR, cy - arcR, arcR * 2.0f, arcR * 2.0f,
                          startAngle, endAngle, true);
        g.setColour (track);
        g.strokePath (trackPath, juce::PathStrokeType (trackW,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float cur = startAngle + sliderPos * (endAngle - startAngle);
    if (sliderPos > 0.001f)
    {
        juce::Path val;
        val.addArc (cx - arcR, cy - arcR, arcR * 2.0f, arcR * 2.0f,
                    startAngle, cur, true);
        g.setColour (accent);
        g.strokePath (val, juce::PathStrokeType (trackW,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // LFO modulation ring on the cutoff knob
    if (lfoCutoffDepth != nullptr && slider.getComponentID() == "cutoff")
    {
        const float depth   = lfoCutoffDepth->load();
        if (depth > 0.001f)
        {
            const float totalArc = endAngle - startAngle;
            const float modArc   = depth * totalArc * 0.5f;
            const float modArcR  = arcR + trackW * 1.1f;
            const float modW     = trackW * 0.4f;
            juce::Path modPath;
            modPath.addArc (cx - modArcR, cy - modArcR,
                            modArcR * 2.0f, modArcR * 2.0f,
                            cur - modArc, cur + modArc, true);
            g.setColour (modRing.withAlpha (0.75f));
            g.strokePath (modPath, juce::PathStrokeType (modW,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    const float dr = trackW * 0.65f;
    g.setColour (text);
    g.fillEllipse (cx + (innerR * 0.52f) * std::sin (cur) - dr,
                   cy - (innerR * 0.52f) * std::cos (cur) - dr, dr * 2.0f, dr * 2.0f);
}

void SynthLookAndFeel::drawLinearSlider (juce::Graphics& g,
    int x, int y, int w, int h,
    float sliderPos, float, float,
    juce::Slider::SliderStyle style, juce::Slider&)
{
    using namespace SynthColors;
    if (style == juce::Slider::LinearHorizontal)
    {
        const float trackY = y + h * 0.5f;
        g.setColour (track);
        g.fillRoundedRectangle (static_cast<float>(x), trackY - 2.0f,
                                static_cast<float>(w), 4.0f, 2.0f);
        g.setColour (accent);
        g.fillRoundedRectangle (static_cast<float>(x), trackY - 2.0f,
                                sliderPos - x, 4.0f, 2.0f);
        g.setColour (text);
        g.fillEllipse (sliderPos - 6.0f, trackY - 6.0f, 12.0f, 12.0f);
    }
}

//==============================================================================
// ADSRVisualizer
//==============================================================================

ADSRVisualizer::ADSRVisualizer (juce::AudioProcessorValueTreeState& a) : apvts (a)
{
    startTimerHz (30);
}

void ADSRVisualizer::paint (juce::Graphics& g)
{
    using namespace SynthColors;
    const float A = apvts.getRawParameterValue ("attack")->load();
    const float D = apvts.getRawParameterValue ("decay")->load();
    const float S = apvts.getRawParameterValue ("sustain")->load();
    const float R = apvts.getRawParameterValue ("release")->load();

    const float total = A + D + 0.4f + R;
    const float W  = static_cast<float> (getWidth());
    const float H  = static_cast<float> (getHeight());
    const float y0 = H - 4.0f;
    const float y1 = 5.0f;
    const float yS = y0 + S * (y1 - y0);

    const float x0 = 0.0f;
    const float x1 = (A / total) * W;
    const float x2 = ((A + D) / total) * W;
    const float x3 = ((A + D + 0.4f) / total) * W;
    const float x4 = W;

    juce::Path env;
    env.startNewSubPath (x0, y0);
    env.lineTo (x1, y1);
    env.lineTo (x2, yS);
    env.lineTo (x3, yS);
    env.lineTo (x4, y0);

    juce::DropShadow ({ accent.withAlpha (0.35f), 6, { 0, 1 } }).drawForPath (g, env);

    juce::Path fill = env;
    fill.lineTo (x4, y0);
    fill.lineTo (x0, y0);
    fill.closeSubPath();
    g.setColour (accent.withAlpha (0.12f));
    g.fillPath (fill);

    g.setColour (accent);
    g.strokePath (env, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    g.setColour (SynthColors::border.withAlpha (0.4f));
    g.drawRoundedRectangle (getLocalBounds().toFloat(), 4.0f, 0.7f);
}

//==============================================================================
// WTVisualizer
//==============================================================================

WTVisualizer::WTVisualizer (juce::AudioProcessorValueTreeState& a) : apvts (a)
{
    startTimerHz (30);
}

void WTVisualizer::paint (juce::Graphics& g)
{
    using namespace SynthColors;
    const float pos    = apvts.getRawParameterValue ("wtPosition")->load();
    const int   tIdxA  = static_cast<int> (pos);
    const int   tIdxB  = juce::jmin (tIdxA + 1, WavetableOscillator::kNumWaveforms - 1);
    const float blend  = pos - static_cast<float> (tIdxA);

    const float* tA = WavetableOscillator::getTable (tIdxA);
    const float* tB = WavetableOscillator::getTable (tIdxB);
    const int    N  = WavetableOscillator::getTableSize();

    const float W = static_cast<float> (getWidth());
    const float H = static_cast<float> (getHeight());
    const float cy = H * 0.5f;
    const float amp = H * 0.42f;

    juce::Path wave;
    constexpr int kPts = 256;
    for (int i = 0; i <= kPts; ++i)
    {
        const float phase = static_cast<float>(i) / static_cast<float>(kPts);
        const float tPos  = phase * static_cast<float>(N);
        const int   idx   = static_cast<int> (tPos) % N;
        const float frac  = tPos - std::floor (tPos);
        const float sA    = tA[idx] + frac * (tA[idx + 1] - tA[idx]);
        const float sB    = tB[idx] + frac * (tB[idx + 1] - tB[idx]);
        const float s     = sA + blend * (sB - sA);

        const float px = phase * W;
        const float py = cy - s * amp;
        if (i == 0) wave.startNewSubPath (px, py);
        else        wave.lineTo (px, py);
    }

    juce::DropShadow ({ accent.withAlpha (0.3f), 5, { 0, 0 } }).drawForPath (g, wave);
    g.setColour (accent);
    g.strokePath (wave, juce::PathStrokeType (1.5f));
    g.setColour (border.withAlpha (0.4f));
    g.drawRoundedRectangle (getLocalBounds().toFloat(), 4.0f, 0.7f);
}

//==============================================================================
// LFOVisualizer
//==============================================================================

LFOVisualizer::LFOVisualizer (Synth1_0AudioProcessor& p) : processor (p)
{
    startTimerHz (30);
}

void LFOVisualizer::timerCallback()
{
    processor.lfoVisBuf.snapshot (snapData.data());
    repaint();
}

void LFOVisualizer::paint (juce::Graphics& g)
{
    using namespace SynthColors;
    const float W  = static_cast<float> (getWidth());
    const float H  = static_cast<float> (getHeight());
    const float cy = H * 0.5f;
    const float amp = H * 0.42f;

    juce::Path wave;
    const int kPts = LfoVisBuf::kSize;
    for (int i = 0; i < kPts; ++i)
    {
        const float px = static_cast<float>(i) / static_cast<float>(kPts - 1) * W;
        const float py = cy - snapData[i] * amp;
        if (i == 0) wave.startNewSubPath (px, py);
        else        wave.lineTo (px, py);
    }

    juce::DropShadow ({ accent.withAlpha (0.25f), 4, { 0, 0 } }).drawForPath (g, wave);
    g.setColour (accent.withAlpha (0.8f));
    g.strokePath (wave, juce::PathStrokeType (1.4f));
    g.setColour (border.withAlpha (0.4f));
    g.drawRoundedRectangle (getLocalBounds().toFloat(), 4.0f, 0.7f);
}

//==============================================================================
// MainTab
//==============================================================================

MainTab::MainTab (Synth1_0AudioProcessor& p)
    : proc (p),
      adsrVis (p.apvts),
      wtVis   (p.apvts),
      wtAttach        (p.apvts, "wtPosition",  wtSlider),
      attackAttach    (p.apvts, "attack",       attackSlider),
      decayAttach     (p.apvts, "decay",        decaySlider),
      sustainAttach   (p.apvts, "sustain",      sustainSlider),
      releaseAttach   (p.apvts, "release",      releaseSlider),
      cutoffAttach    (p.apvts, "cutoff",       cutoffSlider),
      resonanceAttach (p.apvts, "resonance",    resonanceSlider),
      driveAttach     (p.apvts, "drive",        driveSlider),
      gainAttach      (p.apvts, "gain",         gainSlider),
      warpAmountAttach(p.apvts, "warpAmount",   warpAmountSlider)
{
    // WT position slider
    wtSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    wtSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    addAndMakeVisible (wtSlider);
    wtLabel.setText ("WT POSITION", juce::dontSendNotification);
    wtLabel.setJustificationType (juce::Justification::centred);
    wtLabel.setFont (juce::FontOptions (9.0f));
    addAndMakeVisible (wtLabel);
    addAndMakeVisible (wtVis);

    setupKnob (attackSlider,    attackLabel,    "ATTACK",  this);
    setupKnob (decaySlider,     decayLabel,     "DECAY",   this);
    setupKnob (sustainSlider,   sustainLabel,   "SUSTAIN", this);
    setupKnob (releaseSlider,   releaseLabel,   "RELEASE", this);
    addAndMakeVisible (adsrVis);

    // Mark cutoff slider so the look-and-feel can draw the mod ring
    cutoffSlider.setComponentID ("cutoff");
    setupKnob (cutoffSlider,    cutoffLabel,    "CUTOFF",  this);
    setupKnob (resonanceSlider, resonanceLabel, "RESO",    this);
    setupKnob (driveSlider,     driveLabel,     "DRIVE",   this);
    setupKnob (gainSlider,      gainLabel,      "GAIN",    this);

    // Warp — items added BEFORE attachment so ComboBoxAttachment syncs correctly
    warpModeCombo.addItem ("None", 1);
    warpModeCombo.addItem ("Sync", 2);
    warpModeCombo.addItem ("Bend", 3);
    warpModeCombo.addItem ("PWM",  4);
    warpModeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        p.apvts, "warpMode", warpModeCombo);
    addAndMakeVisible (warpModeCombo);
    warpModeLabel.setText ("WARP", juce::dontSendNotification);
    warpModeLabel.setJustificationType (juce::Justification::centred);
    warpModeLabel.setFont (juce::FontOptions (9.0f));
    addAndMakeVisible (warpModeLabel);
    setupKnob (warpAmountSlider, warpAmountLabel, "AMOUNT", this);
}

void MainTab::paint (juce::Graphics& g)
{
    using namespace SynthColors;
    auto drawPanel = [&](juce::Rectangle<int> r, const juce::String& title)
    {
        g.setColour (surface);
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (border.withAlpha (0.5f));
        g.drawRoundedRectangle (r.toFloat(), 6.0f, 0.7f);
        g.setColour (subtext.withAlpha (0.7f));
        g.setFont (juce::FontOptions (7.5f));
        g.drawText (title, r.getX() + 7, r.getY() + 4, r.getWidth() - 14, 12,
                    juce::Justification::centredLeft);
        g.setColour (border.withAlpha (0.25f));
        g.fillRect (r.getX() + 5, r.getY() + 16, r.getWidth() - 10, 1);
    };
    drawPanel (oscR,    "OSC");
    drawPanel (adsrR,   "ADSR");
    drawPanel (filterR, "FILTER");
    drawPanel (outR,    "OUT");
}

void MainTab::resized()
{
    const int W      = getWidth();
    const int H      = getHeight();
    const int titleH = 18;

    constexpr int kOscW    = 180;   // widened to accommodate warp controls
    constexpr int kAdsrW   = kKnobSize * 4 + 20;
    constexpr int kFilterW = kKnobSize * 3 + 16;
    const int kOutW = W - kPad - kOscW - kGap - kAdsrW - kGap - kFilterW - kGap - kPad;

    const int secY  = kPad;
    const int secH  = H - kPad * 2;
    const int ctrlH = kKnobSize + 14 + kLabelH;
    const int ctrlY = secY + titleH + (secH - titleH - ctrlH) / 2;

    int x = kPad;

    // ── OSC ──────────────────────────────────────────────────────────────────
    oscR = { x, secY, kOscW, secH };
    {
        constexpr int warpComboH = 22;
        constexpr int warpKnobH  = kKnobSize + 14 + kLabelH;
        constexpr int wtSliderH  = 18;
        constexpr int wtLabelH   = kLabelH;
        constexpr int gap2       = 4;

        // Total fixed below vis: slider(18) + label(16) + gap(4) + comboLabel(16) + combo(22) + knob(88)
        const int fixedH = wtSliderH + wtLabelH + gap2 + kLabelH + warpComboH + gap2 + warpKnobH;
        const int visH   = secH - titleH - fixedH - 6;

        int oy = secY + titleH + 2;
        wtVis.setBounds    (x + 6, oy, kOscW - 12, visH);
        oy += visH + 4;
        wtSlider.setBounds (x + 6, oy, kOscW - 12, wtSliderH);
        oy += wtSliderH;
        wtLabel.setBounds  (x + 6, oy, kOscW - 12, wtLabelH);
        oy += wtLabelH + gap2;

        warpModeLabel.setBounds (x + 6, oy, kOscW - 12, kLabelH);
        oy += kLabelH;
        warpModeCombo.setBounds (x + 6, oy, kOscW - 12, warpComboH);
        oy += warpComboH + gap2;

        const int kwx = x + (kOscW - kKnobSize) / 2;
        warpAmountSlider.setBounds (kwx, oy, kKnobSize, kKnobSize + 14);
        warpAmountLabel.setBounds  (kwx, oy + kKnobSize + 14, kKnobSize, kLabelH);
    }
    x += kOscW + kGap;

    // ── ADSR ─────────────────────────────────────────────────────────────────
    adsrR = { x, secY, kAdsrW, secH };
    {
        const int visH = secH - titleH - ctrlH - 10;
        adsrVis.setBounds (x + 6, secY + titleH + 2, kAdsrW - 12, visH);
        juce::Slider* sliders[] = { &attackSlider, &decaySlider, &sustainSlider, &releaseSlider };
        juce::Label*  labels[]  = { &attackLabel,  &decayLabel,  &sustainLabel,  &releaseLabel  };
        const int ky = secY + titleH + visH + 8;
        int kx = x + 10;
        for (int i = 0; i < 4; ++i)
        {
            sliders[i]->setBounds (kx, ky, kKnobSize, kKnobSize + 14);
            labels[i]->setBounds  (kx, ky + kKnobSize + 14, kKnobSize, kLabelH);
            kx += kKnobSize;
        }
    }
    x += kAdsrW + kGap;

    // ── Filter ───────────────────────────────────────────────────────────────
    filterR = { x, secY, kFilterW, secH };
    {
        juce::Slider* sliders[] = { &cutoffSlider, &resonanceSlider, &driveSlider };
        juce::Label*  labels[]  = { &cutoffLabel,  &resonanceLabel,  &driveLabel  };
        int kx = x + 8;
        for (int i = 0; i < 3; ++i)
        {
            sliders[i]->setBounds (kx, ctrlY, kKnobSize, kKnobSize + 14);
            labels[i]->setBounds  (kx, ctrlY + kKnobSize + 14, kKnobSize, kLabelH);
            kx += kKnobSize;
        }
    }
    x += kFilterW + kGap;

    // ── Out ──────────────────────────────────────────────────────────────────
    outR = { x, secY, kOutW, secH };
    {
        const int kx = x + (kOutW - kKnobSize) / 2;
        gainSlider.setBounds (kx, ctrlY, kKnobSize, kKnobSize + 14);
        gainLabel.setBounds  (kx, ctrlY + kKnobSize + 14, kKnobSize, kLabelH);
    }
}

//==============================================================================
// ModTab
//==============================================================================

ModTab::ModTab (Synth1_0AudioProcessor& p)
    : proc (p),
      lfoVis (p),
      lfoRateAttach        (p.apvts, "lfoRate",        lfoRateSlider),
      lfoCutoffDepthAttach (p.apvts, "lfoCutoffDepth", lfoCutoffDepthSlider),
      lfoPitchDepthAttach  (p.apvts, "lfoPitchDepth",  lfoPitchDepthSlider),
      unisonVoicesAttach   (p.apvts, "unisonVoices",   unisonVoicesSlider),
      unisonDetuneAttach   (p.apvts, "unisonDetune",   unisonDetuneSlider),
      unisonSpreadAttach   (p.apvts, "unisonSpread",   unisonSpreadSlider),
      glideTimeAttach      (p.apvts, "glideTime",      glideTimeSlider)
{
    setupKnob (lfoRateSlider,        lfoRateLabel,        "RATE",     this);
    setupKnob (lfoCutoffDepthSlider, lfoCutoffDepthLabel, "→ CUTOFF", this);
    setupKnob (lfoPitchDepthSlider,  lfoPitchDepthLabel,  "→ PITCH",  this);
    addAndMakeVisible (lfoVis);

    setupKnob (unisonVoicesSlider, unisonVoicesLabel, "VOICES", this);
    setupKnob (unisonDetuneSlider, unisonDetuneLabel, "DETUNE", this);
    setupKnob (unisonSpreadSlider, unisonSpreadLabel, "SPREAD", this);

    voiceModeCombo.addItem ("Poly",   1);
    voiceModeCombo.addItem ("Mono",   2);
    voiceModeCombo.addItem ("Legato", 3);
    voiceModeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        p.apvts, "voiceMode", voiceModeCombo);
    addAndMakeVisible (voiceModeCombo);
    voiceModeLabel.setText ("MODE", juce::dontSendNotification);
    voiceModeLabel.setJustificationType (juce::Justification::centred);
    voiceModeLabel.setFont (juce::FontOptions (9.0f));
    addAndMakeVisible (voiceModeLabel);
    setupKnob (glideTimeSlider, glideTimeLabel, "GLIDE", this);
}

void ModTab::paint (juce::Graphics& g)
{
    using namespace SynthColors;
    auto drawPanel = [&](juce::Rectangle<int> r, const juce::String& title)
    {
        g.setColour (surface);
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (border.withAlpha (0.5f));
        g.drawRoundedRectangle (r.toFloat(), 6.0f, 0.7f);
        g.setColour (subtext.withAlpha (0.7f));
        g.setFont (juce::FontOptions (7.5f));
        g.drawText (title, r.getX() + 7, r.getY() + 4, r.getWidth() - 14, 12,
                    juce::Justification::centredLeft);
    };
    drawPanel (visR,    "LFO SCOPE");
    drawPanel (lfoR,    "LFO");
    drawPanel (unisonR, "UNISON");
    drawPanel (voiceR,  "VOICE");
}

void ModTab::resized()
{
    const int W    = getWidth();
    const int H    = getHeight();
    const int secY = kPad;
    const int secH = H - kPad * 2;

    constexpr int kVisW = 220;
    const int remaining = W - kPad * 2 - kGap * 3 - kVisW;
    const int kCtrlW    = remaining / 3;

    visR    = { kPad,                                          secY, kVisW,  secH };
    lfoR    = { kPad + kVisW + kGap,                          secY, kCtrlW, secH };
    unisonR = { kPad + kVisW + kGap + kCtrlW + kGap,          secY, kCtrlW, secH };
    voiceR  = { kPad + kVisW + kGap + (kCtrlW + kGap) * 2,    secY, kCtrlW, secH };

    lfoVis.setBounds (kPad + 6, secY + 20, kVisW - 12, secH - 28);

    const int knobRowH  = kKnobSize + 14 + kLabelH;
    const int ctrlBaseY = secY + 20 + (secH - 20 - knobRowH) / 2;

    // LFO knobs
    {
        juce::Slider* sl[] = { &lfoRateSlider, &lfoCutoffDepthSlider, &lfoPitchDepthSlider };
        juce::Label*  la[] = { &lfoRateLabel,  &lfoCutoffDepthLabel,  &lfoPitchDepthLabel  };
        int kx = lfoR.getX() + (lfoR.getWidth() - kKnobSize * 3) / 2;
        for (int i = 0; i < 3; ++i)
        {
            sl[i]->setBounds (kx, ctrlBaseY, kKnobSize, kKnobSize + 14);
            la[i]->setBounds (kx, ctrlBaseY + kKnobSize + 14, kKnobSize, kLabelH);
            kx += kKnobSize;
        }
    }

    // Unison knobs
    {
        juce::Slider* sl[] = { &unisonVoicesSlider, &unisonDetuneSlider, &unisonSpreadSlider };
        juce::Label*  la[] = { &unisonVoicesLabel,  &unisonDetuneLabel,  &unisonSpreadLabel  };
        int kx = unisonR.getX() + (unisonR.getWidth() - kKnobSize * 3) / 2;
        for (int i = 0; i < 3; ++i)
        {
            sl[i]->setBounds (kx, ctrlBaseY, kKnobSize, kKnobSize + 14);
            la[i]->setBounds (kx, ctrlBaseY + kKnobSize + 14, kKnobSize, kLabelH);
            kx += kKnobSize;
        }
    }

    // Voice mode + glide
    {
        const int vx  = voiceR.getX() + kGap;
        const int vw  = voiceR.getWidth() - kGap * 2;
        const int midY = secY + secH / 2;
        voiceModeLabel.setBounds (vx, midY - 46, vw, kLabelH);
        voiceModeCombo.setBounds (vx, midY - 30, vw, 22);
        const int kx = voiceR.getX() + (voiceR.getWidth() - kKnobSize) / 2;
        glideTimeSlider.setBounds (kx, midY + 2,  kKnobSize, kKnobSize + 14);
        glideTimeLabel.setBounds  (kx, midY + 2 + kKnobSize + 14, kKnobSize, kLabelH);
    }
}

//==============================================================================
// FXTab
//==============================================================================

FXTab::FXTab (Synth1_0AudioProcessor& p)
    : proc (p),
      chorusMixAttach    (p.apvts, "chorusMix",      chorusMixSlider),
      chorusRateAttach   (p.apvts, "chorusRate",     chorusRateSlider),
      chorusDepthAttach  (p.apvts, "chorusDepth",    chorusDepthSlider),
      delayTimeAttach    (p.apvts, "delayTime",      delayTimeSlider),
      delayFeedbackAttach(p.apvts, "delayFeedback",  delayFeedbackSlider),
      delayMixAttach     (p.apvts, "delayMix",       delayMixSlider),
      satDriveAttach     (p.apvts, "satDrive",       satDriveSlider),
      satMixAttach       (p.apvts, "satMix",         satMixSlider),
      reverbSizeAttach   (p.apvts, "reverbSize",     reverbSizeSlider),
      reverbDampingAttach(p.apvts, "reverbDamping",  reverbDampingSlider),
      reverbMixAttach    (p.apvts, "reverbMix",      reverbMixSlider)
{
    setupKnob (chorusMixSlider,     chorusMixLabel,    "MIX",      this);
    setupKnob (chorusRateSlider,    chorusRateLabel,   "RATE",     this);
    setupKnob (chorusDepthSlider,   chorusDepthLabel,  "DEPTH",    this);
    setupKnob (delayTimeSlider,     delayTimeLabel,    "TIME",     this);
    setupKnob (delayFeedbackSlider, delayFeedbackLabel,"FEEDBACK", this);
    setupKnob (delayMixSlider,      delayMixLabel,     "MIX",      this);
    setupKnob (satDriveSlider,      satDriveLabel,     "DRIVE",    this);
    setupKnob (satMixSlider,        satMixLabel,       "MIX",      this);
    setupKnob (reverbSizeSlider,    reverbSizeLabel,   "SIZE",     this);
    setupKnob (reverbDampingSlider, reverbDampingLabel,"DAMPING",  this);
    setupKnob (reverbMixSlider,     reverbMixLabel,    "MIX",      this);
}

void FXTab::paint (juce::Graphics& g)
{
    using namespace SynthColors;
    auto drawPanel = [&](juce::Rectangle<int> r, const juce::String& title)
    {
        g.setColour (surface);
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (border.withAlpha (0.5f));
        g.drawRoundedRectangle (r.toFloat(), 6.0f, 0.7f);
        g.setColour (subtext.withAlpha (0.7f));
        g.setFont (juce::FontOptions (7.5f));
        g.drawText (title, r.getX() + 7, r.getY() + 4, r.getWidth() - 14, 12,
                    juce::Justification::centredLeft);
        g.setColour (border.withAlpha (0.25f));
        g.fillRect (r.getX() + 5, r.getY() + 16, r.getWidth() - 10, 1);
    };
    drawPanel (chorusR, "CHORUS");
    drawPanel (delayR,  "DELAY");
    drawPanel (satR,    "SATURATION  (2× OS)");
    drawPanel (reverbR, "REVERB");
}

void FXTab::resized()
{
    const int W    = getWidth();
    const int H    = getHeight();
    const int secY = kPad;
    const int secH = H - kPad * 2;

    const int panelW = (W - kPad * 2 - kGap * 3) / 4;
    const int ctrlH  = kKnobSize + 14 + kLabelH;
    const int ctrlY  = secY + 20 + (secH - 20 - ctrlH) / 2;

    int px = kPad;

    auto layoutKnobs = [&](juce::Slider** sl, juce::Label** la, int n)
    {
        int kx = px + (panelW - kKnobSize * n) / 2;
        for (int i = 0; i < n; ++i)
        {
            sl[i]->setBounds (kx, ctrlY, kKnobSize, kKnobSize + 14);
            la[i]->setBounds (kx, ctrlY + kKnobSize + 14, kKnobSize, kLabelH);
            kx += kKnobSize;
        }
    };

    chorusR = { px, secY, panelW, secH };
    { juce::Slider* sl[] = { &chorusMixSlider, &chorusRateSlider, &chorusDepthSlider };
      juce::Label*  la[] = { &chorusMixLabel,  &chorusRateLabel,  &chorusDepthLabel  };
      layoutKnobs (sl, la, 3); }
    px += panelW + kGap;

    delayR = { px, secY, panelW, secH };
    { juce::Slider* sl[] = { &delayTimeSlider, &delayFeedbackSlider, &delayMixSlider };
      juce::Label*  la[] = { &delayTimeLabel,  &delayFeedbackLabel,  &delayMixLabel  };
      layoutKnobs (sl, la, 3); }
    px += panelW + kGap;

    satR = { px, secY, panelW, secH };
    { juce::Slider* sl[] = { &satDriveSlider, &satMixSlider };
      juce::Label*  la[] = { &satDriveLabel,  &satMixLabel  };
      layoutKnobs (sl, la, 2); }
    px += panelW + kGap;

    reverbR = { px, secY, panelW, secH };
    { juce::Slider* sl[] = { &reverbSizeSlider, &reverbDampingSlider, &reverbMixSlider };
      juce::Label*  la[] = { &reverbSizeLabel,  &reverbDampingLabel,  &reverbMixLabel  };
      layoutKnobs (sl, la, 3); }
}

//==============================================================================
// EQVisualizer — helpers
//==============================================================================

static constexpr float kEqFMin = 20.0f,  kEqFMax  = 20000.0f;
static constexpr float kEqDMin = -24.0f, kEqDMax  = 24.0f;
static constexpr float kEqGMin = -18.0f, kEqGMax  = 18.0f;

float EQVisualizer::freqToX (float freq) const
{
    return std::log (freq / kEqFMin) / std::log (kEqFMax / kEqFMin) * getWidth();
}

float EQVisualizer::gainToY (float gainDb) const
{
    return juce::jmap (gainDb, kEqDMin, kEqDMax, (float) getHeight() - 4.0f, 4.0f);
}

float EQVisualizer::xToFreq (float xPx) const
{
    return kEqFMin * std::pow (kEqFMax / kEqFMin, xPx / (float) getWidth());
}

float EQVisualizer::yToGain (float yPx) const
{
    return juce::jmap (yPx, (float) getHeight() - 4.0f, 4.0f, kEqDMin, kEqDMax);
}

int EQVisualizer::findNearestBand (float xPx, float yPx) const
{
    const char* freqIds[] = { "eqLowFreq", "eqMidFreq", "eqHighFreq" };
    const char* gainIds[] = { "eqLowGain", "eqMidGain", "eqHighGain" };
    int   nearest = -1;
    float minDist = 20.0f;  // pixel hit radius
    for (int b = 0; b < 3; ++b)
    {
        const float hx = freqToX (proc.apvts.getRawParameterValue (freqIds[b])->load());
        const float hy = gainToY (proc.apvts.getRawParameterValue (gainIds[b])->load());
        const float d  = std::hypot (xPx - hx, yPx - hy);
        if (d < minDist) { minDist = d; nearest = b; }
    }
    return nearest;
}

//==============================================================================
// EQVisualizer — mouse
//==============================================================================

EQVisualizer::EQVisualizer (Synth1_0AudioProcessor& p) : proc (p)
{
    startTimerHz (30);
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

void EQVisualizer::mouseDown (const juce::MouseEvent& e)
{
    draggedBand = findNearestBand ((float) e.x, (float) e.y);
}

void EQVisualizer::mouseUp (const juce::MouseEvent&)
{
    draggedBand = -1;
}

void EQVisualizer::mouseDrag (const juce::MouseEvent& e)
{
    if (draggedBand < 0) return;

    const char* freqIds[] = { "eqLowFreq", "eqMidFreq", "eqHighFreq" };
    const char* gainIds[] = { "eqLowGain", "eqMidGain", "eqHighGain" };

    const float newFreq = juce::jlimit (kEqFMin, kEqFMax, xToFreq ((float) e.x));
    const float newGain = juce::jlimit (kEqGMin, kEqGMax, yToGain ((float) e.y));

    if (auto* p = proc.apvts.getParameter (freqIds[draggedBand]))
        p->setValueNotifyingHost (
            proc.apvts.getParameterRange (freqIds[draggedBand]).convertTo0to1 (newFreq));

    if (auto* p = proc.apvts.getParameter (gainIds[draggedBand]))
        p->setValueNotifyingHost (
            proc.apvts.getParameterRange (gainIds[draggedBand]).convertTo0to1 (newGain));
}

void EQVisualizer::mouseWheelMove (const juce::MouseEvent& e,
                                    const juce::MouseWheelDetails& w)
{
    // Q only applies to the mid-bell band
    if (findNearestBand ((float) e.x, (float) e.y) != 1) return;
    if (auto* p = proc.apvts.getParameter ("eqMidQ"))
        p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, p->getValue() + w.deltaY * 0.05f));
}

void EQVisualizer::mouseMove (const juce::MouseEvent& e)
{
    const int prev = hoveredBand;
    hoveredBand = findNearestBand ((float) e.x, (float) e.y);
    if (hoveredBand != prev) repaint();
}

void EQVisualizer::mouseExit (const juce::MouseEvent&)
{
    if (hoveredBand >= 0) { hoveredBand = -1; repaint(); }
}

//==============================================================================
// EQVisualizer — timer + paint
//==============================================================================

void EQVisualizer::timerCallback()
{
    const int kN      = kFFTSize;
    const int kBufSz  = SpecVisBuf::kSize;
    std::fill (fftBuf.begin(), fftBuf.end(), 0.0f);

    const int wp = proc.specVisBuf.writePos.load (std::memory_order_acquire) % kBufSz;
    for (int i = 0; i < kN; ++i)
        fftBuf[i] = proc.specVisBuf.data[((wp - kN + i) + kBufSz * 2) % kBufSz];

    win.multiplyWithWindowingTable (fftBuf.data(), (size_t) kN);
    fft.performFrequencyOnlyForwardTransform (fftBuf.data());

    const float norm = 1.0f / (kN * 0.5f);
    for (int i = 0; i <= kN / 2; ++i)
    {
        const float db = juce::Decibels::gainToDecibels (fftBuf[i] * norm, -80.0f);
        spectrum[i]    = spectrum[i] * 0.75f + db * 0.25f;
    }
    repaint();
}

void EQVisualizer::paint (juce::Graphics& g)
{
    using namespace SynthColors;
    const float W  = static_cast<float> (getWidth());
    const float H  = static_cast<float> (getHeight());
    const double sr = std::max (44100.0, proc.getSampleRate());

    g.setColour (surface);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);

    // Frequency grid
    {
        const float gridFreqs[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
        g.setColour (border.withAlpha (0.25f));
        for (float f : gridFreqs)
            g.drawVerticalLine ((int) freqToX (f), 2.0f, H - 2.0f);

        const float zeroY = gainToY (0.0f);
        g.setColour (border.withAlpha (0.4f));
        g.drawHorizontalLine ((int) zeroY, 2.0f, W - 2.0f);
    }

    // Live FFT spectrum
    {
        const int kBins = kFFTSize / 2;
        juce::Path specPath;
        bool started = false;
        for (int i = 1; i < kBins; ++i)
        {
            const float binHz = static_cast<float> (i) * static_cast<float> (sr) / kFFTSize;
            if (binHz < kEqFMin || binHz > kEqFMax) continue;
            const float px = freqToX (binHz);
            const float db = juce::jlimit (-60.0f, 6.0f, spectrum[i]);
            const float py = juce::jmap (db, -60.0f, 6.0f, H - 4.0f, 4.0f);
            if (!started) { specPath.startNewSubPath (px, py); started = true; }
            else          specPath.lineTo (px, py);
        }
        if (started)
        {
            juce::Path fill = specPath;
            fill.lineTo (W, H); fill.lineTo (0, H); fill.closeSubPath();
            g.setColour (accent.withAlpha (0.07f));
            g.fillPath (fill);
            g.setColour (subtext.withAlpha (0.4f));
            g.strokePath (specPath, juce::PathStrokeType (1.0f));
        }
    }

    // EQ frequency response curve
    {
        using Coeffs = juce::dsp::IIR::Coefficients<float>;
        auto& apvts = proc.apvts;
        auto lo  = Coeffs::makeLowShelf  (sr, apvts.getRawParameterValue ("eqLowFreq")->load(), 0.707f,
                       juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("eqLowGain")->load()));
        auto mid = Coeffs::makePeakFilter (sr, apvts.getRawParameterValue ("eqMidFreq")->load(),
                       juce::jlimit (0.1f, 10.0f, apvts.getRawParameterValue ("eqMidQ")->load()),
                       juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("eqMidGain")->load()));
        auto hi  = Coeffs::makeHighShelf  (sr, apvts.getRawParameterValue ("eqHighFreq")->load(), 0.707f,
                       juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("eqHighGain")->load()));

        constexpr int kPts = 400;
        juce::Path eq;
        for (int i = 0; i <= kPts; ++i)
        {
            const float t  = static_cast<float> (i) / kPts;
            const double f = kEqFMin * std::pow ((double) kEqFMax / kEqFMin, (double) t);
            const double mag = lo->getMagnitudeForFrequency (f, sr)
                             * mid->getMagnitudeForFrequency (f, sr)
                             * hi->getMagnitudeForFrequency (f, sr);
            const float db  = juce::jlimit (kEqDMin, kEqDMax,
                                            juce::Decibels::gainToDecibels ((float) mag));
            const float px  = t * W;
            const float py  = gainToY (db);
            if (i == 0) eq.startNewSubPath (px, py);
            else        eq.lineTo (px, py);
        }

        const float zeroY = gainToY (0.0f);
        juce::Path fill = eq;
        fill.lineTo (W, zeroY); fill.lineTo (0.0f, zeroY); fill.closeSubPath();
        g.setColour (accent.withAlpha (0.10f));
        g.fillPath (fill);

        juce::DropShadow ({ accent.withAlpha (0.3f), 5, { 0, 0 } }).drawForPath (g, eq);
        g.setColour (accent);
        g.strokePath (eq, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        // Band handles
        const juce::Colour bandCols[] = {
            juce::Colour (0xffcba6f7),  // low  — lavender
            juce::Colour (0xffa6e3a1),  // mid  — green
            juce::Colour (0xfffab387),  // high — orange
        };
        const char* fIds[] = { "eqLowFreq",  "eqMidFreq",  "eqHighFreq"  };
        const char* gIds[] = { "eqLowGain",  "eqMidGain",  "eqHighGain"  };
        for (int b = 0; b < 3; ++b)
        {
            const float hx  = freqToX (apvts.getRawParameterValue (fIds[b])->load());
            const float hy  = gainToY (apvts.getRawParameterValue (gIds[b])->load());
            const bool  hot = (b == hoveredBand || b == draggedBand);
            g.setColour (bandCols[b].withAlpha (hot ? 0.9f : 0.55f));
            g.fillEllipse (hx - 6.0f, hy - 6.0f, 12.0f, 12.0f);
            g.setColour (juce::Colours::white.withAlpha (hot ? 0.9f : 0.35f));
            g.drawEllipse (hx - 6.0f, hy - 6.0f, 12.0f, 12.0f, 1.2f);
        }
    }

    g.setColour (border.withAlpha (0.5f));
    g.drawRoundedRectangle (getLocalBounds().toFloat(), 4.0f, 0.7f);
}

//==============================================================================
// EQTab
//==============================================================================

EQTab::EQTab (Synth1_0AudioProcessor& p)
    : proc (p),
      eqVis (p),
      eqLowFreqAttach  (p.apvts, "eqLowFreq",  eqLowFreqSlider),
      eqLowGainAttach  (p.apvts, "eqLowGain",  eqLowGainSlider),
      eqMidFreqAttach  (p.apvts, "eqMidFreq",  eqMidFreqSlider),
      eqMidGainAttach  (p.apvts, "eqMidGain",  eqMidGainSlider),
      eqMidQAttach     (p.apvts, "eqMidQ",     eqMidQSlider),
      eqHighFreqAttach (p.apvts, "eqHighFreq", eqHighFreqSlider),
      eqHighGainAttach (p.apvts, "eqHighGain", eqHighGainSlider)
{
    setupKnob (eqLowFreqSlider,  eqLowFreqLabel,  "FREQ", this);
    setupKnob (eqLowGainSlider,  eqLowGainLabel,  "GAIN", this);
    setupKnob (eqMidFreqSlider,  eqMidFreqLabel,  "FREQ", this);
    setupKnob (eqMidGainSlider,  eqMidGainLabel,  "GAIN", this);
    setupKnob (eqMidQSlider,     eqMidQLabel,     "Q",    this);
    setupKnob (eqHighFreqSlider, eqHighFreqLabel, "FREQ", this);
    setupKnob (eqHighGainSlider, eqHighGainLabel, "GAIN", this);
    addAndMakeVisible (eqVis);
}

void EQTab::paint (juce::Graphics& g)
{
    using namespace SynthColors;
    auto drawPanel = [&](juce::Rectangle<int> r, const juce::String& title)
    {
        g.setColour (surface);
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (border.withAlpha (0.5f));
        g.drawRoundedRectangle (r.toFloat(), 6.0f, 0.7f);
        g.setColour (subtext.withAlpha (0.7f));
        g.setFont (juce::FontOptions (7.5f));
        g.drawText (title, r.getX() + 7, r.getY() + 4, r.getWidth() - 14, 12,
                    juce::Justification::centredLeft);
        g.setColour (border.withAlpha (0.25f));
        g.fillRect (r.getX() + 5, r.getY() + 16, r.getWidth() - 10, 1);
    };
    drawPanel (lowR,  "LOW SHELF");
    drawPanel (midR,  "MID BELL");
    drawPanel (highR, "HIGH SHELF");
    g.setColour (surface);
    g.fillRoundedRectangle (visR.toFloat(), 6.0f);
}

void EQTab::resized()
{
    const int W    = getWidth();
    const int H    = getHeight();
    const int secY = kPad;
    const int secH = H - kPad * 2;

    constexpr int kVisH   = 140;
    constexpr int kPanelH = 140;
    const int panelsY = secY + kVisH + kGap;

    visR = { kPad, secY, W - kPad * 2, kVisH };
    eqVis.setBounds (visR.expanded (-1));

    const int panelW = (W - kPad * 2 - kGap * 2) / 3;
    const int ctrlH  = kKnobSize + 14 + kLabelH;
    const int ctrlY  = panelsY + 20 + (kPanelH - 20 - ctrlH) / 2;
    int px = kPad;

    auto layoutKnobs = [&](juce::Slider** sl, juce::Label** la, int n)
    {
        int kx = px + (panelW - kKnobSize * n) / 2;
        for (int i = 0; i < n; ++i)
        {
            sl[i]->setBounds (kx, ctrlY, kKnobSize, kKnobSize + 14);
            la[i]->setBounds (kx, ctrlY + kKnobSize + 14, kKnobSize, kLabelH);
            kx += kKnobSize;
        }
    };

    lowR = { px, panelsY, panelW, kPanelH };
    { juce::Slider* sl[] = { &eqLowFreqSlider,  &eqLowGainSlider  };
      juce::Label*  la[] = { &eqLowFreqLabel,   &eqLowGainLabel   };
      layoutKnobs (sl, la, 2); }
    px += panelW + kGap;

    midR = { px, panelsY, panelW, kPanelH };
    { juce::Slider* sl[] = { &eqMidFreqSlider, &eqMidGainSlider, &eqMidQSlider };
      juce::Label*  la[] = { &eqMidFreqLabel,  &eqMidGainLabel,  &eqMidQLabel  };
      layoutKnobs (sl, la, 3); }
    px += panelW + kGap;

    highR = { px, panelsY, panelW, kPanelH };
    { juce::Slider* sl[] = { &eqHighFreqSlider, &eqHighGainSlider };
      juce::Label*  la[] = { &eqHighFreqLabel,  &eqHighGainLabel  };
      layoutKnobs (sl, la, 2); }
}

//==============================================================================
// Synth1_0AudioProcessorEditor
//==============================================================================

juce::File Synth1_0AudioProcessorEditor::getPresetsDir() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Synth1_0/Presets");
}

void Synth1_0AudioProcessorEditor::populatePresets()
{
    presetBox.clear (juce::dontSendNotification);
    presetBox.addItem ("-- Preset --", 1);

    auto dir   = getPresetsDir();
    auto files = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
    files.sort();
    for (int i = 0; i < files.size(); ++i)
        presetBox.addItem (files[i].getFileNameWithoutExtension(), i + 2);

    presetBox.setSelectedId (1, juce::dontSendNotification);
}

void Synth1_0AudioProcessorEditor::savePreset()
{
    auto dir = getPresetsDir();
    dir.createDirectory();

    fileChooser = std::make_unique<juce::FileChooser> ("Save Preset", dir, "*.xml");
    fileChooser->launchAsync (
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File{}) return;
            file = file.withFileExtension ("xml");
            if (auto xml = audioProcessor.apvts.copyState().createXml())
                xml->writeTo (file);
            populatePresets();
        });
}

void Synth1_0AudioProcessorEditor::loadPreset (const juce::File& f)
{
    if (auto xml = juce::parseXML (f))
        if (xml->hasTagName (audioProcessor.apvts.state.getType()))
            audioProcessor.apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

Synth1_0AudioProcessorEditor::Synth1_0AudioProcessorEditor (Synth1_0AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&laf);

    // Wire up LFO depth pointer for mod-ring rendering on the cutoff knob
    laf.lfoCutoffDepth = p.apvts.getRawParameterValue ("lfoCutoffDepth");

    tabs.addTab ("MAIN", SynthColors::bg, new MainTab (p), true);
    tabs.addTab ("MOD",  SynthColors::bg, new ModTab  (p), true);
    tabs.addTab ("FX",   SynthColors::bg, new FXTab   (p), true);
    tabs.addTab ("EQ",   SynthColors::bg, new EQTab   (p), true);
    addAndMakeVisible (tabs);

    // Preset bar
    presetBox.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (presetBox);
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id < 2) return;
        auto dir   = getPresetsDir();
        auto files = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
        files.sort();
        if (id - 2 < files.size())
            loadPreset (files[id - 2]);
    };

    saveBtn.onClick = [this] { savePreset(); };
    addAndMakeVisible (saveBtn);

    populatePresets();

    setSize (800, kHeaderH + kTabH);
}

Synth1_0AudioProcessorEditor::~Synth1_0AudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void Synth1_0AudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace SynthColors;
    g.fillAll (bg);

    g.setColour (surface);
    g.fillRect (0, 0, getWidth(), kHeaderH);
    g.setColour (accent.withAlpha (0.2f));
    g.fillRect (0, kHeaderH - 1, getWidth(), 1);

    g.setColour (accent);
    g.setFont (juce::FontOptions (13.5f));
    g.drawText ("SYNTH  1.0", kPad, 0, 180, kHeaderH, juce::Justification::centredLeft);

    g.setColour (subtext);
    g.setFont (juce::FontOptions (9.0f));
    g.drawText ("WAVETABLE SYNTHESIZER", 180, 0, 200, kHeaderH, juce::Justification::centredLeft);

    // Preset area background — ensures controls are visible on the dark header
    constexpr int kBtnW = 48, kBoxW = 130;
    const int areaX = getWidth() - kPad - kBtnW - 6 - kBoxW - 6;
    const int areaW = kBtnW + 6 + kBoxW + kPad + 6;
    g.setColour (border.withAlpha (0.35f));
    g.fillRoundedRectangle ((float) areaX, 4.0f, (float) areaW, (float) (kHeaderH - 8), 4.0f);
    g.setColour (accent.withAlpha (0.3f));
    g.drawRoundedRectangle ((float) areaX, 4.0f, (float) areaW, (float) (kHeaderH - 8), 4.0f, 0.7f);
}

void Synth1_0AudioProcessorEditor::resized()
{
    tabs.setBounds (0, kHeaderH, getWidth(), kTabH);

    constexpr int kBtnW = 48;
    constexpr int kBoxW = 130;
    const int barH = kHeaderH - 10;
    const int barY = 5;
    saveBtn.setBounds   (getWidth() - kPad - kBtnW, barY, kBtnW, barH);
    presetBox.setBounds (getWidth() - kPad - kBtnW - 6 - kBoxW, barY, kBoxW, barH);
}
