#pragma once
#include <JuceHeader.h>
#include <mutex>

// Wavetable oscillator with linear cross-table morphing.
// wavePosition 0=Sine 1=Saw 2=Square 3=Triangle; fractional values interpolate.
// Tables are built once (std::call_once) and shared across all instances.
class WavetableOscillator
{
public:
    static constexpr int kTableSize    = 2048;
    static constexpr int kNumWaveforms = 4;
    static constexpr int kNumHarmonics = 64;

    WavetableOscillator() { std::call_once (sInitFlag, buildTablesStatic); }

    void prepare (double newSampleRate) noexcept
    {
        sampleRate   = newSampleRate;
        phaseDelta   = 0.0f;
        currentPhase = 0.0f;
    }

    void reset() noexcept { currentPhase = 0.0f; }

    void setFrequency (float freqHz) noexcept
    {
        phaseDelta = static_cast<float> (freqHz / sampleRate);
    }

    // pos in [0, 3]: 0=Sine, 1=Saw, 2=Square, 3=Triangle; fractional morphs between adjacent tables.
    // UI access — safe to call from the message thread (tables are const after init)
    static const float* getTable (int idx) noexcept { return sTables[idx]; }
    static int          getTableSize() noexcept     { return kTableSize; }

    void setWavePosition (float pos) noexcept
    {
        wavePosition = juce::jlimit (0.0f, static_cast<float> (kNumWaveforms - 1), pos);
    }

    void renderBlock (float* output, int numSamples) noexcept
    {
        const int   tIdxA = static_cast<int> (wavePosition);
        const int   tIdxB = juce::jmin (tIdxA + 1, kNumWaveforms - 1);
        const float blend = wavePosition - static_cast<float> (tIdxA);
        const float* tA   = sTables[tIdxA];
        const float* tB   = sTables[tIdxB];
        const float  tSz  = static_cast<float> (kTableSize);

        for (int i = 0; i < numSamples; ++i)
        {
            const float pos  = currentPhase * tSz;
            const int   idx  = static_cast<int> (pos);
            const float frac = pos - static_cast<float> (idx);

            const float sA = tA[idx] + frac * (tA[idx + 1] - tA[idx]);
            const float sB = tB[idx] + frac * (tB[idx + 1] - tB[idx]);
            output[i]      = sA + blend * (sB - sA);

            currentPhase += phaseDelta;
            if (currentPhase >= 1.0f) currentPhase -= 1.0f;
        }
    }

private:
    alignas(32) inline static float sTables[kNumWaveforms][kTableSize + 1] {};
    inline static std::once_flag sInitFlag;

    static void buildTablesStatic()
    {
        constexpr float twoPi = juce::MathConstants<float>::twoPi;
        constexpr int   N     = kTableSize;

        for (int i = 0; i < N; ++i)
            sTables[0][i] = std::sin (twoPi * static_cast<float>(i) / N);
        sTables[0][N] = sTables[0][0];

        buildAdditive (1, [](int k, float angle) -> float {
            const float sign = (k % 2 == 1) ? 1.0f : -1.0f;
            return sign * (2.0f / juce::MathConstants<float>::pi) * std::sin (angle) / static_cast<float>(k);
        });
        buildAdditive (2, [](int k, float angle) -> float {
            if (k % 2 == 0) return 0.0f;
            return (4.0f / juce::MathConstants<float>::pi) * std::sin (angle) / static_cast<float>(k);
        });
        buildAdditive (3, [](int k, float angle) -> float {
            if (k % 2 == 0) return 0.0f;
            const float pi2  = juce::MathConstants<float>::pi * juce::MathConstants<float>::pi;
            const float sign = (((k - 1) / 2) % 2 == 0) ? 1.0f : -1.0f;
            return sign * (8.0f / pi2) * std::sin (angle) / (static_cast<float>(k) * static_cast<float>(k));
        });
    }

    template<typename HarmonicFn>
    static void buildAdditive (int waveIdx, HarmonicFn fn)
    {
        constexpr float twoPi = juce::MathConstants<float>::twoPi;
        constexpr int   N     = kTableSize;
        float* t = sTables[waveIdx];
        std::fill (t, t + N + 1, 0.0f);
        for (int k = 1; k <= kNumHarmonics; ++k)
            for (int i = 0; i < N; ++i)
                t[i] += fn (k, twoPi * static_cast<float>(k) * static_cast<float>(i) / N);
        float peak = 0.0f;
        for (int i = 0; i < N; ++i) peak = std::max (peak, std::abs (t[i]));
        if (peak > 0.0f)
            for (int i = 0; i < N; ++i) t[i] /= peak;
        t[N] = t[0];
    }

    float wavePosition = 0.0f;
    float currentPhase = 0.0f;
    float phaseDelta   = 0.0f;
    double sampleRate  = 44100.0;
};
