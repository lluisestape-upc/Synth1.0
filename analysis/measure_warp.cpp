//==============================================================================
//  Offline measurement harness for the oscillator's warp modes.
//
//  Renders the shipping WavetableOscillator at a sweep of fundamentals through
//  each of the four warp modes, writing raw float32 buffers that
//  measure_modulation.py turns into alias-to-signal figures.
//
//  Build (from the Synth1.0 repo root, in a VS developer prompt):
//      cl /std:c++17 /O2 /EHsc /I analysis\juce_stub /I Source ^
//         analysis\measure_warp.cpp /Fe:analysis\measure_warp.exe
//
//  Two questions this answers:
//
//  1. Bend (mode 2) and PWM (mode 3) used to be the same expression under two
//     variable names, so the two rendered buffers were bit-identical. PWM is
//     now x(p) - x(p + w), a two-read difference that cannot be written as a
//     phase remap. The per-sample difference between the modes is reported.
//
//  2. Sync and Bend remap phase, which re-creates the discontinuities the
//     band-limited tables exist to avoid, so both still alias. The two-read
//     PWM is a sum of harmonics already present in the table, so it should
//     not. That is the claim being measured, not asserted.
//
//  As in measure_oscillator.cpp, every fundamental is snapped to an exact FFT
//  bin, so no window is needed and any energy off the harmonic grid is
//  aliasing rather than leakage.
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
// 0.75, not 0.5: Bend's breakpoint is k = 0.05 + amount * 0.9, so at amount
// 0.5 the map is k = 0.5 and both of its branches reduce to the identity. The
// knob does nothing at its own midpoint, which makes it a useless place to
// measure a phase distortion from.
static constexpr float  kWarpAmount = 0.75f;

static const char* kModeName[] = { "none", "sync", "bend", "pwm" };

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

    const double targets[] = { 110.0, 440.0, 1320.0 };
    const int numTargets = static_cast<int> (sizeof (targets) / sizeof (targets[0]));

    FILE* manifest = std::fopen ((outDir + "/warp_manifest.csv").c_str(), "w");
    if (manifest == nullptr)
    {
        std::fprintf (stderr,
                      "cannot write manifest - does %s exist?\n", outDir.c_str());
        return 1;
    }
    std::fprintf (manifest, "mode,bin,exact_hz,amount,samples,file\n");

    std::vector<float> buffer (kFFTSize);

    for (int t = 0; t < numTargets; ++t)
    {
        const int    bin   = static_cast<int> (std::lround (targets[t] * kFFTSize / kSampleRate));
        const double exact = bin * kSampleRate / kFFTSize;

        for (int mode = 0; mode < 4; ++mode)
        {
            WavetableOscillator osc;
            osc.prepare (kSampleRate);
            osc.setWavePosition (1.0f);      // pure sawtooth, no morph blending
            osc.setWarpMode     (mode);
            osc.setWarpAmount   (kWarpAmount);
            osc.setFrequency    (exact);
            osc.reset();

            for (int i = 0; i < kFFTSize; i += kBlock)
                osc.renderBlock (buffer.data() + i, kBlock);

            const std::string name = std::string ("warp_") + kModeName[mode]
                                   + "_" + std::to_string (bin) + ".f32";
            writeRaw (outDir + "/" + name, buffer);
            std::fprintf (manifest, "%s,%d,%.6f,%.3f,%d,%s\n",
                          kModeName[mode], bin, exact, kWarpAmount,
                          kFFTSize, name.c_str());
        }

        std::printf ("rendered %8.2f Hz (bin %5d) x 4 warp modes\n", exact, bin);
    }

    std::fclose (manifest);
    std::printf ("\nwrote %d buffers to %s\n", numTargets * 4, outDir.c_str());
    return 0;
}
