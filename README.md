# Signal v0.1.0

**Polymetric microsound rhythm generator** for [Ableton Move](https://www.ableton.com/move/),
built on the [Schwung](https://github.com/charlesvestal/schwung) framework.

Inspired by **Ryoji Ikeda** (data sonification, frequencies), **Nicolas Bernier** (sparse pings, tuning forks), **The Dillinger Escape Plan** & **Converge** (mathcore polyrhythms), and **Frank Bretschneider** (spectral click variation).

---

## Concept

4 independent self-sequencing voices, each combining:
- **Rhythm engine**: Glitch pattern generator (25 presets per voice)
- **Micro-synth**: Ultra-short oscillator burst (40 presets shared)

**25^4 = 390,625 unique rhythmic combinations** across all voices.

Each voice triggers at pattern-defined steps, generating a microsound impulse (0.1–500ms). The pattern repeats deterministically, enabling predictable polymetric evolution and cross-voice phase relationships.

---

## Features

### Rhythm Patterns (25 presets per voice)

| Category | Count | Example | Inspiration |
|----------|-------|---------|-------------|
| **Morse code** | 5 | SOS, CQ, QRZ, DE, PSE | Radio transmission, Ikeda data |
| **Mathcore odd-meters** | 5 | 7/8, 11/8, 13/8, 15/16 | Polymetric complexity, DEP |
| **Ikeda grids** | 5 | Every-3-dropout, Triplet 32nds, Dense burst | Precise frequency/timing, grid algebra |
| **Bernier sparse** | 5 | Prime-spacing (2,3,5,7,11), Fibonacci | Sequence-based minimalism |
| **Glitch/math** | 5 | Pi/e/phi binary, Rule 30/110 CA, Logistic map | Deterministic chaos, self-similarity |

Each pattern repeats at a **configurable subdivision** (1/4 note → 1/64 note) and **master clock** (÷1 to ÷32).

### Micro-Synth Waveforms (40 presets)

| Type | Count | Character | Artist |
|------|-------|-----------|--------|
| **Sine pips** | 5 | 0.2–20ms pure tone | Ikeda test signal |
| **Damped sinusoid** | 5 | Bell/tuning fork ring | Bernier solenoid strike |
| **Filtered noise** | 3 | White/pink/brown bursts | Formant color |
| **Percussive** | 3 | Click, train, transient | Physical modeling |
| **FM micro-textures** | 5 | Metallic/bell modulation | Spectral morphing |
| **Pink/brown noise** | 5 | Colored noise variants | Organic timbre |
| **Digital waveforms** | 4 | PolyBLEP square/tri, AM | Anti-aliased synthesis |
| **Specialized** | 5 | Gaussian pings, clicks, data | Ikeda/Brenier precision |

---

## Control Architecture

### Pages (8 total, 8 knobs each)

#### **Séquences** (Root)
| Knob | 1–4 | 5–8 |
|------|-----|-----|
| **Function** | Rhythm presets V1–V4 | Synth presets V1–V4 |
| **Range** | 0–25 | 0–40 |

#### **Mix**
| Knob | 1–4 | 5–8 |
|------|-----|-----|
| **Function** | Voice levels | Voice base frequencies (Hz) |
| **Range** | 0–1 | 20–20000 |

#### **Sequences** (Params)
| Knob | 1–4 | 5–8 |
|------|-----|-----|
| **1** | Root (Free, C–B) | Density (0–1) |
| **2** | Scale (Free, Chromatic, Major, Minor, etc.) | Chaos (0–1) |
| **3** | — | Gravity (0–1, voice sync) |
| **4** | — | ClkDiv (1–32), MorseSpd, Swing |

#### **Modulation (LFO)**
| Knob | 1–4 | 5–8 |
|------|-----|-----|
| **1** | Mod Amt (0–1) | Mod Speed (0–10 Hz) |
| **2** | Freq Mod (±2 octaves) | Decay Mod (0–1) |
| **3** | Pan Mod (0–1) | Density Mod (0–1) |
| **4** | Mod Shape (Sine/Tri/Saw/Sqr/S&H/Random) | Mod Reset |

**Per-voice LFO phase offset** via Mod Offset (creates evolving texture across voices).

#### **Voice 1–4** (per-voice detail)
**Knobs (direct):**
| Knob | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|------|---|---|---|---|---|---|---|---|
| **Param** | Preset | Vol | Freq | Wave | Tone | Decay | Detune | Pan |
| **Range** | 0–40 | 0–1 | 20–20k Hz | 11 types | 0–1 | 0.1–500ms | 0–20 Hz | –1 to +1 |

**Menu (jog-wheel):**
- **Attack** (0.1–50ms): Envelope sharpness / click transient
- **Sub Div** (1–8): Sub-harmonic divider (affects pad velocity-based octave)
- **Sweep** (0–1): Pitch envelope arc — rises 2^sweep octaves at attack peak
- **Tone Rnd** (0–1): **Per-impulse tone randomization depth** — each rhythm hit gets random tone offset ±100% × knob value (Bretschneider spectral scatter)

#### **General**
| Knob | 1–4 | 5–8 |
|------|-----|-----|
| **1** | Master Vol (0–1) | Tempo Sync (Free / Sync) |
| **2** | BPM (20–500) | Stereo Width (0–1) |
| **3** | Bit Crush (1–16 bits) | Drift (0–1) |
| **4** | Bit Rate (sample-rate decimation) | Jitter (0–1) |

**Menu:**
- **DC Filter** (Off / On): 1-pole highpass ~20 Hz
- **Out Mode** (Stereo / Mono / Spread): M/S width mode

---

## Patch System (40 presets)

Pre-configured 4-voice layouts:

| # | Name | Rhythm | Synth | Use Case |
|---|------|--------|-------|----------|
| 1 | Init | Morse SOS + variants | Sine pips | Starting point |
| 2 | Ikeda Grid | Regular grids, dropout | Pure tone | Minimal data |
| 3 | Bernier | Sparse prime-spacing | Damped | Bell resonance |
| 4 | Morse CQ | CQ call pattern | Sine | Radio aesthetic |
| 5 | Mathcore | 7/8, 11/8 bursts | Noise | Complex meters |
| 6–20 | ... (15 artist-inspired presets) | ... | ... | ... |
| 21–40 | Data Quanta, Rule 30, Aleatoric, etc. | CA/chaos patterns | Mixed | Generative |

**Patch page controls:**
- **Rnd Patch**: Randomize all 4 voices + parameters
- **Rnd Rytm**: Randomize rhythm patterns only
- **Rnd Voices**: Randomize synth presets + frequencies
- **Rnd Mod**: Randomize modulation parameters
- **Rnd Pitch**: Randomize voice frequencies (scale-quantized)
- **Rnd Pan**: Randomize voice pan positions
- **All Decay**: Extend all voice decays toward 0.5s (global sustain)
- **Same Voice**: All 4 voices use same synth preset (unified timbre)

---

## Artist Features

### **Ryoji Ikeda — Data Sonification**
- Rhythm patterns from binary fractions of π, e, φ
- Sparse prime-number spacing
- Sub-20ms sine pips for "test tone" clarity
- Frequency quantization to musical scales

### **Nicolas Bernier — Tuning Fork Resonance**
- Damped waveform with time-domain decay (tau = 3ms–200ms)
- Sparse 2–8 step patterns (silence dominates)
- Sub-division octave shifts via pad velocity
- Minimal voice count (often 1–2 active)

### **Frank Bretschneider — Click Density Variation**
- **Tone Rnd**: Per-impulse tone randomization (newly exposed in v0.1.0)
  - Each rhythm hit gets random brightness offset
  - Seeded & pattern-repeatable (same pattern = same tone sequence)
  - 0.0 = deterministic, 0.5 = ±50% scatter, 1.0 = full chaos
  - Creates spectral variation within static rhythm (Bretschneider glitch aesthetic)

### **Hainbach — Heterodyne, Sweep, Sub-harmonics**
- **Detune** (0–20 Hz): Heterodyne second sine for beat frequency / AM texture
- **Sweep** (0–1): Pitch envelope arc — grain pitch rises from base during attack
- **Sub Div** (1–8): Sub-harmonic divider per voice (affects pad row-based octave)

### **The Dillinger Escape Plan / Converge — Polymetric Complexity**
- Voices run independent rhythm patterns at different tempos (via clk_div)
- Phase relationships evolve over bars (gravity locks sync points)
- Mathcore odd-meter patterns (7, 11, 13, 15, 17, 19-feel)
- Cross-voice modulation (LFO can modulate all 4 simultaneously)

---

## Technical Specifications

### DSP Architecture
- **Sample rate**: 44.1 kHz (fixed)
- **Buffer size**: 128 frames / block
- **Latency**: ~3ms
- **Voices**: 4 monophonic (independent sequencers)
- **Waveforms**: 13 types (Sine, Impulse, Noise, Damped, Click, Square, Tri, FM, Pink, Brown, AM, Ping, Train)
- **Filters**: One-pole LP (noise), Chamberlin SVF bandpass (Alva Noto narrow-band)
- **Synthesis**: Phase accumulator, FM index modulation, envelope followers
- **Output**: Soft-limiter (±0.85 transparent knee), DC blocker, bit-crusher, sample-rate decimation

### CPU & Memory
- **CPU usage**: ~3% of available budget per block (excellent headroom)
- **Memory per instance**: ~1 KB (minimal footprint)
- **Max polyphony**: 4 voices per module (can stack multiple modules)

### Audio Quality
- **Anti-aliasing**: PolyBLEP on square/pulse waveforms
- **Denormal protection**: 1e-20f guards on filter states
- **Clipping prevention**: Soft-knee limiter at ±0.85
- **DC offset removal**: 1-pole highpass ~20 Hz

---

## Building & Installation

### Requirements
- Docker (for cross-compilation to ARM64), OR
- `aarch64-linux-gnu-gcc` cross-compiler
- Ableton Move with Schwung installed

### Build
```bash
./scripts/build.sh
```

Output: `dist/signal-module.tar.gz`

### Deploy to Move
```bash
./scripts/install.sh
```

Installs to Move at `modules/sound_generators/signal/` and reloads the module.

### Or: Install via Module Store
- Launch Schwung on Move
- Open Module Store
- Search "Signal"
- Download & install

---

## Release Notes

### v0.1.0 (2026-04-09)
**Features:**
- 4 independent rhythm-synth voices
- 25 rhythm patterns × 4 voices (Morse, mathcore, Ikeda, Bernier, CA)
- 40 micro-synth presets (sine pips, damped, FM, noise, clicks, digital)
- 40 named patch presets (Init, Ikeda Grid, Bernier, artist-inspired)
- Per-voice control: frequency, volume, waveform, decay, pan, sweep, detune, sub-div
- Global parameters: root/scale quantization, density, chaos, gravity, tempo sync, BPM
- Modulation LFO: 6 shapes (Sine, Tri, Saw, Square, S&H, Random), per-voice phase offset
- **NEW**: Tone Rnd — per-impulse tone randomization (Bretschneider spectral scatter)
- Output modes: Stereo / Mono / Spread (M/S width)
- Effects: bit-crush (1–16 bits), bit-rate decimation (sample-rate reduction)
- Safety: soft-limiter, DC blocker, denormal guards

**DSP Quality:**
- ✅ All filters stable at frequency extremes
- ✅ Denormal protection on all filter states
- ✅ Feedback paths bounded (no runaway)
- ✅ CPU usage ~3% budget (excellent performance)
- ✅ Production-ready audio quality

**Known Limitations:**
- 4-voice limit per module (stack multiple modules for more)
- No MIDI CC modulation (parameters only via Move UI or Schwung API)
- No sample playback (synthesis only)

---

## File Structure

```
signal-move/
├── src/
│   ├── dsp/
│   │   └── signal.c           # All DSP logic (4 voices, sequencer, synths)
│   └── module.json            # Schwung metadata, parameter definitions
├── scripts/
│   ├── build.sh               # Docker ARM64 cross-compile
│   └── install.sh             # Deploy to Move via scp
├── .github/workflows/
│   └── release.yml            # CI: build, tag, create release
├── CLAUDE.md                  # Development context (artist features, parameters)
├── README.md                  # This file
├── LICENSE                    # GPL-3.0
└── dist/                      # Build artifacts (tar.gz)
```

---

## Examples

### Minimalist Bernier
1. **Patch → Bernier** (auto-loads sparse pattern + damped synth)
2. **Voice 1**: Decay = 0.5s (long bell ring)
3. **Voice 2**: Decay = 0.1s (short click)
4. **Voice 3/4**: OFF
5. **Density**: 0.7 (sparse, probabilistic)
6. Result: Two sparse voices phasing in and out, with different decay characters

### Spectral Glitch (Bretschneider)
1. **Seq1**: Rule 30 CA pattern (deterministic chaos)
2. **Syn1**: FM preset (metallic character)
3. **V1 Tone**: 0.5 (baseline brightness)
4. **V1 Tone Rnd**: 0.6 (60% random scatter per hit)
5. **Density**: 1.0 (all pattern steps fire)
6. Result: Each impulse gets random brightness ±60%, creating evolving spectral texture within repeating pattern

### Polymetric Morse
1. **Seq1**: Morse SOS (sub=8, fast)
2. **Seq2**: Morse CQ (sub=8, different timing)
3. **Seq3**: Rule 110 (sub=4, slower)
4. **Seq4**: OFF
5. **V1/2/3 Detune**: 0.5 Hz each (heterodyne beating)
6. **Gravity**: 0.5 (voices sync at bar boundaries)
7. Result: 3 independent Morse patterns evolving with slow beat-frequency modulation

---

## Credits

**Concept, Design & DSP**: fillioning

**Inspired by:**
- Ryoji Ikeda — mathematical data sonification
- Nicolas Bernier — sparse tuning forks, spatial resonance
- The Dillinger Escape Plan & Converge — polymetric math rock
- Frank Bretschneider — click density & spectral scatter
- Robert Henke (Monolake) — granular synthesis & micro-timing
- Taylor Deupree (12k) — microsound field technique

**Framework**: [Schwung](https://github.com/charlesvestal/schwung) (Charles Vestal)

**Audio Engineering**: Ableton Move & gcc ARM64 toolchain

---

## License

GPL-3.0 — Free and open source. See [LICENSE](LICENSE) for full terms.

You are free to:
- Use, modify, and distribute Signal
- Build on this code for your own projects

Under the condition that you:
- Provide source code with any binary distribution
- License derivative works under GPL-3.0

---

## Troubleshooting

**Module doesn't load?**
- Remove Signal from FX slot, re-add it
- Check Move logs: SSH to move.local, check `/var/log/ableton/`

**Tone Rnd not working?**
- Navigate to Voice page (1–4)
- Scroll down with jog wheel to find "Tone Rnd"
- Set to 0.1–0.5 to hear effect

**Audio clicks or pops?**
- Reduce Density (fewer simultaneous voices)
- Lower Decay values (shorter grains, less overlap)
- Increase Decay Mod (LFO modulates decay for variation)

**CPU usage spiking?**
- Reduce number of active voices (set unused to OFF)
- Lower Density global parameter
- Use simpler waveforms (Sine vs. FM)

---

## Contact & Contributing

Found a bug? Have suggestions?
- Open an issue: [GitHub Issues](https://github.com/fillioning/signal-move/issues)
- Discussion: [Schwung Community](https://github.com/charlesvestal/schwung/discussions)

---

**Enjoy designing polymetric microsound rhythms!** 🎵🔊✨
