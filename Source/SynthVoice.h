#pragma once
#include <JuceHeader.h>
#include "WavetableOscillator.h"

class SynthVoice : public juce::SynthesiserVoice
{
public:
    static constexpr int kMaxUnisonVoices = 8;

    explicit SynthVoice (juce::AudioProcessorValueTreeState& apvts)
        : attackParam      (apvts.getRawParameterValue ("attack")),
          decayParam       (apvts.getRawParameterValue ("decay")),
          sustainParam     (apvts.getRawParameterValue ("sustain")),
          releaseParam     (apvts.getRawParameterValue ("release")),
          cutoffParam      (apvts.getRawParameterValue ("cutoff")),
          resonanceParam   (apvts.getRawParameterValue ("resonance")),
          driveParam       (apvts.getRawParameterValue ("drive")),
          wtPositionParam  (apvts.getRawParameterValue ("wtPosition")),
          unisonVoicesParam(apvts.getRawParameterValue ("unisonVoices")),
          unisonDetuneParam(apvts.getRawParameterValue ("unisonDetune")),
          unisonSpreadParam(apvts.getRawParameterValue ("unisonSpread")),
          warpModeParam    (apvts.getRawParameterValue ("warpMode")),
          warpAmountParam  (apvts.getRawParameterValue ("warpAmount")),
          glideTimeParam   (apvts.getRawParameterValue ("glideTime"))
    {}

    void prepareToPlay (double sampleRate, int samplesPerBlock)
    {
        currentSampleRate = sampleRate;

        // The cutoff ceiling has to follow the sample rate, not a hard 20 kHz:
        // the TPT filter needs cutoff < fs/2, and below a 40 kHz rate a 20 kHz
        // cutoff sends std::tan(pi * fc / fs) past pi/2 and negative.
        maxCutoffHz  = static_cast<float> (juce::jmin (20000.0, 0.49 * sampleRate));
        lastCutoffHz = -1.0f;

        for (int u = 0; u < kMaxUnisonVoices; ++u)
            unisonOscs[u].prepare (sampleRate);

        juce::dsp::ProcessSpec spec;
        spec.sampleRate       = sampleRate;
        spec.maximumBlockSize = static_cast<uint32_t> (samplesPerBlock);
        spec.numChannels      = 2;
        svFilter.prepare (spec);
        svFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

        adsr.setSampleRate (sampleRate);

        smoothCutoffMod.reset (sampleRate, 0.02);
        smoothPitchMod.reset  (sampleRate, 0.02);

        smoothedFreqHz.reset (sampleRate, 0.0);
        smoothedFreqHz.setCurrentAndTargetValue (440.0f);

        const auto ns = static_cast<size_t> (samplesPerBlock);
        tempBuffer.assign (ns, 0.0f);
        stereoL.assign    (ns, 0.0f);
        stereoR.assign    (ns, 0.0f);
        isPrepared = true;
    }

    void setLFOMod (float cutoffHz, float pitchSemitones) noexcept
    {
        smoothCutoffMod.setTargetValue (cutoffHz);
        smoothPitchMod.setTargetValue  (pitchSemitones);
    }

    // Called by processor for legato pitch change without ADSR retrigger
    void setLegatoNote (int midiNoteNumber) noexcept
    {
        if (!adsr.isActive()) return;
        baseFreqHz      = static_cast<float> (juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber));
        currentMidiNote = static_cast<float> (midiNoteNumber);
        const float gt  = glideTimeParam->load();
        if (gt > 0.001f) {
            smoothedFreqHz.reset        (currentSampleRate, gt);
            smoothedFreqHz.setTargetValue (baseFreqHz);
        } else {
            smoothedFreqHz.setCurrentAndTargetValue (baseFreqHz);
        }
    }

    bool canPlaySound (juce::SynthesiserSound*) override { return true; }

    void startNote (int midiNoteNumber, float velocity,
                    juce::SynthesiserSound*, int) override
    {
        currentMidiNote = static_cast<float> (midiNoteNumber);
        baseFreqHz      = static_cast<float> (juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber));
        noteVelocity    = velocity;

        const float gt = glideTimeParam->load();
        if (gt > 0.001f && adsr.isActive()) {
            smoothedFreqHz.reset        (currentSampleRate, gt);
            smoothedFreqHz.setTargetValue (baseFreqHz);
        } else {
            smoothedFreqHz.setCurrentAndTargetValue (baseFreqHz);
        }

        const float wtPos  = wtPositionParam->load();
        const int   wm     = static_cast<int> (warpModeParam->load() + 0.5f);
        const float wa     = warpAmountParam->load();
        for (int u = 0; u < kMaxUnisonVoices; ++u)
        {
            unisonOscs[u].setFrequency    (baseFreqHz);
            unisonOscs[u].setWavePosition (wtPos);
            unisonOscs[u].setWarpMode     (wm);
            unisonOscs[u].setWarpAmount   (wa);

            // Randomised, not zero and not evenly spaced.
            //
            // Resetting every unison oscillator to phase 0 made them one signal
            // copied N times, so the 1/sqrt(N) normalisation below (the law for
            // *incoherent* sources) left the attack of every note up to
            // sqrt(8) = +9 dB hot, and at zero detune they never drifted apart:
            // the voice-count knob was a gain knob.
            //
            // Evenly spaced phases (u/N) are worse, not better: summing N copies
            // of one waveform at phases k/N cancels every harmonic that is not a
            // multiple of N, so an 8-voice unison would come out three octaves up,
            // on 8*f0, with seven eighths of its spectrum gone. Random phases
            // decorrelate without that structure, which is exactly the assumption
            // 1/sqrt(N) is already making.
            //
            // The generator is seeded from the voice's construction index, so a
            // given note sequence from a fresh instance still renders identically.
            // Oscillator 0 keeps phase 0 so that unison = 1 is bit-identical to
            // the behaviour before this change.
            unisonOscs[u].resetPhase (u == 0 ? 0.0 : rng.nextDouble());
        }
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

        const int numU = juce::jlimit (1, kMaxUnisonVoices,
                            static_cast<int> (unisonVoicesParam->load() + 0.5f));
        const float detune = unisonDetuneParam->load();
        const float spread = unisonSpreadParam->load();

        const int   wm       = static_cast<int> (warpModeParam->load() + 0.5f);
        const float wa       = warpAmountParam->load();
        const float wtPos    = wtPositionParam->load();

        // Glide + pitch LFO, once per call. The processor now calls this on a
        // fixed 32-sample control chunk rather than on the host's buffer, so
        // the step size stops depending on the host: an octave portamento in
        // 100 ms moves in ~9 cent steps at any buffer size, where it used to be
        // 139 cents at 512 samples and 279 at 1024.
        //
        // Kept in double to the setFrequency call: the oscillator widened that
        // argument on purpose (a float32 Hz value leaves the tone slightly
        // non-periodic, measurable as -86 dBc of leakage) and computing the
        // frequency in float threw the precision away one line before the call.
        const double glideHz = smoothedFreqHz.skip (numSamples);
        const double pMod    = smoothPitchMod.skip (numSamples);
        const double modBase = glideHz * std::pow (2.0, pMod / 12.0);

        const auto ns = static_cast<size_t> (numSamples);
        if (ns > tempBuffer.size())
        {
            tempBuffer.resize (ns, 0.0f);
            stereoL.resize    (ns, 0.0f);
            stereoR.resize    (ns, 0.0f);
        }

        std::fill (stereoL.begin(), stereoL.begin() + numSamples, 0.0f);
        std::fill (stereoR.begin(), stereoR.begin() + numSamples, 0.0f);

        const float normGain = 1.0f / std::sqrt (static_cast<float> (numU));

        for (int u = 0; u < numU; ++u)
        {
            float detuneSt = 0.0f;
            float pan      = 0.5f;
            if (numU > 1)
            {
                const float t = static_cast<float> (u) / static_cast<float> (numU - 1);
                detuneSt = (t - 0.5f) * detune * 2.0f;
                pan      = 0.5f + (t - 0.5f) * spread;
            }

            const double freq = juce::jlimit (20.0, 20000.0,
                modBase * std::pow (2.0, static_cast<double> (detuneSt) / 12.0));

            unisonOscs[u].setFrequency    (freq);
            unisonOscs[u].setWavePosition (wtPos);
            unisonOscs[u].setWarpMode     (wm);
            unisonOscs[u].setWarpAmount   (wa);
            unisonOscs[u].renderBlock     (tempBuffer.data(), numSamples);

            const float panAngle = pan * juce::MathConstants<float>::halfPi;
            const float gainL    = std::cos (panAngle) * normGain;
            const float gainR    = std::sin (panAngle) * normGain;

            for (int i = 0; i < numSamples; ++i)
            {
                stereoL[i] += tempBuffer[i] * gainL;
                stereoR[i] += tempBuffer[i] * gainR;
            }
        }

        const int numChannels = outputBuffer.getNumChannels();
        for (int i = 0; i < numSamples; ++i)
        {
            const float cMod = smoothCutoffMod.getNextValue();
            const float env  = adsr.getNextSample();

            // setCutoffFrequency() is not a store: it runs update(), which is a
            // std::tan and two divisions. Called unconditionally per sample per
            // voice it was 705,600 tangents a second at 16 voices, recomputing
            // identical coefficients whenever the modulation was not moving,
            // which includes every patch with the LFO cutoff depth at zero.
            const float target = juce::jlimit (20.0f, maxCutoffHz, cutoff + cMod);
            if (std::abs (target - lastCutoffHz) > lastCutoffHz * 1.0e-4f)
            {
                svFilter.setCutoffFrequency (target);
                lastCutoffHz = target;
            }

            float sL = juce::dsp::FastMathApproximations::tanh (stereoL[i] * drive);
            float sR = juce::dsp::FastMathApproximations::tanh (stereoR[i] * drive);
            sL = svFilter.processSample (0, sL) * env * noteVelocity;
            sR = svFilter.processSample (1, sR) * env * noteVelocity;

            outputBuffer.addSample (0, startSample + i, sL);
            if (numChannels > 1)
                outputBuffer.addSample (1, startSample + i, sR);
        }

        if (!adsr.isActive()) { svFilter.reset(); clearCurrentNote(); }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

private:
    WavetableOscillator                      unisonOscs[kMaxUnisonVoices];
    juce::ADSR                               adsr;
    juce::dsp::StateVariableTPTFilter<float> svFilter;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>        smoothCutoffMod;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>        smoothPitchMod;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smoothedFreqHz;

    std::vector<float> tempBuffer, stereoL, stereoR;
    float noteVelocity    = 1.0f;
    float baseFreqHz      = 440.0f;
    float currentMidiNote = 69.0f;
    double currentSampleRate = 44100.0;
    bool  isPrepared      = false;

    float maxCutoffHz     = 20000.0f;   // 0.49 * fs, so the TPT filter stays sane
    float lastCutoffHz    = -1.0f;      // last value actually pushed into svFilter

    // Seeded from construction order so note-on phases are random but a fresh
    // instance renders a given sequence identically twice.
    inline static int sVoiceCounter = 0;
    juce::Random rng { static_cast<juce::int64> (++sVoiceCounter) * 2654435761LL };

    std::atomic<float>* attackParam       = nullptr;
    std::atomic<float>* decayParam        = nullptr;
    std::atomic<float>* sustainParam      = nullptr;
    std::atomic<float>* releaseParam      = nullptr;
    std::atomic<float>* cutoffParam       = nullptr;
    std::atomic<float>* resonanceParam    = nullptr;
    std::atomic<float>* driveParam        = nullptr;
    std::atomic<float>* wtPositionParam   = nullptr;
    std::atomic<float>* unisonVoicesParam = nullptr;
    std::atomic<float>* unisonDetuneParam = nullptr;
    std::atomic<float>* unisonSpreadParam = nullptr;
    std::atomic<float>* warpModeParam     = nullptr;
    std::atomic<float>* warpAmountParam   = nullptr;
    std::atomic<float>* glideTimeParam    = nullptr;
};
