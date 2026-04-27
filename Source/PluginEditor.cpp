#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr int kKnobSize  = 58;
    constexpr int kLabelH    = 16;
    constexpr int kHeaderH   = 38;
    constexpr int kPad       = 14;
    constexpr int kGap       = 8;
    constexpr int kTabH      = 320;   // height given to TabbedComponent
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
}

void SynthLookAndFeel::drawRotarySlider (juce::Graphics& g,
    int x, int y, int w, int h,
    float sliderPos, float startAngle, float endAngle, juce::Slider&)
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

    // Drop shadow
    juce::DropShadow ({ accent.withAlpha (0.35f), 6, { 0, 1 } }).drawForPath (g, env);

    // Fill
    juce::Path fill = env;
    fill.lineTo (x4, y0);
    fill.lineTo (x0, y0);
    fill.closeSubPath();
    g.setColour (accent.withAlpha (0.12f));
    g.fillPath (fill);

    // Stroke
    g.setColour (accent);
    g.strokePath (env, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    // Background border
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

    // Sample the morphed waveform at display resolution
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

    // Glow
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
      wtAttach      (p.apvts, "wtPosition", wtSlider),
      attackAttach  (p.apvts, "attack",     attackSlider),
      decayAttach   (p.apvts, "decay",      decaySlider),
      sustainAttach (p.apvts, "sustain",    sustainSlider),
      releaseAttach (p.apvts, "release",    releaseSlider),
      cutoffAttach  (p.apvts, "cutoff",     cutoffSlider),
      resonanceAttach(p.apvts,"resonance",  resonanceSlider),
      driveAttach   (p.apvts, "drive",      driveSlider),
      gainAttach    (p.apvts, "gain",       gainSlider)
{
    // WT position — horizontal slider
    wtSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    wtSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    addAndMakeVisible (wtSlider);
    wtLabel.setText ("WT POSITION", juce::dontSendNotification);
    wtLabel.setJustificationType (juce::Justification::centred);
    wtLabel.setFont (juce::FontOptions (9.0f));
    addAndMakeVisible (wtLabel);
    addAndMakeVisible (wtVis);

    setupKnob (attackSlider,    attackLabel,    "ATTACK",    this);
    setupKnob (decaySlider,     decayLabel,     "DECAY",     this);
    setupKnob (sustainSlider,   sustainLabel,   "SUSTAIN",   this);
    setupKnob (releaseSlider,   releaseLabel,   "RELEASE",   this);

    addAndMakeVisible (adsrVis);

    setupKnob (cutoffSlider,    cutoffLabel,    "CUTOFF",    this);
    setupKnob (resonanceSlider, resonanceLabel, "RESO",      this);
    setupKnob (driveSlider,     driveLabel,     "DRIVE",     this);
    setupKnob (gainSlider,      gainLabel,      "GAIN",      this);
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
    const int W   = getWidth();
    const int H   = getHeight();
    const int titleH = 18;

    // ── Section widths ────────────────────────────────────────────────────────
    constexpr int kOscW    = 152;
    constexpr int kAdsrW   = kKnobSize * 4 + 20;    // 252
    constexpr int kFilterW = kKnobSize * 3 + 16;    // 190
    // kOutW fills the rest
    const int kOutW = W - kPad - kOscW - kGap - kAdsrW - kGap - kFilterW - kGap - kPad;

    const int secY = kPad;
    const int secH = H - kPad * 2;
    const int ctrlH = kKnobSize + 14 + kLabelH;     // knob+textbox+label
    const int ctrlY = secY + titleH + (secH - titleH - ctrlH) / 2;

    int x = kPad;

    // ── OSC ──────────────────────────────────────────────────────────────────
    oscR = { x, secY, kOscW, secH };
    {
        const int visH = secH - titleH - 38;    // remaining after WT slider
        wtVis.setBounds (x + 6, secY + titleH + 2, kOscW - 12, visH);
        wtSlider.setBounds (x + 6, secY + titleH + visH + 6, kOscW - 12, 18);
        wtLabel.setBounds  (x + 6, secY + titleH + visH + 26, kOscW - 12, kLabelH);
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
      lfoPitchDepthAttach  (p.apvts, "lfoPitchDepth",  lfoPitchDepthSlider)
{
    setupKnob (lfoRateSlider,        lfoRateLabel,        "RATE",      this);
    setupKnob (lfoCutoffDepthSlider, lfoCutoffDepthLabel, "→ CUTOFF",  this);
    setupKnob (lfoPitchDepthSlider,  lfoPitchDepthLabel,  "→ PITCH",   this);
    addAndMakeVisible (lfoVis);
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
    drawPanel (visR,  "LFO SCOPE");
    drawPanel (ctrlR, "LFO");
}

void ModTab::resized()
{
    const int H = getHeight();
    const int secY = kPad;
    const int secH = H - kPad * 2;
    constexpr int kVisW = 320;
    const int kCtrlW = getWidth() - kPad * 2 - kGap - kVisW;

    visR  = { kPad,             secY, kVisW,  secH };
    ctrlR = { kPad + kVisW + kGap, secY, kCtrlW, secH };

    lfoVis.setBounds (kPad + 6, secY + 20, kVisW - 12, secH - 28);

    const int ctrlBaseY = secY + 20 + (secH - 20 - (kKnobSize + 14 + kLabelH)) / 2;
    juce::Slider* sliders[] = { &lfoRateSlider, &lfoCutoffDepthSlider, &lfoPitchDepthSlider };
    juce::Label*  labels[]  = { &lfoRateLabel,  &lfoCutoffDepthLabel,  &lfoPitchDepthLabel  };
    int kx = kPad + kVisW + kGap + 8;
    for (int i = 0; i < 3; ++i)
    {
        sliders[i]->setBounds (kx, ctrlBaseY, kKnobSize, kKnobSize + 14);
        labels[i]->setBounds  (kx, ctrlBaseY + kKnobSize + 14, kKnobSize, kLabelH);
        kx += kKnobSize + 4;
    }
}

//==============================================================================
// FXTab
//==============================================================================

FXTab::FXTab (Synth1_0AudioProcessor& p)
    : proc (p),
      chorusMixAttach   (p.apvts, "chorusMix",      chorusMixSlider),
      chorusRateAttach  (p.apvts, "chorusRate",     chorusRateSlider),
      chorusDepthAttach (p.apvts, "chorusDepth",    chorusDepthSlider),
      delayTimeAttach   (p.apvts, "delayTime",      delayTimeSlider),
      delayFeedbackAttach(p.apvts,"delayFeedback",  delayFeedbackSlider),
      delayMixAttach    (p.apvts, "delayMix",       delayMixSlider),
      satDriveAttach    (p.apvts, "satDrive",       satDriveSlider),
      satMixAttach      (p.apvts, "satMix",         satMixSlider)
{
    setupKnob (chorusMixSlider,    chorusMixLabel,    "MIX",      this);
    setupKnob (chorusRateSlider,   chorusRateLabel,   "RATE",     this);
    setupKnob (chorusDepthSlider,  chorusDepthLabel,  "DEPTH",    this);
    setupKnob (delayTimeSlider,    delayTimeLabel,    "TIME",     this);
    setupKnob (delayFeedbackSlider,delayFeedbackLabel,"FEEDBACK", this);
    setupKnob (delayMixSlider,     delayMixLabel,     "MIX",      this);
    setupKnob (satDriveSlider,     satDriveLabel,     "DRIVE",    this);
    setupKnob (satMixSlider,       satMixLabel,       "MIX",      this);
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
    drawPanel (satR,    "SATURATION  (2× oversampled)");
}

void FXTab::resized()
{
    const int W   = getWidth();
    const int H   = getHeight();
    const int secY = kPad;
    const int secH = H - kPad * 2;

    // Three equal panels
    const int panelW = (W - kPad * 2 - kGap * 2) / 3;
    const int ctrlH  = kKnobSize + 14 + kLabelH;
    const int ctrlY  = secY + 20 + (secH - 20 - ctrlH) / 2;

    int px = kPad;

    // Chorus
    chorusR = { px, secY, panelW, secH };
    {
        juce::Slider* sl[] = { &chorusMixSlider, &chorusRateSlider, &chorusDepthSlider };
        juce::Label*  la[] = { &chorusMixLabel,  &chorusRateLabel,  &chorusDepthLabel  };
        int kx = px + (panelW - kKnobSize * 3) / 2;
        for (int i = 0; i < 3; ++i)
        {
            sl[i]->setBounds (kx, ctrlY, kKnobSize, kKnobSize + 14);
            la[i]->setBounds (kx, ctrlY + kKnobSize + 14, kKnobSize, kLabelH);
            kx += kKnobSize;
        }
    }
    px += panelW + kGap;

    // Delay
    delayR = { px, secY, panelW, secH };
    {
        juce::Slider* sl[] = { &delayTimeSlider, &delayFeedbackSlider, &delayMixSlider };
        juce::Label*  la[] = { &delayTimeLabel,  &delayFeedbackLabel,  &delayMixLabel  };
        int kx = px + (panelW - kKnobSize * 3) / 2;
        for (int i = 0; i < 3; ++i)
        {
            sl[i]->setBounds (kx, ctrlY, kKnobSize, kKnobSize + 14);
            la[i]->setBounds (kx, ctrlY + kKnobSize + 14, kKnobSize, kLabelH);
            kx += kKnobSize;
        }
    }
    px += panelW + kGap;

    // Saturation
    satR = { px, secY, panelW, secH };
    {
        juce::Slider* sl[] = { &satDriveSlider, &satMixSlider };
        juce::Label*  la[] = { &satDriveLabel,  &satMixLabel  };
        int kx = px + (panelW - kKnobSize * 2) / 2;
        for (int i = 0; i < 2; ++i)
        {
            sl[i]->setBounds (kx, ctrlY, kKnobSize, kKnobSize + 14);
            la[i]->setBounds (kx, ctrlY + kKnobSize + 14, kKnobSize, kLabelH);
            kx += kKnobSize;
        }
    }
}

//==============================================================================
// Synth1_0AudioProcessorEditor
//==============================================================================

Synth1_0AudioProcessorEditor::Synth1_0AudioProcessorEditor (Synth1_0AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&laf);

    tabs.addTab ("MAIN", SynthColors::bg, new MainTab (p), true);
    tabs.addTab ("MOD",  SynthColors::bg, new ModTab  (p), true);
    tabs.addTab ("FX",   SynthColors::bg, new FXTab   (p), true);
    addAndMakeVisible (tabs);

    setSize (720, kHeaderH + kTabH);
}

Synth1_0AudioProcessorEditor::~Synth1_0AudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void Synth1_0AudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace SynthColors;
    g.fillAll (bg);

    // Header
    g.setColour (surface);
    g.fillRect (0, 0, getWidth(), kHeaderH);
    g.setColour (accent.withAlpha (0.2f));
    g.fillRect (0, kHeaderH - 1, getWidth(), 1);

    g.setColour (accent);
    g.setFont (juce::FontOptions (13.5f));
    g.drawText ("SYNTH  1.0", kPad, 0, 180, kHeaderH, juce::Justification::centredLeft);

    g.setColour (subtext);
    g.setFont (juce::FontOptions (9.0f));
    g.drawText ("WAVETABLE SYNTHESIZER",
                0, 0, getWidth() - kPad, kHeaderH, juce::Justification::centredRight);
}

void Synth1_0AudioProcessorEditor::resized()
{
    tabs.setBounds (0, kHeaderH, getWidth(), kTabH);
}
