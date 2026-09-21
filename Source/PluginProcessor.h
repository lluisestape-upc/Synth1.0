#pragma once
#include <JuceHeader.h>

// Lock-free ring buffer for LFO visualisation.
// Audio thread writes; UI thread reads a snapshot. Minor torn-read risk is
// acceptable for a display-only consumer.
struct LfoVisBuf
{
    static constexpr int kSize = 512;
    float           data[kSize] {};
    std::atomic<int> writePos { 0 };

    void write (float v) noexcept
    {
        data[writePos.load (std::memory_order_relaxed) % kSize] = v;
        writePos.fetch_add (1, std::memory_order_relaxed);
    }

    // Copies the most recent kSize samples into dst[0..kSize-1] for UI use.
    void snapshot (float* dst) const noexcept
    {
        const int wp = writePos.load (std::memory_order_acquire);
        for (int i = 0; i < kSize; ++i)
            dst[i] = data[(wp + i) % kSize];
    }
};

// Larger ring buffer for spectrum analysis (holds 4096 samples — fits a 2048-point FFT).
struct SpecVisBuf
{
    static constexpr int kSize = 4096;
    float            data[kSize] {};
    std::atomic<int> writePos { 0 };

    void write (float v) noexcept
    {
        data[writePos.load (std::memory_order_relaxed) % kSize] = v;
        writePos.fetch_add (1, std::memory_order_relaxed);
    }
};

class Synth1_0AudioProcessor : public juce::AudioProcessor
{
public:
    Synth1_0AudioProcessor();
    ~Synth1_0AudioProcessor() override;

    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout&) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool   acceptsMidi() const override;
    bool   producesMidi() const override;
    bool   isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int  getNumPrograms() override;
    int  getCurrentProgram() override;
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Public — editor attaches controls via APVTS attachments
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // LFO vis buffer — UI reads snapshots from here
    LfoVisBuf lfoVisBuf;

    // Spectrum vis buffer — larger ring to hold a full FFT window of audio data
    SpecVisBuf specVisBuf;

    // Sequencer shared state (audio ↔ UI, lock-free)
    static constexpr int kSeqMaxSteps = 16;
    std::atomic<int>  seqNotes      [kSeqMaxSteps];
    std::atomic<int>  seqVelocities [kSeqMaxSteps];
    std::atomic<bool> seqActives    [kSeqMaxSteps];
    std::atomic<int>  seqCurrentStep  { 0 };
    std::atomic<bool> seqPlaying      { false };
    std::atomic<bool> seqTriggerMode  { false };

private:
    // ── Synth ─────────────────────────────────────────────────────────────────
    juce::Synthesiser synth;

    /** Renders the voices in fixed-size chunks, advancing the LFO once per chunk.

        Everything modulated per block is a signal sampled at fs/N, where N is
        whatever buffer size the host happens to use. Its Nyquist is fs/2N: 43 Hz
        on a 512-sample buffer, 10.8 Hz on a 2048-sample one. With a 20 Hz LFO
        that folds, and the rate knob stops meaning what it says as soon as the
        user changes their buffer setting. Rendering in fixed chunks decouples
        the control rate from the host: fs/32 = 1378 Hz at 44.1 kHz.

        juce::Synthesiser::renderNextBlock takes MIDI positions as absolute
        offsets into the buffer, so the same MidiBuffer can be handed to every
        chunk with only startSample moving.
    */
    void renderVoices (juce::AudioBuffer<float>& buffer,
                       const juce::MidiBuffer& midi,
                       int numSamples);

    // ── Cached param pointers (audio thread only) ─────────────────────────────
    std::atomic<float>* masterGainParam    = nullptr;
    std::atomic<float>* lfoRateParam       = nullptr;
    std::atomic<float>* lfoCutoffDepthParam= nullptr;
    std::atomic<float>* lfoPitchDepthParam = nullptr;
    std::atomic<float>* chorusMixParam     = nullptr;
    std::atomic<float>* chorusRateParam    = nullptr;
    std::atomic<float>* chorusDepthParam   = nullptr;
    std::atomic<float>* delayTimeParam     = nullptr;
    std::atomic<float>* delayFeedbackParam = nullptr;
    std::atomic<float>* delayMixParam      = nullptr;
    std::atomic<float>* satDriveParam      = nullptr;
    std::atomic<float>* satMixParam        = nullptr;
    std::atomic<float>* phaserRateParam    = nullptr;
    std::atomic<float>* phaserDepthParam   = nullptr;
    std::atomic<float>* phaserMixParam     = nullptr;

    // EQ
    std::atomic<float>* eqLowFreqParam    = nullptr;
    std::atomic<float>* eqLowGainParam    = nullptr;
    std::atomic<float>* eqMidFreqParam    = nullptr;
    std::atomic<float>* eqMidGainParam    = nullptr;
    std::atomic<float>* eqMidQParam       = nullptr;
    std::atomic<float>* eqHighFreqParam   = nullptr;
    std::atomic<float>* eqHighGainParam   = nullptr;

    // Reverb
    std::atomic<float>* reverbSizeParam    = nullptr;
    std::atomic<float>* reverbDampingParam = nullptr;
    std::atomic<float>* reverbMixParam     = nullptr;

    // Voice mode / glide / warp
    std::atomic<float>* voiceModeParam  = nullptr;
    std::atomic<float>* glideTimeParam  = nullptr;
    std::atomic<float>* warpModeParam   = nullptr;
    std::atomic<float>* warpAmountParam = nullptr;

    // ── Mono/Legato state ─────────────────────────────────────────────────────
    std::vector<int> monoNoteStack;
    int              currentMonoNote = -1;

    // ── LFO engine ────────────────────────────────────────────────────────────
    // Control-rate chunk. 32 samples puts the modulation Nyquist at 689 Hz even
    // at 44.1 kHz, which is two decades above the 20 Hz the rate knob allows.
    static constexpr int kControlBlockSize = 32;

    // The scope is decimated to a fixed interval rather than to the chunk, so
    // its time span stays the same regardless of buffer size and control rate.
    static constexpr int kLfoVisInterval = 512;

    double lfoPhase       = 0.0;   // double: the accumulator runs for hours
    int    lfoVisCounter  = 0;
    double currentSR      = 44100.0;

    // ── FX chain ──────────────────────────────────────────────────────────────
    juce::dsp::Chorus<float>   chorus;
    juce::dsp::Phaser<float>   phaser;

    // Stereo delay lines (max 2 s)
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>
        delayLineL, delayLineR;
    float delayFeedbackState[2] { 0.0f, 0.0f };

    // Oversampling (2×) wraps the saturation wave-shaper
    juce::dsp::Oversampling<float> oversampler {
        2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false };

    // Pre-allocated dry buffer for wet/dry blend
    juce::AudioBuffer<float> dryBuffer;

    // Post-processing EQ — one filter per channel per band (3 bands × 2 channels)
    juce::dsp::IIR::Filter<float> eqLowL,  eqLowR;
    juce::dsp::IIR::Filter<float> eqMidL,  eqMidR;
    juce::dsp::IIR::Filter<float> eqHighL, eqHighR;

    // Reverb
    juce::dsp::Reverb reverb;

    void processDelay (juce::AudioBuffer<float>&, int numSamples);

    // Sequencer engine (audio thread only)
    std::atomic<float>* seqBPMParam      = nullptr;
    std::atomic<float>* seqNumStepsParam = nullptr;
    double seqPhase        = 0.0;
    int    seqStep         = -1;
    int    seqNoteOn       = -1;
    bool   seqWasPlaying   = false;

    // Trigger-mode state (audio thread only)
    int    seqTrigStep      = -1;
    int    seqTrigStepNote  = -1;
    bool   seqWasTrigMode   = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Synth1_0AudioProcessor)
};
