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

private:
    // ── Synth ─────────────────────────────────────────────────────────────────
    juce::Synthesiser synth;

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
    float  lfoPhase       = 0.0f;
    double currentSR      = 44100.0;

    // ── FX chain ──────────────────────────────────────────────────────────────
    juce::dsp::Chorus<float>   chorus;

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Synth1_0AudioProcessor)
};
