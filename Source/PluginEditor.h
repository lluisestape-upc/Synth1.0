#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "WavetableOscillator.h"

namespace SynthColors
{
    const juce::Colour bg      { 0xff0f0f1a };
    const juce::Colour surface { 0xff191928 };
    const juce::Colour panel   { 0xff1e1e30 };
    const juce::Colour border  { 0xff35355a };
    const juce::Colour accent  { 0xff89b4fa };
    const juce::Colour text    { 0xffe0e0f0 };
    const juce::Colour subtext { 0xff6868a0 };
    const juce::Colour track   { 0xff2a2a42 };
}

//==============================================================================
class SynthLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SynthLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;
    void drawLinearSlider  (juce::Graphics&, int x, int y, int w, int h,
                            float sliderPos, float minSliderPos, float maxSliderPos,
                            juce::Slider::SliderStyle, juce::Slider&) override;
};

//==============================================================================
// Draws a live ADSR envelope shape; repaints via 30 Hz timer.
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
// Draws the current morphed wavetable shape.
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
// Reads the LFO circular buffer from the processor and shows a scrolling scope.
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
// Helper: sets up a rotary knob + label beneath it.
inline void setupKnob (juce::Slider& s, juce::Label& l,
                        const juce::String& name,
                        juce::Component* parent)
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 14);
    parent->addAndMakeVisible (s);
    l.setText (name, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setFont (juce::FontOptions (9.0f));
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

    // Section rectangles — set in resized(), used in paint()
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

    juce::Rectangle<int> visR, ctrlR;
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

    juce::Rectangle<int> chorusR, delayR, satR;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FXTab)
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
    Synth1_0AudioProcessor& audioProcessor;
    SynthLookAndFeel laf;

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Synth1_0AudioProcessorEditor)
};
