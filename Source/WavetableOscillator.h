#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include <cmath>
#include <mutex>
#include <vector>

/**
    Band-limited wavetable oscillator with mip-mapped tables.

    ---------------------------------------------------------------------------
    The problem this replaces
    ---------------------------------------------------------------------------
    The previous version built one table per waveform containing a fixed 64
    harmonics, and used it at every pitch. A table holding harmonics up to
    64*f0 is only alias-free while

        64 * f0 < fs/2      =>      f0 < fs/128

    which at 44.1 kHz is f0 < 344.5 Hz, roughly F4. Above that note the upper
    partials of the table exceed Nyquist and fold back as inharmonic tones that
    move the *wrong way* as the player goes up the keyboard. Since more than
    half the MIDI range sits above F4, the oscillator was aliasing over most of
    its useful span.

    ---------------------------------------------------------------------------
    The fix
    ---------------------------------------------------------------------------
    Mip-mapping: a bank of tables spaced every half octave, each holding only
    the harmonics that still fit under Nyquist at the TOP of its band,

        H(L) = floor( (fs/2) / (f_base * 2^((L+1)/M)) )   with M bands/octave

    so every fundamental inside the band is safe, not just the lowest one.

    Taking the budget from the top of the band is what makes it safe, and it is
    also what costs brightness: at A4 the ideal budget is floor(22050/440) = 50
    harmonics, and a full-octave bank would only allow 34. Halving the band
    spacing recovers most of that - 48 of the ideal 50 - for twice the tables
    and a build that still takes only tens of milliseconds. That is the whole
    tradeoff, and kMipsPerOctave is the knob for it.

    Level selection is continuous: the fractional band position crossfades
    between adjacent levels, because a hard switch changes the harmonic count
    by a step that is plainly audible during a glide or a slow pitch LFO.

    Interpolation moved from linear to 4-point cubic Hermite (Catmull-Rom).
    Linear interpolation of a 2048-point table is a triangular kernel whose
    frequency response leaks badly into the stopband; it acts as a gentle
    lowpass on the signal plus a noise floor that rises with playback rate.
    Hermite costs a handful of extra multiply-adds and drops that floor
    substantially.

    ---------------------------------------------------------------------------
    Two limits that Nyquist alone does not explain
    ---------------------------------------------------------------------------
    A 2048-point table can *represent* 1024 harmonics, but no practical
    interpolator can *read* them cleanly. Reading a table at a fractional rate
    produces images at k*(N*f0) +/- n*f0, and the interpolation kernel is what
    suppresses them. Content sitting near the table's own Nyquist has only a
    couple of samples per cycle, where the kernel has almost no stopband left,
    so those images come back as a noise floor.

    Measured: at 55 Hz an uncapped budget of 389 harmonics put images at
    -75.5 dBc - worse than the 64-harmonic version it replaced, despite being
    far more correct spectrally. The offenders sat exactly at
    bin(N*f0) - H*bin(f0), which identifies them as table images beyond doubt.
    Capping the budget at kTableSize/8 keeps eight table samples per cycle of
    the topmost harmonic and moves that floor to -77.3 dBc. Honest accounting:
    that is 1.8 dB, not a fix. The real remedy is a longer table at the low
    mips, since the image floor is set by table length against harmonic count,
    not by the cap alone. The cap is kept because it makes the invariant
    explicit and costs nothing audible - it only binds below fs/2/256 = 86 Hz,
    where the harmonics it discards are already under -48 dB.

    The phase accumulator is double, not float. Accumulating f0/fs in float32
    lets rounding error random-walk, which smears each harmonic into its
    immediate neighbours; with a double accumulator the energy within +/-2 bins
    of the harmonic grid sits at -164 dBc. Two double operations per sample,
    against four Hermite evaluations already there.

    setFrequency also takes a double. A float32 Hz value lands within about
    1e-4 Hz of the target, which is inaudible as pitch but leaves the tone very
    slightly non-periodic - enough that a rectangular-window spectrum shows
    leakage skirts at -86 dBc. That is a measurement-grade concern rather than
    a musical one, but the caller already holds the precise value, so there is
    nothing to gain by throwing it away.

    ---------------------------------------------------------------------------
    Known limitation
    ---------------------------------------------------------------------------
    Sync and Bend are phase distortion: they remap the phase before the table
    read, which re-creates the slope and value discontinuities the band-limited
    table was built to avoid. Those two alias even with perfect tables, and
    fixing them properly needs oversampling around the warp stage. Left as is,
    and stated rather than hidden.

    PWM is the exception, and only since it stopped being a phase remap. It is
    now x(p) - x(p + w), two reads of the same table a duty cycle apart, which
    is a sum of the harmonics already in the table and therefore band-limited.

    Cost note: with kBlendMips enabled a morphing oscillator performs four
    Hermite evaluations per sample (two waveforms x two mip levels). Setting
    kBlendMips to false halves that, at the cost of an audible step at band
    boundaries - the tradeoff the accompanying measurements quantify. PWM reads
    the table twice, so it doubles whichever of those two figures applies.
*/
class WavetableOscillator
{
public:
    static constexpr int  kTableSize     = 2048;
    static constexpr int  kNumWaveforms  = 4;
    static constexpr int  kMipsPerOctave = 2;    // half-octave bands
    static constexpr int  kNumMips       = 21;   // 20 Hz .. 20.5 kHz
    static constexpr int  kGuard         = 4;    // 1 before + 3 after, for Hermite
    static constexpr bool kBlendMips     = true;

    // Keep at least 8 table samples per cycle of the highest harmonic, so the
    // interpolator still has stopband left to suppress table images with.
    static constexpr int kMinSamplesPerCycle = 8;
    static constexpr int kMaxHarmonics       = kTableSize / kMinSamplesPerCycle;

    static constexpr double kMipBaseHz = 20.0;   // bottom of mip level 0

    WavetableOscillator() = default;

    void prepare (double newSampleRate)
    {
        sampleRate   = newSampleRate;
        phaseDelta   = 0.0;
        currentPhase = 0.0;
        mipPosition  = 0.0f;
        ensureTables (newSampleRate);
    }

    void reset() noexcept { currentPhase = 0.0; }

    /** Start from a given phase in [0, 1).

        Unison stacks need this: starting every oscillator at 0 makes them one
        coherent signal for the first cycles, which defeats the 1/sqrt(N)
        normalisation the caller applies.
    */
    void resetPhase (double p) noexcept { currentPhase = p - std::floor (p); }

    /** @param freqHz  taken as double: a float32 Hz value is musically exact
                       but quantises f0/fs enough to be visible in a spectrum,
                       and the caller already has the precise value.
    */
    void setFrequency (double freqHz) noexcept
    {
        const double f = juce::jmax (1.0e-3, freqHz);
        phaseDelta = f / sampleRate;

        // Continuous band index above the mip base.
        const float bands = static_cast<float> (std::log2 (f / kMipBaseHz)
                                                * kMipsPerOctave);
        mipPosition = juce::jlimit (0.0f, static_cast<float> (kNumMips - 1), bands);
    }

    void setWavePosition (float pos) noexcept
    {
        wavePosition = juce::jlimit (0.0f, static_cast<float> (kNumWaveforms - 1), pos);
    }

    // warpMode: 0=None, 1=Sync, 2=Bend, 3=PWM
    void setWarpMode   (int m)   noexcept { warpMode   = m; }
    void setWarpAmount (float a) noexcept { warpAmount = juce::jlimit (0.0f, 1.0f, a); }

    /** Richest table for a waveform - what the UI should draw.

        The editor can be constructed before prepareToPlay runs, so this
        guarantees the tables exist rather than handing back silence.
    */
    static const float* getTable (int waveIdx)
    {
        if (sBuiltForSampleRate == 0.0)
            ensureTables (44100.0);
        return tableData (juce::jlimit (0, kNumWaveforms - 1, waveIdx), 0);
    }

    static int getTableSize() noexcept { return kTableSize; }

    /** Harmonic count held by a mip level. Used by the analysis tooling. */
    static int getHarmonicCount (int mip) noexcept
    {
        return sHarmonics[juce::jlimit (0, kNumMips - 1, mip)];
    }

    void renderBlock (float* output, int numSamples) noexcept
    {
        const int   waveA     = static_cast<int> (wavePosition);
        const int   waveB     = juce::jmin (waveA + 1, kNumWaveforms - 1);
        const float waveBlend = wavePosition - static_cast<float> (waveA);

        const int   mipA     = static_cast<int> (mipPosition);
        const int   mipB     = juce::jmin (mipA + 1, kNumMips - 1);
        const float mipBlend = mipPosition - static_cast<float> (mipA);

        const float* aLo = tableData (waveA, mipA);
        const float* bLo = tableData (waveB, mipA);
        const float* aHi = tableData (waveA, mipB);
        const float* bHi = tableData (waveB, mipB);

        const bool  blendMips = kBlendMips && (mipB != mipA) && (mipBlend > 1.0e-4f);
        const double tSz      = static_cast<double> (kTableSize);

        // One table read, morph and mip blending included.
        //
        // Phase stays in double right up to the table index. Casting it to
        // float first quantises the read position to roughly 1e-4 of a table
        // sample, and because that error is a deterministic function of phase
        // it lands as discrete spurs beside every harmonic rather than as
        // broadband noise - measured at -86 dBc before that change.
        auto readAt = [&] (double phase01) noexcept -> float
        {
            const double pos  = phase01 * tSz;
            const int    idx  = juce::jlimit (0, kTableSize - 1, static_cast<int> (pos));
            const float  frac = static_cast<float> (pos - static_cast<double> (idx));

            float s = lerp (hermite (aLo + idx, frac),
                            hermite (bLo + idx, frac), waveBlend);

            if (blendMips)
            {
                const float hi = lerp (hermite (aHi + idx, frac),
                                       hermite (bHi + idx, frac), waveBlend);
                s = lerp (s, hi, mipBlend);
            }
            return s;
        };

        // PWM is not a phase remap and cannot be expressed as one, which is how
        // it ended up a copy of Bend: two branches of applyWarp with the same
        // arithmetic under different variable names. A pulse is the difference
        // of two reads of the same table a duty cycle apart, x(p) - x(p + w).
        //
        // That difference is a sum of the table's own harmonics with modified
        // amplitudes and phases, so unlike Sync and Bend it introduces nothing
        // above the mip's budget: PWM is the one warp mode that does not alias.
        // Tables carry no DC term, so neither does the difference. The 0.5
        // keeps the result inside unity: two unit sawtooths a distance w apart
        // differ by at most 2 - 2w, which reaches 1.9 at the narrow end.
        const bool   isPWM  = (warpMode == 3);
        const double pulseW = 0.05 + warpAmount * 0.9;

        for (int i = 0; i < numSamples; ++i)
        {
            float s;

            if (isPWM)
            {
                double p2 = currentPhase + pulseW;
                p2 -= std::floor (p2);
                s = 0.5f * (readAt (currentPhase) - readAt (p2));
            }
            else
            {
                s = readAt (applyWarp (currentPhase));
            }

            output[i] = s;

            // Accumulated in double: a float32 increment quantises f0/fs
            // enough to smear every harmonic across neighbouring bins.
            currentPhase += phaseDelta;
            if (currentPhase >= 1.0) currentPhase -= 1.0;
        }
    }

private:
    //==========================================================================
    //  Interpolation
    //==========================================================================
    static inline float lerp (float a, float b, float t) noexcept
    {
        return a + t * (b - a);
    }

    /** 4-point, 3rd-order Hermite (Catmull-Rom).

        @param p  points at the guard offset, so p[0..3] are y[-1] y[0] y[1] y[2]
        @param t  fractional position between y[0] and y[1]
    */
    static inline float hermite (const float* p, float t) noexcept
    {
        const float y0 = p[0], y1 = p[1], y2 = p[2], y3 = p[3];
        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + c0;
    }

    double applyWarp (double phase) const noexcept
    {
        switch (warpMode)
        {
            case 1: // Sync - multiply phase by ratio, fold back to [0,1)
            {
                const double ratio = 1.0 + warpAmount * 7.0;
                const double p     = phase * ratio;
                return p - std::floor (p);
            }
            case 2: // Bend - non-linear phase redistribution
            {
                const double k = 0.05 + warpAmount * 0.9;
                return (phase < k) ? phase / (2.0 * k)
                                   : 0.5 + (phase - k) / (2.0 * (1.0 - k));
            }
            // case 3 (PWM) is not a phase remap and is handled in renderBlock.
            // It used to live here as a character-for-character copy of Bend,
            // so the UI offered four warp modes over three implementations.
            default: return phase;
        }
    }

    //==========================================================================
    //  Shared table storage
    //==========================================================================
    // Laid out [waveform][mip][kTableSize + kGuard]. One guard sample sits
    // before each table and three after, so a Hermite read at any legal index
    // touches only valid memory without a wrap test in the inner loop.
    static constexpr int kStride = kTableSize + kGuard;

    alignas(32) inline static float sTables[kNumWaveforms * kNumMips * kStride] {};
    inline static int    sHarmonics[kNumMips] {};
    inline static double sBuiltForSampleRate = 0.0;
    inline static std::mutex sBuildMutex;

    static const float* tableData (int wave, int mip) noexcept
    {
        return sTables + ((wave * kNumMips) + mip) * kStride + 1;
    }

    static float* tableDataMutable (int wave, int mip) noexcept
    {
        return sTables + ((wave * kNumMips) + mip) * kStride + 1;
    }

    /** Sine lookup used only while building tables.

        Every partial is sampled on multiples of the same 2048-point grid, so
        sin(2*pi*k*i/N) is just sSineLut[(k*i) mod N]. That turns the build
        from millions of libm calls into millions of array reads, which is what
        keeps it at tens of milliseconds - worth caring about because the build
        runs inside prepareToPlay, where a stall shows up as a load-time hitch.
    */
    inline static double sSineLut[kTableSize] {};
    inline static bool   sLutReady = false;

    static void ensureLut() noexcept
    {
        if (sLutReady) return;
        constexpr double twoPi = juce::MathConstants<double>::twoPi;
        for (int i = 0; i < kTableSize; ++i)
            sSineLut[i] = std::sin (twoPi * i / kTableSize);
        sLutReady = true;
    }

    /** Build every table for this sample rate, once. */
    static void ensureTables (double fs)
    {
        std::lock_guard<std::mutex> lock (sBuildMutex);
        if (sBuiltForSampleRate == fs)
            return;

        ensureLut();
        const double nyquist = fs * 0.5;

        for (int mip = 0; mip < kNumMips; ++mip)
        {
            // The TOP of the band sets the harmonic budget, so every note
            // inside the band is covered, not just the lowest one.
            const double topHz = kMipBaseHz
                               * std::pow (2.0, static_cast<double> (mip + 1)
                                                / kMipsPerOctave);
            const int maxH = static_cast<int> (std::floor (nyquist / topHz));

            // Two ceilings, for two different reasons: Nyquist of the output,
            // and how much the interpolator can actually read back out of the
            // table without dragging images in behind it.
            sHarmonics[mip] = juce::jlimit (1, kMaxHarmonics, maxH);

            buildSine     (tableDataMutable (0, mip));
            buildSaw      (tableDataMutable (1, mip), sHarmonics[mip]);
            buildSquare   (tableDataMutable (2, mip), sHarmonics[mip]);
            buildTriangle (tableDataMutable (3, mip), sHarmonics[mip]);
        }

        sBuiltForSampleRate = fs;
    }

    //==========================================================================
    //  Additive table construction
    //==========================================================================
    // Fourier series, truncated to the mip's harmonic budget:
    //   saw       sum_k    (-1)^(k+1) (2/pi)   sin(k x) / k
    //   square    sum_odd             (4/pi)   sin(k x) / k
    //   triangle  sum_odd  (-1)^m     (8/pi^2) sin(k x) / k^2
    static void buildSine (float* t) noexcept
    {
        for (int i = 0; i < kTableSize; ++i)
            t[i] = static_cast<float> (sSineLut[i]);
        closeTable (t);
    }

    static void buildSaw (float* t, int harmonics)
    {
        accumulate (t, harmonics, [] (int k) -> double
        {
            const double sign = (k % 2 == 1) ? 1.0 : -1.0;
            return sign * (2.0 / juce::MathConstants<double>::pi) / k;
        });
    }

    static void buildSquare (float* t, int harmonics)
    {
        accumulate (t, harmonics, [] (int k) -> double
        {
            if (k % 2 == 0) return 0.0;
            return (4.0 / juce::MathConstants<double>::pi) / k;
        });
    }

    static void buildTriangle (float* t, int harmonics)
    {
        accumulate (t, harmonics, [] (int k) -> double
        {
            if (k % 2 == 0) return 0.0;
            const double pi2  = juce::MathConstants<double>::pi
                              * juce::MathConstants<double>::pi;
            const double sign = (((k - 1) / 2) % 2 == 0) ? 1.0 : -1.0;
            return sign * (8.0 / pi2) / (static_cast<double> (k) * k);
        });
    }

    /** Sum a harmonic series, then normalise to unit peak. */
    template <typename AmplitudeFn>
    static void accumulate (float* t, int harmonics, AmplitudeFn amplitude)
    {
        std::vector<double> acc (static_cast<size_t> (kTableSize), 0.0);

        for (int k = 1; k <= harmonics; ++k)
        {
            const double a = amplitude (k);
            if (a == 0.0) continue;

            // k <= kTableSize/2, so one conditional subtract always suffices
            // to fold the index back into range.
            int idx = 0;
            for (int i = 0; i < kTableSize; ++i)
            {
                acc[static_cast<size_t> (i)] += a * sSineLut[idx];
                idx += k;
                if (idx >= kTableSize) idx -= kTableSize;
            }
        }

        double peak = 0.0;
        for (double v : acc) peak = std::max (peak, std::abs (v));
        const double norm = (peak > 0.0) ? 1.0 / peak : 1.0;

        for (int i = 0; i < kTableSize; ++i)
            t[i] = static_cast<float> (acc[static_cast<size_t> (i)] * norm);

        closeTable (t);
    }

    /** Fill the wrap-around guard samples surrounding a table. */
    static void closeTable (float* t) noexcept
    {
        t[-1]             = t[kTableSize - 1];
        t[kTableSize]     = t[0];
        t[kTableSize + 1] = t[1];
        t[kTableSize + 2] = t[2];
    }

    float  wavePosition = 0.0f;
    float  mipPosition  = 0.0f;
    double currentPhase = 0.0;
    double phaseDelta   = 0.0;
    double sampleRate   = 44100.0;
    int    warpMode     = 0;
    float  warpAmount   = 0.0f;
};
