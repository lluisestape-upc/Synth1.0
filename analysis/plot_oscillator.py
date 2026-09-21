"""Turn the C++ harness output into the oscillator figures.

Usage::

    cl /std:c++17 /O2 /EHsc /I analysis\\juce_stub /I Source \\
       analysis\\measure_oscillator.cpp /Fe:analysis\\measure_oscillator.exe
    analysis\\measure_oscillator.exe analysis/data
    python analysis/plot_oscillator.py

The buffers being analysed come from the shipping ``WavetableOscillator.h``
compiled as-is against a minimal JUCE stub, so these figures describe the code
that runs in the plugin rather than a Python model of it.

Method
------
Each fundamental was snapped to an exact FFT bin by the harness, so every true
harmonic lands exactly on a bin. No window is applied and none is needed: with
no leakage, any energy off the harmonic grid is aliasing, full stop. That makes
the alias-to-signal ratio a clean, assumption-free number.
"""
from __future__ import annotations

import csv
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402

HERE = Path(__file__).resolve().parent
DATA = HERE / "data"
FIGS = HERE / "figures"
FIGS.mkdir(exist_ok=True)

FS = 44100.0
C_BAD, C_GOOD, C_REF = "#c1442e", "#2e6fc1", "#6b6b6b"

LABEL = {"legacy": "64 harmonics + linear (previous)",
         "mip": "mip-mapped + Hermite (current)"}
COLOUR = {"legacy": C_BAD, "mip": C_GOOD}


def load_manifest() -> list[dict]:
    path = DATA / "manifest.csv"
    if not path.exists():
        raise SystemExit(f"{path} not found - run measure_oscillator.exe first")
    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def alias_to_signal_db(x: np.ndarray, bin_index: int) -> float:
    """Off-harmonic energy over on-harmonic energy, in dB."""
    spec = np.abs(np.fft.rfft(x)) ** 2
    spec[0] = 0.0
    mask = np.zeros_like(spec, dtype=bool)
    m = 1
    while m * bin_index < len(spec):
        mask[m * bin_index] = True
        m += 1
    return 10.0 * np.log10(max(spec[~mask].sum(), 1e-300)
                           / max(spec[mask].sum(), 1e-300))


def main() -> None:
    rows = load_manifest()
    results: dict[str, list[tuple[float, float]]] = {"legacy": [], "mip": []}
    buffers: dict[tuple[str, int], np.ndarray] = {}

    for row in rows:
        cfg = row["config"]
        bin_index = int(row["bin"])
        hz = float(row["exact_hz"])
        data = np.fromfile(DATA / row["file"], dtype=np.float32).astype(np.float64)
        buffers[(cfg, bin_index)] = data
        results[cfg].append((hz, alias_to_signal_db(data, bin_index)))

    for key in results:
        results[key].sort()

    print("\n=== Sawtooth alias-to-signal ratio (dB, lower is better) ===")
    print(f"{'f0 (Hz)':>9} | {'previous':>10} | {'current':>10} | {'gain':>7}")
    print("-" * 46)
    for (hz, old), (_, new) in zip(results["legacy"], results["mip"]):
        print(f"{hz:9.1f} | {old:10.1f} | {new:10.1f} | {old - new:7.1f}")

    # ── Sweep ────────────────────────────────────────────────────────────────
    fig, ax = plt.subplots(figsize=(9, 4.4))
    for cfg in ("legacy", "mip"):
        arr = np.array(results[cfg])
        ax.semilogx(arr[:, 0], arr[:, 1], "o-", color=COLOUR[cfg],
                    lw=1.8, ms=5, label=LABEL[cfg])
    ax.axvline(FS / 128, color=C_REF, lw=1.0, ls="--")
    ax.annotate("$f_s/128$ = 344 Hz\n64 harmonics reach Nyquist here",
                xy=(FS / 128, -20), xytext=(400, -12), fontsize=8, color=C_REF,
                arrowprops=dict(arrowstyle="->", color=C_REF, lw=0.8))
    ax.set_xlabel("fundamental (Hz)")
    ax.set_ylabel("alias-to-signal ratio (dB)")
    ax.set_title("Sawtooth aliasing across the keyboard "
                 f"($f_s$ = {FS/1000:.1f} kHz)")
    ax.grid(alpha=0.3, which="both")
    ax.legend(frameon=False)
    fig.tight_layout()
    fig.savefig(FIGS / "oscillator_alias_sweep.png", dpi=150)
    plt.close(fig)

    # ── Spectra at two representative notes ──────────────────────────────────
    picks = []
    for target in (440.0, 2000.0):
        best = min(results["mip"], key=lambda r: abs(r[0] - target))
        bin_index = int(round(best[0] * len(buffers[("mip", 1)]) / FS)) \
            if ("mip", 1) in buffers else None
        picks.append(best[0])

    fig, axes = plt.subplots(1, 2, figsize=(12, 4.4), sharey=True)
    for ax, hz in zip(axes, picks):
        for cfg in ("legacy", "mip"):
            key = next(k for k in buffers if k[0] == cfg
                       and abs(k[1] * FS / len(buffers[k]) - hz) < 0.5)
            data = buffers[key]
            n = len(data)
            freqs = np.fft.rfftfreq(n, 1.0 / FS)
            mag = 20.0 * np.log10(np.abs(np.fft.rfft(data)) / (n / 2) + 1e-14)
            ax.plot(freqs, mag, color=COLOUR[cfg], lw=0.8,
                    label=LABEL[cfg], alpha=0.85)
        for m in range(1, int(FS / 2 / hz) + 1):
            ax.axvline(m * hz, color=C_REF, lw=0.4, ls=":", alpha=0.4)
        ax.set_xlim(0, FS / 2)
        ax.set_ylim(-140, 5)
        ax.set_xlabel("frequency (Hz)")
        ax.set_title(f"$f_0$ = {hz:.1f} Hz")
        ax.grid(alpha=0.3)
    axes[0].set_ylabel("magnitude (dB)")
    axes[0].legend(frameon=False, fontsize=8, loc="lower right")
    fig.suptitle("Dotted lines mark true harmonics; everything between them "
                 "is aliasing")
    fig.tight_layout()
    fig.savefig(FIGS / "oscillator_spectra.png", dpi=150)
    plt.close(fig)

    print(f"\nfigures -> {FIGS / 'oscillator_alias_sweep.png'}")
    print(f"           {FIGS / 'oscillator_spectra.png'}\n")


if __name__ == "__main__":
    main()
