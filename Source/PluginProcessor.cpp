#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SynthSound.h"
#include "SynthVoice.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
Synth1_0AudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    // ── ADSR ──────────────────────────────────────────────────────────────────
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID{"attack",  1}, "Attack",
        NormalisableRange<float>{0.001f, 2.0f, 0.001f, 0.4f}, 0.01f));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID{"decay",   1}, "Decay",
        NormalisableRange<float>{0.001f, 2.0f, 0.001f, 0.4f}, 0.1f));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID{"sustain", 1}, "Sustain",
        NormalisableRange<float>{0.0f, 1.0f, 0.001f}, 0.8f));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID{"release", 1}, "Release",
        NormalisableRange<float>{0.01f, 5.0f, 0.001f, 0.4f}, 0.2f));

    // ── Oscillator ────────────────────────────────────────────────────────────
    // 0=Sine 1=Saw 2=Square 3=Triangle; fractional values morph between tables
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID{"wtPosition", 1}, "WT Position",
        NormalisableRange<float>{0.0f, 3.0f, 0.001f}, 0.0f));

    // ── Filter ────────────────────────────────────────────────────────────────
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID{"cutoff",    1}, "Cutoff",
        NormalisableRange<float>{20.0f, 20000.0f, 0.1f, 0.25f}, 8000.0f));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID{"resonance", 1}, "Resonance",
        NormalisableRange<float>{0.1f, 10.0f, 0.01f}, 0.7f));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID{"drive",     1}, "Drive",
        NormalisableRange<float>{1.0f, 10.0f, 0.01f}, 1.0f));

    // ── Master ────────────────────────────────────────────────────────────────
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID{"gain", 1}, "Master Gain",
        NormalisableRange<float>{0.0f, 1.0f, 0.001f}, 0.7f));

    // ── LFO ───────────────────────────────────────────────────────────────────
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID{"lfoRate",        1}, "LFO Rate",
        NormalisableRange<float>{0.1f, 20.0f, 0.01f, 0.4f}, 1.0f));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID{"lfoCutoffDepth", 1}, "LFO→Cutoff",
        NormalisableRange<float>{0.0f, 1.0f, 0.001f}, 0.0f));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID{"lfoPitchDepth",  1}, "LFO→Pitch",
        NormalisableRange<float>{0.0f, 48.0f, 0.01f}, 0.0f));

    // ── FX: Chorus ────────────────────────────────────────────────────────────
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID{"chorusMix",   1}, "Chorus Mix",
        NormalisableRange<float>{0.0f, 1.0f, 0.001f}, 0.0f));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID{"chorusRate",  1}, "Chorus Rate",
        NormalisableRange<float>{0.1f, 8.0f, 0.01f}, 1.0f));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID{"chorusDepth", 1}, "Chorus Depth",
        NormalisableRange<float>{0.0f, 1.0f, 0.001f}, 0.25f));

    // ── FX: Delay ─────────────────────────────────────────────────────────────
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID{"delayTime",     1}, "Delay Time",
        NormalisableRange<float>{10.0f, 1000.0f, 1.0f, 0.4f}, 250.0f));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID{"delayFeedback", 1}, "Delay Feedback",
        NormalisableRange<float>{0.0f, 0.95f, 0.001f}, 0.3f));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID{"delayMix",      1}, "Delay Mix",
        NormalisableRange<float>{0.0f, 1.0f, 0.001f}, 0.0f));

    // ── FX: Saturation ────────────────────────────────────────────────────────
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID{"satDrive", 1}, "Sat Drive",
        NormalisableRange<float>{1.0f, 20.0f, 0.01f}, 1.0f));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID{"satMix",   1}, "Sat Mix",
        NormalisableRange<float>{0.0f, 1.0f, 0.001f}, 0.0f));

    return layout;
}

//==============================================================================
Synth1_0AudioProcessor::Synth1_0AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsSynth
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                     #endif
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
#else
    :
#endif
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    masterGainParam     = apvts.getRawParameterValue ("gain");
    lfoRateParam        = apvts.getRawParameterValue ("lfoRate");
    lfoCutoffDepthParam = apvts.getRawParameterValue ("lfoCutoffDepth");
    lfoPitchDepthParam  = apvts.getRawParameterValue ("lfoPitchDepth");
    chorusMixParam      = apvts.getRawParameterValue ("chorusMix");
    chorusRateParam     = apvts.getRawParameterValue ("chorusRate");
    chorusDepthParam    = apvts.getRawParameterValue ("chorusDepth");
    delayTimeParam      = apvts.getRawParameterValue ("delayTime");
    delayFeedbackParam  = apvts.getRawParameterValue ("delayFeedback");
    delayMixParam       = apvts.getRawParameterValue ("delayMix");
    satDriveParam       = apvts.getRawParameterValue ("satDrive");
    satMixParam         = apvts.getRawParameterValue ("satMix");

    for (int i = 0; i < 16; ++i)
        synth.addVoice (new SynthVoice (apvts));
    synth.addSound (new SynthSound());
}

Synth1_0AudioProcessor::~Synth1_0AudioProcessor() {}

//==============================================================================
void Synth1_0AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSR = sampleRate;
    synth.setCurrentPlaybackSampleRate (sampleRate);

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<SynthVoice*> (synth.getVoice (i)))
            v->prepareToPlay (sampleRate, samplesPerBlock);

    // Chorus
    juce::dsp::ProcessSpec spec { sampleRate, static_cast<uint32_t>(samplesPerBlock), 2 };
    chorus.prepare (spec);
    chorus.setCentreDelay (7.0f);
    chorus.setFeedback    (0.0f);

    // Delay lines (max 2 s)
    juce::dsp::ProcessSpec monoSpec { sampleRate, static_cast<uint32_t>(samplesPerBlock), 1 };
    const int maxDelaySamples = static_cast<int> (sampleRate * 2.0);
    delayLineL.prepare (monoSpec);
    delayLineR.prepare (monoSpec);
    delayLineL.setMaximumDelayInSamples (maxDelaySamples);
    delayLineR.setMaximumDelayInSamples (maxDelaySamples);
    delayFeedbackState[0] = 0.0f;
    delayFeedbackState[1] = 0.0f;

    // Oversampler
    oversampler.initProcessing (static_cast<size_t> (samplesPerBlock));

    // Pre-allocate dry buffer (for saturation wet/dry)
    dryBuffer.setSize (2, samplesPerBlock, false, true, true);

    lfoPhase = 0.0f;
}

void Synth1_0AudioProcessor::releaseResources()
{
    chorus.reset();
    delayLineL.reset();
    delayLineR.reset();
    oversampler.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool Synth1_0AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
  #endif
}
#endif

//==============================================================================
void Synth1_0AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int N = buffer.getNumSamples();

    // ── LFO (per-block, sine wave) ────────────────────────────────────────────
    const float lfoRate = lfoRateParam->load();
    lfoPhase += (lfoRate / static_cast<float> (currentSR)) * static_cast<float> (N);
    if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
    const float lfoVal = std::sin (lfoPhase * juce::MathConstants<float>::twoPi);

    lfoVisBuf.write (lfoVal);

    const float cutoffMod = lfoVal * lfoCutoffDepthParam->load() * 4000.0f;
    const float pitchMod  = lfoVal * lfoPitchDepthParam->load();

    // Push LFO mod to voices (audio thread — no synchronisation needed)
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<SynthVoice*> (synth.getVoice (i)))
            v->setLFOMod (cutoffMod, pitchMod);

    // ── Synth ─────────────────────────────────────────────────────────────────
    synth.renderNextBlock (buffer, midiMessages, 0, N);
    buffer.applyGain (masterGainParam->load());

    // ── Chorus ────────────────────────────────────────────────────────────────
    {
        const float mix = chorusMixParam->load();
        if (mix > 0.001f)
        {
            chorus.setRate  (chorusRateParam->load());
            chorus.setDepth (chorusDepthParam->load());
            chorus.setMix   (mix);
            juce::dsp::AudioBlock<float>           block  (buffer);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            chorus.process (ctx);
        }
    }

    // ── Delay (tempo-synced when playhead available) ───────────────────────────
    {
        const float delayMix = delayMixParam->load();
        if (delayMix > 0.001f)
            processDelay (buffer, N);
    }

    // ── Saturation with 2× oversampling ───────────────────────────────────────
    {
        const float satMix   = satMixParam->load();
        const float satDrive = satDriveParam->load();

        if (satMix > 0.001f)
        {
            // Save dry signal (pre-allocated buffer — no allocation here)
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                dryBuffer.copyFrom (ch, 0, buffer, ch, 0, N);

            juce::dsp::AudioBlock<float> block (buffer);
            auto overBlock = oversampler.processSamplesUp (block);

            // WaveShaper: tanh(x * drive) — analog saturation
            for (size_t ch = 0; ch < overBlock.getNumChannels(); ++ch)
            {
                float* data = overBlock.getChannelPointer (ch);
                for (size_t i = 0; i < overBlock.getNumSamples(); ++i)
                    data[i] = std::tanh (data[i] * satDrive);
            }

            oversampler.processSamplesDown (block);

            // Wet/dry mix: buffer = wet*satMix + dry*(1-satMix)
            buffer.applyGain (satMix);
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.addFrom (ch, 0, dryBuffer, ch, 0, N, 1.0f - satMix);
        }
    }
}

//==============================================================================
void Synth1_0AudioProcessor::processDelay (juce::AudioBuffer<float>& buffer, int numSamples)
{
    // Tempo sync: quarter-note delay when playhead available
    float delaySamples = delayTimeParam->load() * 0.001f * static_cast<float> (currentSR);

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm())
            {
                // Snap to quarter note at current BPM
                const float quarterNoteSamples =
                    static_cast<float> (60.0 / *bpm * currentSR);
                delaySamples = quarterNoteSamples;
            }
        }
    }

    const float feedback = delayFeedbackParam->load();
    const float mix      = delayMixParam->load();

    auto& lineL = delayLineL;
    auto& lineR = (buffer.getNumChannels() > 1) ? delayLineR : delayLineL;

    lineL.setDelay (delaySamples);
    lineR.setDelay (delaySamples);

    const int numCh = buffer.getNumChannels();

    for (int i = 0; i < numSamples; ++i)
    {
        const float dryL = buffer.getSample (0, i);
        const float dryR = (numCh > 1) ? buffer.getSample (1, i) : dryL;

        const float wetL = lineL.popSample (0);
        const float wetR = lineR.popSample (0);

        lineL.pushSample (0, dryL + wetL * feedback);
        lineR.pushSample (0, dryR + wetR * feedback);

        buffer.setSample (0, i, dryL + wetL * mix);
        if (numCh > 1)
            buffer.setSample (1, i, dryR + wetR * mix);
    }
}

//==============================================================================
void Synth1_0AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void Synth1_0AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
const juce::String Synth1_0AudioProcessor::getName() const { return JucePlugin_Name; }
bool   Synth1_0AudioProcessor::acceptsMidi() const          { return true;  }
bool   Synth1_0AudioProcessor::producesMidi() const         { return false; }
bool   Synth1_0AudioProcessor::isMidiEffect() const         { return false; }
double Synth1_0AudioProcessor::getTailLengthSeconds() const { return 5.0;   }

int    Synth1_0AudioProcessor::getNumPrograms()             { return 1;    }
int    Synth1_0AudioProcessor::getCurrentProgram()          { return 0;    }
void   Synth1_0AudioProcessor::setCurrentProgram (int)      {}
const  juce::String Synth1_0AudioProcessor::getProgramName (int) { return {}; }
void   Synth1_0AudioProcessor::changeProgramName (int, const juce::String&) {}

bool   Synth1_0AudioProcessor::hasEditor() const            { return true; }
juce::AudioProcessorEditor* Synth1_0AudioProcessor::createEditor()
{
    return new Synth1_0AudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Synth1_0AudioProcessor();
}
