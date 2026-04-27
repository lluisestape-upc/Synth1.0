#pragma once
#include <JuceHeader.h>
#include "WavetableOscillator.h"

class SynthVoice : public juce::SynthesiserVoice
{
public:
    explicit SynthVoice (juce::AudioProcessorValueTreeState& apvts)
        : attackParam    (apvts.getRawParameterValue ("attack")),
          decayParam     (apvts.getRawParameterValue ("decay")),
          sustainParam   (apvts.getRawParameterValue ("sustain")),
          releaseParam   (apvts.getRawParameterValue ("release")),
          cutoffParam    (apvts.getRawParameterValue ("cutoff")),
          resonanceParam (apvts.getRawParameterValue ("resonance")),
          driveParam     (apvts.getRawParameterValue ("drive")),
          wtPositionParam(apvts.getRawParameterValue ("wtPosition"))
    {}

    void prepareToPlay (double sampleRate, int samplesPerBlock)
    {
        currentSampleRate = sampleRate;
        oscillator.prepare (sampleRate);

        juce::dsp::ProcessSpec spec;
        spec.sampleRate       = sampleRate;
        spec.maximumBlockSize = static_cast<uint32_t> (samplesPerBlock);
        spec.numChannels      = 1;
        svFilter.prepare (spec);
        svFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

        adsr.setSampleRate (sampleRate);

        smoothCutoffMod.reset (sampleRate, 0.02);
        smoothPitchMod.reset  (sampleRate, 0.02);

        tempBuffer.resize (static_cast<size_t> (samplesPerBlock), 0.0f);
        isPrepared = true;
    }

    // Called from processBlock (audio thread) before renderNextBlock.
    void setLFOMod (float cutoffHz, float pitchSemitones) noexcept
    {
        smoothCutoffMod.setTargetValue (cutoffHz);
        smoothPitchMod.setTargetValue  (pitchSemitones);
    }

    bool canPlaySound (juce::SynthesiserSound*) override { return true; }

    void startNote (int midiNoteNumber, float velocity,
                    juce::SynthesiserSound*, int) override
    {
        currentMidiNote = static_cast<float> (midiNoteNumber);
        baseFreqHz      = static_cast<float> (juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber));
        oscillator.setFrequency   (baseFreqHz);
        oscillator.setWavePosition (wtPositionParam->load());
        oscillator.reset();
        noteVelocity = velocity;
        adsr.noteOn();
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff) adsr.noteOff();
        else { adsr.reset(); svFilter.reset(); clearCurrentNote(); }
    }

    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                          int startSample, int numSamples) override
    {
        jassert (isPrepared);
        if (!adsr.isActive()) { clearCurrentNote(); return; }

        // ADSR params
        {
            juce::ADSR::Parameters p;
            p.attack  = attackParam->load();
            p.decay   = decayParam->load();
            p.sustain = sustainParam->load();
            p.release = releaseParam->load();
            adsr.setParameters (p);
        }

        const float cutoff    = cutoffParam->load();
        const float resonance = resonanceParam->load();
        const float drive     = driveParam->load();
        svFilter.setResonance (juce::jlimit (0.1f, 10.0f, resonance));

        // Pitch LFO: advance smoother by one step and apply once per block
        // (block-rate vibrato is standard; avoids re-rendering the oscillator per sample)
        {
            const float pMod     = smoothPitchMod.skip (numSamples);
            const float modFreq  = baseFreqHz * std::pow (2.0f, pMod / 12.0f);
            oscillator.setFrequency (juce::jlimit (20.0f, 20000.0f, modFreq));
        }

        oscillator.setWavePosition (wtPositionParam->load());

        if (numSamples > static_cast<int> (tempBuffer.size()))
            tempBuffer.resize (static_cast<size_t> (numSamples), 0.0f);

        oscillator.renderBlock (tempBuffer.data(), numSamples);

        const int numChannels = outputBuffer.getNumChannels();

        for (int i = 0; i < numSamples; ++i)
        {
            // Cutoff LFO: per-sample smooth to eliminate stepping artefacts
            const float cMod = smoothCutoffMod.getNextValue();
            const float env  = adsr.getNextSample();

            float s = juce::dsp::FastMathApproximations::tanh (tempBuffer[i] * drive);
            svFilter.setCutoffFrequency (juce::jlimit (20.0f, 20000.0f, cutoff + cMod));
            s = svFilter.processSample (0, s);
            s *= env * noteVelocity;

            for (int ch = 0; ch < numChannels; ++ch)
                outputBuffer.addSample (ch, startSample + i, s);
        }

        if (!adsr.isActive()) { svFilter.reset(); clearCurrentNote(); }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

private:
    WavetableOscillator                      oscillator;
    juce::ADSR                               adsr;
    juce::dsp::StateVariableTPTFilter<float> svFilter;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothCutoffMod;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothPitchMod;

    std::vector<float> tempBuffer;
    float noteVelocity    = 1.0f;
    float baseFreqHz      = 440.0f;
    float currentMidiNote = 69.0f;
    double currentSampleRate = 44100.0;
    bool  isPrepared      = false;

    std::atomic<float>* attackParam     = nullptr;
    std::atomic<float>* decayParam      = nullptr;
    std::atomic<float>* sustainParam    = nullptr;
    std::atomic<float>* releaseParam    = nullptr;
    std::atomic<float>* cutoffParam     = nullptr;
    std::atomic<float>* resonanceParam  = nullptr;
    std::atomic<float>* driveParam      = nullptr;
    std::atomic<float>* wtPositionParam = nullptr;
};
