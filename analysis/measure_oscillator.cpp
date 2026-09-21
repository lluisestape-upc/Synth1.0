//==============================================================================
//  Offline measurement harness for WavetableOscillator.
//
//  Renders the shipping oscillator and the one it replaced at a sweep of
//  fundamentals, writing raw float32 buffers that plot_oscillator.py turns into
//  alias-to-signal figures.
//
//  Build (from the Synth1.0 repo root, in a VS developer prompt):
//      cl /std:c++17 /O2 /EHsc /I analysis\juce_stub /I Source ^
//         analysis\measure_oscillator.cpp /Fe:analysis\measure_oscillator.exe
//
//  Every fundamental is snapped to an exact FFT bin so that all true harmonics
//  land on bins too. No window is then needed and there is no spectral leakage
//  to be mistaken for aliasing - which is the entire point of the measurement.
//==============================================================================
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "WavetableOscillator.h"

static constexpr double kSampleRate = 44100.0;
static constexpr int    kFFTSize    = 1 << 16;
static constexpr int    kBlock      = 512;

//==============================================================================
//  The previous implementation, reproduced verbatim for comparison:
//  one table per waveform, a fixed 64 harmonics, linear interpolation.
//==============================================================================
class LegacyOscillator
{
public:
    static constexpr int kTableSize    = 2048;
    static constexpr int kNumHarmonics = 64;

    void prepare (double fs) noexcept
    {
        sampleRate   = fs;
        currentPhase = 0.0f;
        buildSaw();
    }

    void setFrequency (double hz) noexcept
    {
        phaseDelta = static_cast<float> (hz / sampleRate);
    }

    void renderBlock (float* out, int n) noexcept
    {
        const float tSz = static_cast<float> (kTableSize);
        for (int i = 0; i < n; ++i)
        {
            const float pos  = currentPhase * tSz;
            const int   idx  = static_cast<int> (pos);
            const float frac = pos - static_cast<float> (idx);
            out[i] = table[idx] + frac * (table[idx + 1] - table[idx]);

            currentPhase += phaseDelta;
            if (currentPhase >= 1.0f) currentPhase -= 1.0f;
        }
    }

private:
    void buildSaw() noexcept
    {
        constexpr double twoPi = 6.283185307179586477;
        std::vector<double> acc (kTableSize, 0.0);
        for (int k = 1; k <= kNumHarmonics; ++k)
        {
            const double sign = (k % 2 == 1) ? 1.0 : -1.0;
            const double a    = sign * (2.0 / 3.141592653589793) / k;
            for (int i = 0; i < kTableSize; ++i)
                acc[i] += a * std::sin (twoPi * k * i / kTableSize);
        }
        double peak = 0.0;
        for (double v : acc) peak = std::max (peak, std::abs (v));
        for (int i = 0; i < kTableSize; ++i)
            table[i] = static_cast<float> (acc[i] / (peak > 0.0 ? peak : 1.0));
        table[kTableSize] = table[0];
    }

    float  table[kTableSize + 1] {};
    float  currentPhase = 0.0f;
    float  phaseDelta   = 0.0f;
    double sampleRate   = 44100.0;
};

//==============================================================================
static void writeRaw (const std::string& path, const std::vector<float>& data)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (f == nullptr)
    {
        std::fprintf (stderr, "cannot open %s\n", path.c_str());
        std::exit (1);
    }
    std::fwrite (data.data(), sizeof (float), data.size(), f);
    std::fclose (f);
}

int main (int argc, char** argv)
{
    const std::string outDir = (argc > 1) ? argv[1] : "analysis/data";

    // Fundamentals spanning the instrument's usable range, snapped to bins.
    const double targets[] = { 55.0, 110.0, 220.0, 330.0, 440.0, 660.0,
                               880.0, 1320.0, 2000.0, 3000.0, 4186.0 };
    const int numTargets = static_cast<int> (sizeof (targets) / sizeof (targets[0]));

    FILE* manifest = std::fopen ((outDir + "/manifest.csv").c_str(), "w");
    if (manifest == nullptr)
    {
        std::fprintf (stderr,
                      "cannot write manifest - does %s exist?\n", outDir.c_str());
        return 1;
    }
    std::fprintf (manifest, "config,bin,exact_hz,samples,file\n");

    // Report the harmonic budget of every mip level: this is the table the
    // write-up quotes.
    WavetableOscillator probe;
    probe.prepare (kSampleRate);
    std::printf ("mip | band top (Hz) | harmonics | highest partial (Hz)\n");
    std::printf ("----+---------------+-----------+---------------------\n");
    for (int m = 0; m < WavetableOscillator::kNumMips; ++m)
    {
        const double topHz = WavetableOscillator::kMipBaseHz
                           * std::pow (2.0, static_cast<double> (m + 1)
                                            / WavetableOscillator::kMipsPerOctave);
        const int h = WavetableOscillator::getHarmonicCount (m);
        std::printf ("%3d | %13.1f | %9d | %19.1f\n", m, topHz, h, h * topHz);
    }
    std::printf ("\n");

    std::vector<float> buffer (kFFTSize);

    for (int t = 0; t < numTargets; ++t)
    {
        const int    bin   = static_cast<int> (std::lround (targets[t] * kFFTSize / kSampleRate));
        const double exact = bin * kSampleRate / kFFTSize;

        // ---- shipping implementation -------------------------------------
        {
            WavetableOscillator osc;
            osc.prepare (kSampleRate);
            osc.setWavePosition (1.0f);   // pure sawtooth, no morph blending
            osc.setWarpMode (0);
            // Exact double: a float32 Hz here would sit ~1e-4 Hz off the bin,
            // and the resulting window leakage would swamp the aliasing floor
            // this harness exists to measure.
            osc.setFrequency (exact);
            osc.reset();
            for (int i = 0; i < kFFTSize; i += kBlock)
                osc.renderBlock (buffer.data() + i, kBlock);

            const std::string name = "mip_" + std::to_string (bin) + ".f32";
            writeRaw (outDir + "/" + name, buffer);
            std::fprintf (manifest, "mip,%d,%.6f,%d,%s\n",
                          bin, exact, kFFTSize, name.c_str());
        }

        // ---- the implementation it replaced ------------------------------
        {
            LegacyOscillator osc;
            osc.prepare (kSampleRate);
            osc.setFrequency (exact);
            for (int i = 0; i < kFFTSize; i += kBlock)
                osc.renderBlock (buffer.data() + i, kBlock);

            const std::string name = "legacy_" + std::to_string (bin) + ".f32";
            writeRaw (outDir + "/" + name, buffer);
            std::fprintf (manifest, "legacy,%d,%.6f,%d,%s\n",
                          bin, exact, kFFTSize, name.c_str());
        }

        std::printf ("rendered %8.2f Hz (bin %5d)\n", exact, bin);
    }

    std::fclose (manifest);
    std::printf ("\nwrote %d buffers to %s\n", numTargets * 2, outDir.c_str());
    return 0;
}
