"""Measure SYNTH/1's control-rate paths: LFO, glide, warp, unison, saturator.

Usage::

    python analysis/measure_modulation.py

Companion to ``plot_oscillator.py``. That one measures the oscillator, which is
audio-rate and lives in a header a stub can compile. Everything measured here
sits in ``SynthVoice.h`` and ``PluginProcessor.cpp``, tangled up with APVTS and
``juce::Synthesiser``, so these paths are **replicated from the source rather
than compiled from it**. Each replication is quoted beside the original in the
write-up; where a claim can be settled by reading the source instead of running
it (the two identical warp modes, the unison gain law) the arithmetic is given
too, so nothing here rests on the replication being faithful.

What is being measured
----------------------
1. The LFO advances once per ``processBlock``, so it is sampled at ``fs/N``.
   Above ``N = fs/(2*rate)`` the modulation folds and the knob stops meaning
   what it says.
2. Glide reads ``smoothedFreqHz.skip(numSamples)``: one pitch per block, so a
   portamento is a staircase whose step height is set by the host buffer size.
3. ``applyWarp`` cases 2 (Bend) and 3 (PWM) are the same expression.
4. Unison sums N coherent copies scaled by 1/sqrt(N), so at zero detune the
   voice count is a gain knob.
5. ``FastMathApproximations::tanh`` is documented for |x| <= 5; ``drive`` goes
   to 10.
"""
from __future__ import annotations

from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402

HERE = Path(__file__).resolve().parent
FIGS = HERE / "figures"
FIGS.mkdir(exist_ok=True)

FS = 44100.0
C_BAD, C_GOOD, C_REF = "#c1442e", "#2e6fc1", "#6b6b6b"
C_ALT = "#c18a2e"

# Must match Synth1_0AudioProcessor::kControlBlockSize.
KCONTROL = 32


# -----------------------------------------------------------------------------
#  1. The LFO is sampled once per block
# -----------------------------------------------------------------------------
def lfo_signal(rate_hz: float, block: int, seconds: float = 4.0) -> np.ndarray:
    """Replicates PluginProcessor::processBlock's LFO, float32 and all.

        lfoPhase += (lfoRate / currentSR) * N;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
        const float lfoVal = std::sin (lfoPhase * twoPi);

    Note the single conditional subtract: it only normalises the phase while
    the per-block increment stays below 1.
    """
    num_blocks = int(seconds * FS / block)
    phase = np.float32(0.0)
    inc = np.float32(rate_hz / FS) * np.float32(block)
    out = np.empty(num_blocks, dtype=np.float64)
    for b in range(num_blocks):
        phase = np.float32(phase + inc)
        if phase >= np.float32(1.0):
            phase = np.float32(phase - np.float32(1.0))
        out[b] = np.sin(float(phase) * 2.0 * np.pi)
    return out


def dominant_hz(block_values: np.ndarray, block: int) -> float:
    """Strongest non-DC component of a signal sampled at fs/block."""
    n = len(block_values)
    spec = np.abs(np.fft.rfft(block_values * np.hanning(n)))
    spec[0] = 0.0
    k = int(np.argmax(spec))
    return k * (FS / block) / n


def measure_lfo() -> None:
    """Before: one LFO step per processBlock. After: one per kControlBlockSize."""
    blocks = [32, 64, 128, 256, 512, 1024, 2048, 4096]
    rates = [1.0, 5.0, 20.0]

    print("\n=== LFO: set rate vs rate actually produced (Hz) ===")
    print(f"advancing once per processBlock (before), "
          f"vs once per {KCONTROL}-sample control chunk (after)\n")
    header = "  set | " + " | ".join(f"N={b:<5d}" for b in blocks) + " | after"
    print(header)
    print("-" * len(header))

    before = {}
    after = {}
    for rate in rates:
        before[rate] = [dominant_hz(lfo_signal(rate, b), b) for b in blocks]
        after[rate] = dominant_hz(lfo_signal(rate, KCONTROL), KCONTROL)
        print(f"{rate:5.1f} | " + " | ".join(f"{v:7.2f}" for v in before[rate])
              + f" | {after[rate]:5.2f}")

    print("\nfolding threshold N = fs/(2*rate), before the fix:")
    for rate in rates:
        print(f"  {rate:5.1f} Hz -> N = {FS / (2 * rate):8.0f} samples")
    print(f"\nafter the fix the control rate is fixed at fs/{KCONTROL} = "
          f"{FS / KCONTROL:.0f} Hz, so the ceiling is {FS / (2 * KCONTROL):.0f} Hz "
          f"at any buffer size")

    fig, axes = plt.subplots(1, 2, figsize=(12, 4.4))

    ax = axes[0]
    for rate, colour in zip(rates, (C_REF, C_GOOD, C_BAD)):
        ax.semilogx(blocks, before[rate], "o-", color=colour, lw=1.8, ms=5,
                    base=2, label=f"{rate:g} Hz knob, per processBlock")
        ax.axhline(rate, color=colour, lw=0.8, ls=":", alpha=0.6)
    ax.semilogx(blocks, [after[20.0]] * len(blocks), "s--", color="#2e8b57",
                lw=1.8, ms=5, base=2,
                label=f"20 Hz knob, per {KCONTROL}-sample chunk")
    ax.axvline(FS / 40.0, color=C_BAD, lw=1.0, ls="--")
    ax.annotate("N = $f_s/2f$ = 1102\nthe 20 Hz setting folded past here",
                xy=(FS / 40.0, 13), xytext=(110, 15.0), fontsize=8, color=C_BAD,
                arrowprops=dict(arrowstyle="->", color=C_BAD, lw=0.8))
    ax.set_xlabel("host buffer size (samples)")
    ax.set_ylabel("modulation frequency produced (Hz)")
    ax.set_title("A per-block LFO is sampled at $f_s/N$; a chunked one is not")
    ax.set_xticks(blocks)
    ax.set_xticklabels([str(b) for b in blocks])
    ax.grid(alpha=0.3)
    ax.legend(frameon=False, fontsize=8)

    ax = axes[1]
    for block, colour, tag in ((2048, C_BAD, "per processBlock, N = 2048"),
                               (KCONTROL, "#2e8b57",
                                f"per {KCONTROL}-sample chunk, any N")):
        vals = lfo_signal(20.0, block, seconds=1.0)
        t = np.arange(len(vals)) * block / FS
        out_hz = dominant_hz(lfo_signal(20.0, block), block)
        ax.step(t, vals, where="post", color=colour, lw=1.4,
                label=f"{tag} ({out_hz:.2f} Hz out)")
    t = np.linspace(0, 1.0, 4000)
    ax.plot(t, np.sin(2 * np.pi * 20.0 * t), color=C_REF, lw=0.6, alpha=0.5,
            label="20 Hz, what the knob says")
    ax.set_xlim(0, 0.5)
    ax.set_xlabel("time (s)")
    ax.set_ylabel("LFO value")
    ax.set_title("Rate knob at 20 Hz")
    ax.grid(alpha=0.3)
    ax.legend(frameon=False, fontsize=8, loc="lower right")

    fig.tight_layout()
    fig.savefig(FIGS / "lfo_block_rate.png", dpi=150)
    plt.close(fig)


# -----------------------------------------------------------------------------
#  2. Glide is a staircase with a host-dependent step
# -----------------------------------------------------------------------------
def glide_cents(glide_s: float, block: int, semitones: float = 12.0,
                seconds: float = 0.35):
    """Replicates SmoothedValue<float, Multiplicative> read once per block.

        const float glideHz = smoothedFreqHz.skip (numSamples);

    JUCE's multiplicative smoother stores a per-sample ratio
    (target/current)^(1/steps); skip(n) applies it n times and returns the
    value. The ramp rate is therefore correct; only its sampling is coarse.
    """
    steps = max(1, int(glide_s * FS))
    start, target = 261.63, 261.63 * 2.0 ** (semitones / 12.0)
    ratio = (target / start) ** (1.0 / steps)

    value, remaining = start, steps
    times, cents = [0.0], [0.0]
    for b in range(int(seconds * FS / block)):
        n = min(block, remaining)
        value *= ratio ** n
        remaining -= n
        times.append((b + 1) * block / FS)
        cents.append(1200.0 * np.log2(value / start))
    return np.array(times), np.array(cents)


def measure_glide() -> None:
    glide_s = 0.1
    print(f"\n=== Glide: one octave in {glide_s * 1000:.0f} ms ===")
    print(f"{'buffer':>7} | {'updates':>8} | {'cents/step':>11}")
    print("-" * 32)
    for b in (64, 128, 256, 512, 1024):
        updates = glide_s * FS / b
        print(f"{b:7d} | {updates:8.1f} | {1200.0 / updates:11.1f}")
    updates = glide_s * FS / KCONTROL
    print(f"{KCONTROL:7d} | {updates:8.1f} | {1200.0 / updates:11.1f}   <- after"
          f" the fix, at any buffer size")
    print("\ncents per step = 1200 * N / (glideTime * fs)")

    fig, ax = plt.subplots(figsize=(9, 4.4))
    for b, colour in zip((1024, 256, KCONTROL), (C_BAD, C_GOOD, "#2e8b57")):
        t, c = glide_cents(glide_s, b)
        tag = (f"per {KCONTROL}-sample chunk, any N" if b == KCONTROL
               else f"per processBlock, N = {b}")
        ax.step(t * 1000.0, c, where="post", color=colour, lw=1.6,
                label=f"{tag}  ({1200.0 / (glide_s * FS / b):.0f} cents/step)")
    ax.axhline(1200, color=C_REF, lw=0.8, ls=":")
    ax.set_xlim(0, 160)
    ax.set_xlabel("time since note-on (ms)")
    ax.set_ylabel("pitch above start (cents)")
    ax.set_title("Portamento was quantised to the host buffer, not to the glide time")
    ax.grid(alpha=0.3)
    ax.legend(frameon=False, fontsize=9, loc="lower right")
    fig.tight_layout()
    fig.savefig(FIGS / "glide_staircase.png", dpi=150)
    plt.close(fig)


# -----------------------------------------------------------------------------
#  3. Two of the four warp modes are one warp mode
# -----------------------------------------------------------------------------
def apply_warp(phase: np.ndarray, mode: int, amount: float) -> np.ndarray:
    """Replicates WavetableOscillator::applyWarp, case for case.

    Mode 3 is deliberately absent: PWM is no longer a phase remap. It is
    x(p) - x(p + w), two table reads a duty cycle apart, handled in
    renderBlock. That is why it is measured below from rendered audio rather
    than drawn as a curve.
    """
    if mode == 1:                                    # Sync
        ratio = 1.0 + amount * 7.0
        p = phase * ratio
        return p - np.floor(p)
    if mode == 2:                                    # Bend
        k = 0.05 + amount * 0.9
        return np.where(phase < k, phase / (2.0 * k),
                        0.5 + (phase - k) / (2.0 * (1.0 - k)))
    return phase


def alias_to_signal_db(x: np.ndarray, bin_index: int) -> float:
    """Off-harmonic energy over on-harmonic energy, in dB.

    Same measure as plot_oscillator.py: the fundamental sits on an exact FFT
    bin, so no window is needed and anything off the harmonic grid is aliasing.
    """
    spec = np.abs(np.fft.rfft(x)) ** 2
    spec[0] = 0.0
    mask = np.zeros_like(spec, dtype=bool)
    m = 1
    while m * bin_index < len(spec):
        mask[m * bin_index] = True
        m += 1
    return 10.0 * np.log10(max(spec[~mask].sum(), 1e-300)
                           / max(spec[mask].sum(), 1e-300))


def load_warp_buffers():
    """Buffers written by measure_warp.exe, keyed (mode, bin)."""
    import csv
    path = HERE / "data" / "warp_manifest.csv"
    if not path.exists():
        return None, None
    with open(path, newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    buffers = {}
    for row in rows:
        buffers[(row["mode"], int(row["bin"]))] = np.fromfile(
            HERE / "data" / row["file"], dtype=np.float32).astype(np.float64)
    return rows, buffers


def measure_warp() -> None:
    rows, buffers = load_warp_buffers()
    if rows is None:
        print("\n=== Warp ===")
        print("data/warp_manifest.csv not found - build and run measure_warp.exe:")
        print("  cl /std:c++17 /O2 /EHsc /I analysis\\juce_stub /I Source \\")
        print("     analysis\\measure_warp.cpp /Fe:analysis\\measure_warp.exe")
        print("  analysis\\measure_warp.exe analysis/data")
        return

    bins = sorted({int(r["bin"]) for r in rows})
    hz = {int(r["bin"]): float(r["exact_hz"]) for r in rows}
    amount = float(rows[0]["amount"])

    print(f"\n=== Warp at Amount = {amount:g}, rendered by the shipping header ===")
    print("alias-to-signal ratio (dB, lower is better)")
    print(f"{'f0 (Hz)':>9} | " + " | ".join(f"{m:>8}" for m in
                                            ("none", "sync", "bend", "pwm")))
    print("-" * 52)
    for b in bins:
        cells = [alias_to_signal_db(buffers[(m, b)], b)
                 for m in ("none", "sync", "bend", "pwm")]
        print(f"{hz[b]:9.1f} | " + " | ".join(f"{c:8.1f}" for c in cells))

    print("\nBend vs PWM, max |sample difference| (was exactly 0 when PWM was a"
          " copy of Bend):")
    for b in bins:
        d = np.max(np.abs(buffers[("bend", b)] - buffers[("pwm", b)]))
        print(f"  {hz[b]:9.1f} Hz | {d:.4f}")

    # -- figure: the phase remaps, and the two modes that used to coincide ----
    fig, axes = plt.subplots(1, 2, figsize=(12, 4.4))

    phase = np.linspace(0.0, 1.0, 20001, endpoint=False)
    ax = axes[0]
    ax.plot(phase, apply_warp(phase, 0, amount), color=C_REF, lw=1.2, label="None")
    ax.plot(phase, apply_warp(phase, 1, amount), color=C_ALT, lw=1.2, label="Sync")
    ax.plot(phase, apply_warp(phase, 2, amount), color=C_GOOD, lw=2.0, label="Bend")
    ax.set_xlabel("input phase")
    ax.set_ylabel("warped phase")
    ax.set_title(f"Phase remaps at Amount = {amount:g}")
    ax.grid(alpha=0.3)
    ax.legend(frameon=False, fontsize=9, loc="upper left")
    ax.text(0.97, 0.05, "PWM is not on this panel any more:\n"
                        "it is a two-read difference, not a curve",
            transform=ax.transAxes, fontsize=8, color=C_BAD,
            va="bottom", ha="right")

    b = bins[len(bins) // 2]
    cycle = int(round(FS / hz[b]))
    ax = axes[1]
    t = np.arange(2 * cycle) / FS * 1000.0
    ax.plot(t, buffers[("bend", b)][:2 * cycle], color=C_GOOD, lw=1.6, label="Bend")
    ax.plot(t, buffers[("pwm", b)][:2 * cycle], color=C_BAD, lw=1.6, label="PWM")
    ax.set_xlabel("time (ms)")
    ax.set_ylabel("output")
    ax.set_title(f"Two cycles at {hz[b]:.0f} Hz, rendered by the plugin's oscillator")
    ax.grid(alpha=0.3)
    ax.legend(frameon=False, fontsize=9, loc="upper right")

    fig.tight_layout()
    fig.savefig(FIGS / "warp_curves.png", dpi=150)
    plt.close(fig)


# -----------------------------------------------------------------------------
#  4. Unison at zero detune is a gain knob
# -----------------------------------------------------------------------------
def measure_unison(draws: int = 500, harmonics: int = 50) -> None:
    """From SynthVoice::renderNextBlock and startNote.

    normGain = 1/sqrt(N) is the law for *incoherent* sources. With detune = 0
    every oscillator renders the same signal, and startNote() used to reset them
    all to phase 0, so the sum was N coherent copies: N * (1/sqrt(N)) = sqrt(N).

    startNote now draws a random phase per oscillator, which is the condition
    1/sqrt(N) already assumed. Measured here on a 50-harmonic sawtooth, over
    random phase draws, as level relative to a single oscillator.
    """
    print("\n=== Unison, level relative to one oscillator (dB) ===")
    print(f"{'voices':>7} | {'phase 0 (before)':>17} | "
          f"{'random phase, RMS':>19} | {'random, peak':>13}")
    print("-" * 66)

    t = np.linspace(0.0, 1.0, 4096, endpoint=False)
    k = np.arange(1, harmonics + 1)
    amps = ((-1.0) ** (k + 1)) * (2.0 / np.pi) / k
    rng = np.random.default_rng(0)

    single = amps @ np.sin(2 * np.pi * np.outer(k, t))
    single_rms = np.sqrt(np.mean(single ** 2))

    for n in range(1, 9):
        rms, peak = [], []
        for _ in range(draws):
            phases = rng.random(n)
            mix = np.zeros_like(t)
            for p in phases:
                mix += amps @ np.sin(2 * np.pi * np.outer(k, t + p))
            mix /= np.sqrt(n)
            rms.append(np.sqrt(np.mean(mix ** 2)))
            peak.append(np.max(np.abs(mix)))
        print(f"{n:7d} | {20 * np.log10(np.sqrt(n)):17.2f} | "
              f"{20 * np.log10(np.mean(rms) / single_rms):19.2f} | "
              f"{20 * np.log10(np.mean(peak) / np.max(np.abs(single))):13.2f}")

    print("\nThe phase-0 column also applied to the first cycles of every note at")
    print("any detune setting, since all eight oscillators started aligned.")
    print("Evenly spaced phases (u/N) would be worse than either: summing N copies")
    print("at phases k/N cancels every harmonic that is not a multiple of N.")


# -----------------------------------------------------------------------------
#  5. The saturator past its documented range
# -----------------------------------------------------------------------------
def fast_tanh(x: np.ndarray) -> np.ndarray:
    """juce::dsp::FastMathApproximations::tanh, verbatim (Pade approximant)."""
    x2 = x * x
    num = x * (135135 + x2 * (17325 + x2 * (378 + x2)))
    den = 135135 + x2 * (62370 + x2 * (3150 + 28 * x2))
    return num / den


def measure_saturator() -> None:
    print("\n=== FastMathApproximations::tanh outside its documented |x| <= 5 ===")
    print(f"{'x':>6} | {'fast':>9} | {'tanh':>9} | {'error':>9} | {'dB over':>8}")
    print("-" * 52)
    for x in (1.0, 5.0, 7.5, 10.0, 15.0, 20.0):
        f, t = float(fast_tanh(np.array([x]))[0]), float(np.tanh(x))
        print(f"{x:6.1f} | {f:9.5f} | {t:9.5f} | {f - t:9.5f} | "
              f"{20 * np.log10(f):8.3f}")
    x = np.linspace(0, 25, 250001)
    first = x[fast_tanh(x) > 1.0][0]
    print(f"\nfirst x where the approximation exceeds unity: {first:.3f}")


def main() -> None:
    measure_lfo()
    measure_glide()
    measure_warp()
    measure_unison()
    measure_saturator()
    print(f"\nfigures -> {FIGS / 'lfo_block_rate.png'}")
    print(f"           {FIGS / 'glide_staircase.png'}")
    print(f"           {FIGS / 'warp_curves.png'}\n")


if __name__ == "__main__":
    main()
