# Synth 1.0

A polyphonic **wavetable synthesizer** VST3/Standalone plugin built with [JUCE 8](https://juce.com/).  
Designed with a Serum-inspired architecture: wavetable morphing, LFO modulation engine, and a post-processing FX rack with 2× oversampled saturation.

---

## Features

### Oscillator
- Four band-limited wavetables: **Sine · Saw · Square · Triangle** (64 harmonics each, additive synthesis)
- **Wavetable morphing** — a single WT Position slider linearly cross-fades between adjacent tables
- Real-time waveform visualizer reflects the current morphed shape
- SSE2-ready table architecture (per-instance phase, shared static tables built once via `std::call_once`)

### Voice Architecture
- **16-voice polyphony** via `juce::Synthesiser`
- Per-voice signal chain: `Oscillator → tanh(drive) → SVT Lowpass Filter → ADSR × velocity`
- `juce::dsp::StateVariableTPTFilter` (Q-controlled resonance)
- ADSR with live envelope visualizer (animated path + drop shadow)

### LFO Engine (MOD tab)
- Sine LFO with **Rate** (0.1–20 Hz) control
- Routes to **Filter Cutoff** and **Pitch** with independent depth knobs
- `juce::SmoothedValue` on both mod targets — click-free transitions
- Real-time LFO scope reading from a lock-free circular buffer

### FX Rack (FX tab)
| Effect | Controls | Notes |
|---|---|---|
| **Chorus** | Mix · Rate · Depth | `juce::dsp::Chorus` |
| **Delay** | Time · Feedback · Mix | Stereo `DelayLine`; snaps to quarter-note when a DAW tempo is available |
| **Saturation** | Drive · Mix | `tanh(x)` wave shaper with **2× oversampling** (`juce::dsp::Oversampling`) to eliminate aliasing |

### UI
- Tabbed layout: **MAIN · MOD · FX**
- Dark theme (Catppuccin Mocha-inspired palette)
- Custom `LookAndFeel`: arc-style rotary knobs, horizontal WT position slider
- All controls bound to APVTS — fully automatable in any DAW

---

## Building

### Requirements
- **Windows 10/11**, x64
- [Visual Studio 2022](https://visualstudio.microsoft.com/) (Community or higher) with the *Desktop development with C++* workload
- [JUCE 8](https://juce.com/get-juce/) installed to `C:\Program Files\JUCE\`
- [Projucer](https://juce.com/discover/projucer) (bundled with JUCE)

### Steps

```
1. Open Synth1.jucer in Projucer
2. Click "Save Project and Open in IDE" — this regenerates Builds/ and JuceLibraryCode/
3. In Visual Studio 2022, select Debug|x64 or Release|x64
4. Build → Build Solution  (Ctrl+Shift+B)
```

**Standalone app** (quickest for testing):
```
Builds/VisualStudio2022/x64/Debug/Synth1_0 - Standalone Plugin/Synth1_0.exe
```

**VST3 plugin** (load in any VST3 host):
```
Builds/VisualStudio2022/x64/Debug/Synth1_0 - VST3/Synth1_0.vst3/
```

> **Note:** `Builds/`, `JuceLibraryCode/`, and `.vs/` are excluded from version control.  
> Re-run Projucer to regenerate them after cloning.

---

## Architecture

```
Source/
├── PluginProcessor.h/.cpp   — APVTS, LFO engine, FX chain, processBlock
├── PluginEditor.h/.cpp      — Tabbed UI (Main/Mod/FX), custom LookAndFeel,
│                              ADSR/WT/LFO visualizers
├── SynthVoice.h             — Per-voice DSP (osc → drive → filter → ADSR)
├── WavetableOscillator.h    — Band-limited wavetable engine with morphing
└── SynthSound.h             — Trivial SynthesiserSound (applies to all notes)
```

### Lock-free audio thread guarantees
- UI ↔ Audio: parameters flow through `std::atomic<float>*` pointers obtained once from APVTS
- LFO → UI: single-writer/single-reader circular buffer (`LfoVisBuf`) — no mutex, visualization reads tolerate torn floats
- No dynamic allocation inside `processBlock` — `dryBuffer` pre-allocated in `prepareToPlay`

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
| `lfoRate` | 0.1 – 20 Hz | 1 Hz | LFO speed |
| `lfoCutoffDepth` | 0 – 1 | 0 | LFO → filter cutoff depth |
| `lfoPitchDepth` | 0 – 48 st | 0 | LFO → pitch depth (semitones) |
| `chorusMix` | 0 – 1 | 0 | Chorus wet level |
| `chorusRate` | 0.1 – 8 Hz | 1 Hz | Chorus modulation rate |
| `chorusDepth` | 0 – 1 | 0.25 | Chorus depth |
| `delayTime` | 10 – 1 000 ms | 250 ms | Delay time (or quarter-note) |
| `delayFeedback` | 0 – 0.95 | 0.3 | Delay feedback |
| `delayMix` | 0 – 1 | 0 | Delay wet level |
| `satDrive` | 1 – 20 | 1 | Saturation drive |
| `satMix` | 0 – 1 | 0 | Saturation wet level |

---

## License

MIT — see [LICENSE](LICENSE) for details.
