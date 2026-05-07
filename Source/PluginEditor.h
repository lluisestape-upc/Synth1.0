#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "WavetableOscillator.h"

namespace SynthColors
{
    extern juce::Colour bg, surface, panel, border, accent, text, subtext, track, modRing;
    void        applySkin        (int index);
    int         getCurrentSkinIndex ();
    int         getNumSkins      ();
    const char* getSkinName      (int index);
}

//==============================================================================
class SynthLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SynthLookAndFeel();
    void refreshColours();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;
    void drawLinearSlider  (juce::Graphics&, int x, int y, int w, int h,
                            float sliderPos, float minSliderPos, float maxSliderPos,
                            juce::Slider::SliderStyle, juce::Slider&) override;
    void drawLabel         (juce::Graphics&, juce::Label&) override;

    // Set by editor to enable the LFO modulation ring on the cutoff knob
    std::atomic<float>* lfoCutoffDepth = nullptr;
};

//==============================================================================
class ADSRVisualizer : public juce::Component, public juce::Timer
{
public:
    explicit ADSRVisualizer (juce::AudioProcessorValueTreeState& apvts);
    ~ADSRVisualizer() override { stopTimer(); }
    void timerCallback() override { repaint(); }
    void paint (juce::Graphics&) override;
private:
    juce::AudioProcessorValueTreeState& apvts;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ADSRVisualizer)
};

//==============================================================================
class WTVisualizer : public juce::Component, public juce::Timer
{
public:
    explicit WTVisualizer (juce::AudioProcessorValueTreeState& apvts);
    ~WTVisualizer() override { stopTimer(); }
    void timerCallback() override { repaint(); }
    void paint (juce::Graphics&) override;
private:
    juce::AudioProcessorValueTreeState& apvts;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WTVisualizer)
};

//==============================================================================
class LFOVisualizer : public juce::Component, public juce::Timer
{
public:
    explicit LFOVisualizer (Synth1_0AudioProcessor& p);
    ~LFOVisualizer() override { stopTimer(); }
    void timerCallback() override;
    void paint (juce::Graphics&) override;
private:
    Synth1_0AudioProcessor& processor;
    std::array<float, LfoVisBuf::kSize> snapData {};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LFOVisualizer)
};

//==============================================================================
inline void setupKnob (juce::Slider& s, juce::Label& l,
                        const juce::String& name,
                        juce::Component* parent)
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 14);
    parent->addAndMakeVisible (s);
    l.setText (name, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setFont (juce::FontOptions (9.5f));
    parent->addAndMakeVisible (l);
}

//==============================================================================
class MainTab : public juce::Component
{
public:
    explicit MainTab (Synth1_0AudioProcessor& p);
    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    Synth1_0AudioProcessor& proc;

    ADSRVisualizer adsrVis;
    WTVisualizer   wtVis;

    // WT position
    juce::Slider wtSlider;
    juce::Label  wtLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment wtAttach;

    // ADSR
    juce::Slider attackSlider,  decaySlider,  sustainSlider,  releaseSlider;
    juce::Label  attackLabel,   decayLabel,   sustainLabel,   releaseLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment
        attackAttach, decayAttach, sustainAttach, releaseAttach;

    // Filter
    juce::Slider cutoffSlider, resonanceSlider, driveSlider;
    juce::Label  cutoffLabel,  resonanceLabel,  driveLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment
        cutoffAttach, resonanceAttach, driveAttach;

    // Master
    juce::Slider gainSlider;
    juce::Label  gainLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment gainAttach;

    // Warp
    juce::ComboBox warpModeCombo;
    juce::Label    warpModeLabel;
    juce::Slider   warpAmountSlider;
    juce::Label    warpAmountLabel;
    // combo attachment created after items added (init order fix)
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> warpModeAttach;
    juce::AudioProcessorValueTreeState::SliderAttachment                    warpAmountAttach;

    juce::Rectangle<int> oscR, adsrR, filterR, outR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainTab)
};

//==============================================================================
class ModTab : public juce::Component
{
public:
    explicit ModTab (Synth1_0AudioProcessor& p);
    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    Synth1_0AudioProcessor& proc;
    LFOVisualizer lfoVis;

    juce::Slider lfoRateSlider,    lfoCutoffDepthSlider, lfoPitchDepthSlider;
    juce::Label  lfoRateLabel,     lfoCutoffDepthLabel,  lfoPitchDepthLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment
        lfoRateAttach, lfoCutoffDepthAttach, lfoPitchDepthAttach;

    // Unison
    juce::Slider unisonVoicesSlider, unisonDetuneSlider, unisonSpreadSlider;
    juce::Label  unisonVoicesLabel,  unisonDetuneLabel,  unisonSpreadLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment
        unisonVoicesAttach, unisonDetuneAttach, unisonSpreadAttach;

    // Voice mode & glide
    juce::ComboBox voiceModeCombo;
    juce::Label    voiceModeLabel;
    juce::Slider   glideTimeSlider;
    juce::Label    glideTimeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> voiceModeAttach;
    juce::AudioProcessorValueTreeState::SliderAttachment                    glideTimeAttach;

    juce::Rectangle<int> visR, lfoR, unisonR, voiceR;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModTab)
};

//==============================================================================
class FXTab : public juce::Component
{
public:
    explicit FXTab (Synth1_0AudioProcessor& p);
    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    Synth1_0AudioProcessor& proc;

    // Chorus
    juce::Slider chorusMixSlider, chorusRateSlider, chorusDepthSlider;
    juce::Label  chorusMixLabel,  chorusRateLabel,  chorusDepthLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment
        chorusMixAttach, chorusRateAttach, chorusDepthAttach;

    // Delay
    juce::Slider delayTimeSlider, delayFeedbackSlider, delayMixSlider;
    juce::Label  delayTimeLabel,  delayFeedbackLabel,  delayMixLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment
        delayTimeAttach, delayFeedbackAttach, delayMixAttach;

    // Saturation
    juce::Slider satDriveSlider, satMixSlider;
    juce::Label  satDriveLabel,  satMixLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment
        satDriveAttach, satMixAttach;

    // Phaser
    juce::Slider phaserRateSlider, phaserDepthSlider, phaserMixSlider;
    juce::Label  phaserRateLabel,  phaserDepthLabel,  phaserMixLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment
        phaserRateAttach, phaserDepthAttach, phaserMixAttach;

    // Reverb
    juce::Slider reverbSizeSlider, reverbDampingSlider, reverbMixSlider;
    juce::Label  reverbSizeLabel,  reverbDampingLabel,  reverbMixLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment
        reverbSizeAttach, reverbDampingAttach, reverbMixAttach;

    juce::Rectangle<int> chorusR, delayR, satR, phaserR, reverbR;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FXTab)
};

//==============================================================================
class EQVisualizer : public juce::Component, public juce::Timer
{
public:
    static constexpr int kFFTOrder = 11;
    static constexpr int kFFTSize  = 1 << kFFTOrder;   // 2048

    explicit EQVisualizer (Synth1_0AudioProcessor& p);
    ~EQVisualizer() override { stopTimer(); }
    void timerCallback() override;
    void paint (juce::Graphics&) override;

    // Mouse interaction for band editing
    void mouseDown      (const juce::MouseEvent&) override;
    void mouseDrag      (const juce::MouseEvent&) override;
    void mouseUp        (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove      (const juce::MouseEvent&) override;
    void mouseExit      (const juce::MouseEvent&) override;

private:
    // Returns 0=low, 1=mid, 2=high, -1=none — based on proximity in pixel space
    int   findNearestBand (float xPx, float yPx) const;
    float freqToX         (float freq) const;
    float gainToY         (float gainDb) const;
    float xToFreq         (float xPx) const;
    float yToGain         (float yPx) const;

    Synth1_0AudioProcessor& proc;

    juce::dsp::FFT                       fft  { kFFTOrder };
    juce::dsp::WindowingFunction<float>  win  { (size_t) kFFTSize,
                                                juce::dsp::WindowingFunction<float>::hann };
    std::array<float, kFFTSize * 2>      fftBuf  {};
    std::array<float, kFFTSize / 2 + 1>  spectrum {};

    int draggedBand = -1;
    int hoveredBand = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQVisualizer)
};

//==============================================================================
class EQTab : public juce::Component
{
public:
    explicit EQTab (Synth1_0AudioProcessor& p);
    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    Synth1_0AudioProcessor& proc;

    EQVisualizer eqVis;

    // Low shelf
    juce::Slider eqLowFreqSlider,  eqLowGainSlider;
    juce::Label  eqLowFreqLabel,   eqLowGainLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment
        eqLowFreqAttach, eqLowGainAttach;

    // Mid bell (peak)
    juce::Slider eqMidFreqSlider,  eqMidGainSlider,  eqMidQSlider;
    juce::Label  eqMidFreqLabel,   eqMidGainLabel,   eqMidQLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment
        eqMidFreqAttach, eqMidGainAttach, eqMidQAttach;

    // High shelf
    juce::Slider eqHighFreqSlider, eqHighGainSlider;
    juce::Label  eqHighFreqLabel,  eqHighGainLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment
        eqHighFreqAttach, eqHighGainAttach;

    juce::Rectangle<int> visR, lowR, midR, highR;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQTab)
};

//==============================================================================
class SeqTab : public juce::Component, public juce::Timer
{
public:
    explicit SeqTab (Synth1_0AudioProcessor& p);
    ~SeqTab() override;
    void paint   (juce::Graphics&) override;
    void resized () override;
    void mouseDown      (const juce::MouseEvent&) override;
    void mouseDrag      (const juce::MouseEvent&) override;
    void mouseUp        (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void timerCallback  () override;

private:
    static juce::String  midiNoteName (int note);
    int                  getNumSteps  () const;
    juce::Rectangle<int> getStepGridBounds () const;

    Synth1_0AudioProcessor& proc;

    juce::TextButton playBtn    { "PLAY" };
    juce::TextButton triggerBtn { "TRIGGER" };

    juce::Slider bpmSlider;
    juce::Label  bpmLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment bpmAttach;

    juce::ComboBox stepsCombo;
    juce::Label    stepsLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> stepsAttach;

    int dragStep     = -1;
    int dragStartY   = 0;
    int dragStartVel = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeqTab)
};

//==============================================================================
class Synth1_0AudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit Synth1_0AudioProcessorEditor (Synth1_0AudioProcessor&);
    ~Synth1_0AudioProcessorEditor() override;
    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    void setSkin (int index);

    Synth1_0AudioProcessor& audioProcessor;
    SynthLookAndFeel laf;

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    // Preset bar
    juce::ComboBox   presetBox;
    juce::TextButton saveBtn { "SAVE" };

    // Skin selector
    juce::ComboBox skinBox;

    juce::File getPresetsDir() const;
    void       populatePresets();
    void       savePreset();
    void       loadPreset (const juce::File& f);

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Synth1_0AudioProcessorEditor)
};
