# Synth 1.0

A polyphonic **wavetable synthesizer** VST3/Standalone plugin built with [JUCE 8](https://juce.com/).  
Serum-inspired architecture: wavetable morphing, oscillator warp, LFO engine, unison, 3-band EQ, reverb, and a full FX rack with 2× oversampled saturation.

---



## Features

### Oscillator
- Four band-limited wavetables: **Sine · Saw · Square · Triangle** (64 harmonics each)
- **Wavetable morphing** — WT Position slider linearly cross-fades between adjacent tables
- **Oscillator Warp** — four phase-distortion modes applied before table lookup:
  - `None` — bypass
  - `Sync` — hard-sync simulation (phase × ratio, folded)
  - `Bend` — non-linear phase redistribution (tilt point controlled by Amount)
  - `PWM` — pulse-width modulation via phase folding
- Real-time waveform visualizer reflects the current morphed + warped shape

### Voice Architecture
- **16-voice polyphony** via `juce::Synthesiser`
- **Voice Modes** (MOD tab):
  - `Poly` — standard polyphony
  - `Mono` — single voice, always retrigers ADSR; note stack resumes prior held notes
  - `Legato` — slides pitch without ADSR retrigger; returns to previous note when released
- **Glide** (portamento) — `SmoothedValue<float, Multiplicative>` per voice; only activates when a note is played while another is held
- Per-voice signal chain: `Unison Oscs (warped) → constant-power pan → tanh drive → SVT Filter → ADSR × velocity`

### Unison (MOD tab)
- Up to **8 oscillators per voice** with constant-power normalization (`1/√N`)
- **Detune** — spreads voices ± semitones symmetrically
- **Spread** — pans voices across the stereo field (constant-power law)

### LFO Engine (MOD tab)
- Sine LFO with **Rate** (0.1–20 Hz)
- Routes to **Filter Cutoff** (±4000 Hz) and **Pitch** (±48 semitones) with independent depth knobs
- **Mod-depth ring** — the Cutoff knob draws a live amber arc showing the LFO sweep range
- Real-time LFO scope from a lock-free circular buffer

### 3-Band EQ (EQ tab)
- Low Shelf · Mid Bell (peak) · High Shelf
- **Interactive visualizer** — drag band handles directly on the frequency response display:
  - X axis: frequency (log scale, 20 Hz – 20 kHz)
  - Y axis: gain (±18 dB)
  - Scroll wheel on the mid-band handle: adjust Q (0.1 – 10)
- Live FFT spectrum overlay (2048-point Hann-windowed, 30 Hz refresh)

<img width="1092" height="586" alt="image" src="https://github.com/user-attachments/assets/ac1eac33-7274-4cb9-bc26-a57d5e18db62" />

### FX Rack (FX tab)
| Effect | Controls | Notes |
|---|---|---|
| **Chorus** | Mix · Rate · Depth | `juce::dsp::Chorus` |
| **Delay** | Time · Feedback · Mix | Stereo delay; snaps to quarter-note when DAW tempo is available |
| **Saturation** | Drive · Mix | `tanh` wave shaper with **2× oversampling** to eliminate aliasing |
| **Reverb** | Size · Damping · Mix | `juce::dsp::Reverb` (Schroeder/Moorer) |

### Preset System
- **Save** — exports current APVTS state as XML via file dialog
- **Load** — ComboBox populated from `Documents/Synth1_0/Presets/*.xml`; selecting a preset applies it instantly
- Fully state-serialized: all parameters saved and restored

<img width="1087" height="576" alt="image" src="https://github.com/user-attachments/assets/e2a8630f-4dba-451e-ad19-1f72b36af2f4" />

### UI
- Tabbed layout: **MAIN · MOD · FX · EQ** — 800 × 358 px
- Dark theme (Catppuccin Mocha-inspired)
- Custom `LookAndFeel`: arc-style rotary knobs, LFO mod-depth ring on Cutoff
- All controls bound to APVTS — fully automatable

---

## Building

### Requirements
- **Windows 10/11**, x64
- [Visual Studio 2022](https://visualstudio.microsoft.com/) with *Desktop development with C++*
- [JUCE 8](https://juce.com/get-juce/) installed to `C:\Program Files\JUCE\`
- [Projucer](https://juce.com/discover/projucer) (bundled with JUCE)

### Steps

```
1. Open Synth1.jucer in Projucer
2. Click "Save Project and Open in IDE" — regenerates Builds/ and JuceLibraryCode/
3. In Visual Studio 2022, select Debug|x64 or Release|x64
4. Build → Build Solution  (Ctrl+Shift+B)
```

**Standalone app** (quickest for testing):
```
Builds/VisualStudio2022/x64/Debug/Synth1_0 - Standalone Plugin/Synth1_0.exe
```

**VST3 plugin**:
```
Builds/VisualStudio2022/x64/Debug/Synth1_0 - VST3/Synth1_0.vst3/
```

> `Builds/`, `JuceLibraryCode/`, and `.vs/` are excluded from version control.  
> Re-run Projucer to regenerate them after cloning.

---

## Architecture

```
Source/
├── PluginProcessor.h/.cpp   — APVTS, LFO engine, Mono/Legato MIDI, FX chain
├── PluginEditor.h/.cpp      — Tabbed UI, interactive EQ, preset bar, mod rings
├── SynthVoice.h             — Unison engine, glide, warp, SVT filter, ADSR
├── WavetableOscillator.h    — Band-limited wavetable with phase-warp modes
└── SynthSound.h             — Trivial SynthesiserSound marker
```

### Signal chain (`processBlock`)
```
MIDI (mono/legato pre-process) →
  LFO →
  Voices (unison + warp + glide + filter + ADSR) →
  Master Gain →
  Chorus → Delay → Saturation (2× OS) → 3-Band EQ →
  SpecVisBuf (FFT feed) →
  Reverb
```

### Lock-free audio ↔ UI
- Parameters: `std::atomic<float>*` pointers from APVTS, read once per block
- LFO scope: `LfoVisBuf` ring buffer (512 samples), single-writer/single-reader
- Spectrum: `SpecVisBuf` ring buffer (4096 samples), feeds the EQ FFT display

---

## Parameters

| ID | Range | Default | Description |
|----|-------|---------|-------------|
| `wtPosition` | 0 – 3 | 0 | Wavetable morph position |
| `attack` | 1 ms – 2 s | 10 ms | ADSR attack |
| `decay` | 1 ms – 2 s | 100 ms | ADSR decay |
| `sustain` | 0 – 1 | 0.8 | ADSR sustain level |
| `release` | 10 ms – 5 s | 200 ms | ADSR release |
| `cutoff` | 20 – 20 000 Hz | 8 000 Hz | Filter cutoff |
| `resonance` | 0.1 – 10 | 0.7 | Filter Q |
| `drive` | 1 – 10 | 1 | Pre-filter tanh drive |
| `gain` | 0 – 1 | 0.7 | Master output gain |
| `warpMode` | None/Sync/Bend/PWM | None | Oscillator warp mode |
| `warpAmount` | 0 – 1 | 0 | Warp intensity |
| `voiceMode` | Poly/Mono/Legato | Poly | Voice mode |
| `glideTime` | 0 – 2 s | 0 | Portamento time |
| `lfoRate` | 0.1 – 20 Hz | 1 Hz | LFO speed |
| `lfoCutoffDepth` | 0 – 1 | 0 | LFO → filter cutoff (× 4000 Hz) |
| `lfoPitchDepth` | 0 – 48 st | 0 | LFO → pitch (semitones) |
| `unisonVoices` | 1 – 8 | 1 | Unison voice count |
| `unisonDetune` | 0 – 1 | 0 | Unison detune spread (semitones) |
| `unisonSpread` | 0 – 1 | 0 | Unison stereo spread |
| `chorusMix/Rate/Depth` | — | 0/1/0.25 | Chorus |
| `delayTime` | 10 – 1 000 ms | 250 ms | Delay time |
| `delayFeedback` | 0 – 0.95 | 0.3 | Delay feedback |
| `delayMix` | 0 – 1 | 0 | Delay wet level |
| `satDrive` | 1 – 20 | 1 | Saturation drive |
| `satMix` | 0 – 1 | 0 | Saturation wet level |
| `eqLowFreq/Gain` | 20–2000 Hz / ±18 dB | 200/0 | Low shelf |
| `eqMidFreq/Gain/Q` | 200–8000 Hz / ±18 dB / 0.1–10 | 1000/0/1 | Mid bell |
| `eqHighFreq/Gain` | 1–20 kHz / ±18 dB | 5000/0 | High shelf |
| `reverbSize/Damping/Mix` | 0 – 1 | 0.5/0.5/0 | Reverb |

---

## License

MIT — see [LICENSE](LICENSE) for details.
