# Signal v0.2.0

**Polymetric microsound rhythm generator with breakbeat engine + Scene A/B morph** for [Ableton Move](https://www.ableton.com/move/), built on the [Schwung](https://github.com/charlesvestal/schwung) framework.

Inspired by **Ryoji Ikeda** (data sonification, frequencies), **Nicolas Bernier** (sparse pings, tuning forks), **The Dillinger Escape Plan** & **Converge** (mathcore polyrhythms), **Frank Bretschneider** (spectral click variation), **Russell Holzman** (live jazz-breakbeat drumming), and the breakbeat algorithm documented in [mestela/schwung-breakbeat](https://github.com/mestela/schwung-breakbeat).

---

## What's new in v0.2.0

- **Breakbeat engine** — 10 new rhythm presets (Amen, Think, Funky Drummer, Jungle Roll, Liquid DnB, Holzman Improv, ...) driven by a stochastic anchor-curve algorithm with Roll/Complexity/Anchor characteristics baked in per preset.
- **Kit synthesis** — 10 new synth presets (Kick, Kick Sub, Snare Body, Piccolo Snare, Rim Click, Closed/Open Hat, Ride Ping, Tom, Ghost Snare) plus 3 new layered waveforms (`Kick`, `Snare`, `Hat`).
- **Scene A/B + Morph** — capture two complete module states and morph between them with a single knob (K7 on the General page). Continuous params lerp smoothly; discrete enums snap at 0.5. Three morph curves (Linear / Exp / Stepped) + 0–200 ms smoothing.
- **Holzman live-drummer layer** — per-voice Improv (per-bar curve mutation), Push/Pull micro-timing, multi-pitch Snare Bank (Off/Rule/Random with 5 selectable pitches), Ghost Crescendo (velocity ramp into accents). Plus global Drummer Brain (cross-voice influence Off/Light/Heavy).
- **Phrase + fills** — global Phrase Length (Off / 2 / 4 / 8 / 16 bars) with 6 selectable fill shapes (Snare Roll, Tom Run, Kick Fill, Hat Splatter, Crash Drop, Random) overlaid on the last bar of each phrase.
- **Retrigger** — per-breakbeat-preset retrigger probability with global rate (2× / 3× / 4× / 8× / Random) creating jungle-style stutter bursts.
- **10 new patches** (40–49): Amen, Think, Funky Drummer, Piccolo Jungle, Liquid DnB, Holzman Live, Microsound Jungle, Bernier Jungle, Mathcore Break, Bretschneider Roll. Patches 46–49 are hybrids that mix breakbeat rhythms with v0.1 microsound synthesis.
- **General page reorganized** — BPM is now K1, Master Vol moves to K8, Scene Morph lands on K7. Tempo Sync moves to the menu.
- **Action-knob fix** — `rnd_*`, `same_voice`, `mod_reset`, `save_scene_a/b` now act as tap-to-fire buttons (each right-turn fires the action). Trade-off: the displayed value counts up because Schwung Move tracks the local knob value internally.

Boot defaults: **Scene A = Ikeda Grid** (the v0.1 microsound aesthetic), **Scene B = Amen** (the canonical breakbeat). Twist K7 on the General page to morph between them.

---

## Concept

4 independent self-sequencing voices, each combining:
- **Rhythm engine**: 25 deterministic pattern presets (v0.1) **OR** one of 10 stochastic breakbeat presets (v0.2)
- **Micro-synth**: 50 presets (40 v0.1 ultra-short oscillator bursts + 10 v0.2 kit-style synths)

**35 rhythms × 50 synths × 4 voices** gives an astronomically large patch space. The Scene Morph knob lets you sweep between any two configurations during performance.

---

## Features

### Rhythm presets (35 total — 25 voice-dependent patterns + 10 voice-independent breakbeats)

#### Pattern engine — `seq` values 1–25

| Category | Count | Example | Inspiration |
|----------|-------|---------|-------------|
| **Morse code** | 5 | SOS, CQ, QRZ, DE, PSE | Radio transmission, Ikeda data |
| **Mathcore odd-meters** | 5 | 7/8, 11/8, 13/8, 15/16 | Polymetric complexity, DEP |
| **Ikeda grids** | 5 | Every-3-dropout, Triplet 32nds, Dense burst | Precise frequency/timing, grid algebra |
| **Bernier sparse** | 5 | Prime-spacing (2,3,5,7,11), Fibonacci | Sequence-based minimalism |
| **Glitch/math** | 5 | Pi/e/phi binary, Rule 30/110 CA, Logistic map | Deterministic chaos, self-similarity |

Each pattern repeats at a configurable subdivision (1/4 → 1/64 note) and master clock divider (÷1 to ÷32).

#### Breakbeat engine — `seq` values 26–35 (v0.2 NEW)

| # | Name | Role | Default A/C/R | Inspiration |
|---|------|------|---------------|-------------|
| 26 | Amen Kick | Kick | 0.80 / 0.20 / 0.10 | Halftime kick anchor |
| 27 | Amen Snare | Snare | 0.90 / 0.10 / 0.20 | Snare on 2/4 + ghost cluster |
| 28 | Amen Hat | Hat | 0.40 / 0.30 / 0.40 | Steady 8th notes |
| 29 | Amen Ghost | Ghost | 0.50 / 0.50 / 0.60 | Anti-anchored fillers |
| 30 | Think Snare | Snare | 0.85 / 0.15 / 0.30 | Lyn Collins ghost approach |
| 31 | Funky Drummer Kick | Kick | 0.70 / 0.30 / 0.20 | Clyde Stubblefield syncopation |
| 32 | Jungle Roll | Hat | 0.30 / 0.50 / 0.80 | High Roll, ±1 walks |
| 33 | Liquid DnB Snare | Snare | 0.95 / 0.05 / 0.10 | Sparse clean snares |
| 34 | Holzman Improv | Any | 0.60 / 0.40 / 0.30 | Mutation-receptive base |
| 35 | Breakbeat Random | Any | 0.20 / 0.80 / 0.50 | Maximum stochastic |

When a voice's `seq` is 26+, the rhythm engine switches from deterministic pattern playback to a **stochastic step-trigger algorithm**:

1. At each step boundary, look up the step's weight on a 32-element anchor curve.
2. With probability `Roll`, enter the "Stay" branch — either repeat (no fire), walk ±1 to a neighbor, or fire after a 5% escape jump.
3. Otherwise enter the "Move" branch — independent decision based on `Complexity × weight + base probability`.
4. Optionally schedule a retrigger burst (jungle stutter) on fire.

The algorithm is re-implemented from scratch from mestela's documented description — no code copied.

### Micro-synth presets — `syn` values 1–50

| # | Type | Count | Character |
|---|------|-------|-----------|
| 1–5 | Sine pips | 5 | 0.2–20 ms pure tone (Ikeda) |
| 6–10 | Damped sinusoid | 5 | Bell / tuning fork (Bernier) |
| 11–13 | Filtered noise | 3 | White noise bursts |
| 14–15 | Click / impulse | 2 | Sharp transients |
| 16–20 | FM | 5 | Metallic / bell |
| 21–23 | Square / Tri (PolyBLEP) | 3 | Anti-aliased digital |
| 24–26 | Pink noise | 3 | Colored noise |
| 27–28 | Brown noise | 2 | Deep / warm noise |
| 29–33 | AM | 5 | Amplitude modulation |
| 34–37 | Specialized | 4 | Long bell, hard FM, sine, filtered noise |
| 38–40 | Click / Gaussian ping | 3 | Ikeda signature |
| **41–50** | **Kit synthesis (v0.2 NEW)** | **10** | Kick, Kick Sub, Snare Body, Piccolo Snare, Rim Click, Closed Hat, Open Hat, Ride Ping, Tom Synth, Ghost Snare |

Kit synths add 3 new waveforms (`Kick`, `Snare`, `Hat`) with internal sweep/body/noise mixing tuned for breakbeat voices.

---

## Control architecture

10 pages × 8 knobs (jog-wheel navigation between pages, jog-wheel menu within each page).

### Page 1 — Generate (root)

| Knob | 1–4 | 5–8 |
|------|-----|-----|
| **Function** | Rhythm preset V1–V4 | Synth preset V1–V4 |
| **Range** | 0–35 (0=OFF, 1–25=pattern, 26–35=breakbeat) | 0–50 (0=OFF, 1–40=micro, 41–50=kit) |

### Page 2 — Patch

| Knob | Function |
|------|----------|
| 1 | Patch selector (50 patches) |
| 2 | Rnd Patch *(tap to fire)* |
| 3 | Rnd Rytm *(tap to fire)* |
| 4 | Rnd Voices *(tap to fire)* |
| 5 | Rnd Mod *(tap to fire)* |
| 6 | Rnd Pitch *(tap to fire)* |
| 7 | Rnd Pan *(tap to fire)* |
| 8 | All Decay (0–1, extends all decays toward 0.5 s) |

Menu: `Same Voice` (tap to unify all 4 voices to same synth).

> **Tap-to-fire knobs** (`Rnd *`, `Save Scene *`, `Mod Reset`, `Same Voice`) are exposed as wide-range integers (0–127) so every right-turn fires the action. The on-screen value will count up — that's expected. Each right-turn is an independent "fire" event regardless of where the value sits.

### Page 3 — Sequences

| Knob | Param | Range |
|------|-------|-------|
| 1 | Root | Free / C / C# / ... / B |
| 2 | Scale | Free / Chromatic / Major / Minor / Pentatonic / WholeTone / Diminished / HarmMin |
| 3 | Density | 0–1 |
| 4 | Chaos | 0–1 |
| 5 | Gravity | 0–1 |
| 6 | Clk Div | 1–32 |
| 7 | Morse Spd | 0–1 |
| 8 | Swing | 0–1 |

### Page 4 — Modulation (LFO)

| Knob | Param | Range |
|------|-------|-------|
| 1 | Mod Amt | 0–1 |
| 2 | Mod Speed | 0–10 Hz |
| 3 | Freq Mod | ±2 octaves |
| 4 | Decay Mod | 0–1 |
| 5 | Pan Mod | 0–1 |
| 6 | Density Mod | 0–1 |
| 7 | Mod Shape | Sine / Tri / Saw / Square / S&H / Random |
| 8 | Mod Reset *(tap to fire)* |

Menu: `Mod Offset` (per-voice LFO phase stagger).

### Page 5 — Mix

| Knob | 1–4 | 5–8 |
|------|-----|-----|
| **Function** | Voice levels | Voice base frequencies (Hz) |
| **Range** | 0–1 | 20–20000 |

### Pages 6–9 — Voice 1–4 (per-voice detail)

**Knobs:**
| Knob | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|------|---|---|---|---|---|---|---|---|
| **Param** | Preset | Vol | Freq | Wave | Tone | Decay | Detune | Pan |
| **Range** | 0–50 | 0–1 | 20–20k Hz | 16 types | 0–1 | 0.1–500 ms | 0–20 Hz | –1 to +1 |

Waveforms (16): Sine, Impulse, Noise, Damped, Click, Square, Tri, FM, Pink, Brown, AM, Ping, Train, **Kick, Snare, Hat** (v0.2 NEW).

**Menu (jog wheel):**
- **Attack** (0.1–50 ms): envelope sharpness
- **Sub Div** (1–8): sub-harmonic divider
- **Sweep** (0–1): pitch envelope arc
- **Tone Rnd** (0–100%): per-impulse tone randomization (Bretschneider)
- **Improv** (0–1, v0.2): per-bar anchor-curve mutation (Holzman)
- **Push/Pull** (–20 to +20 ms, v0.2): directional micro-timing on odd steps
- **Bank Mode** (Off / Rule / Random, v0.2): multi-pitch snare bank
- **Bank Pitch 0–4** (–24 to +24 semitones, v0.2): 5 selectable pitches
- **Ghost Cresc** (Off / On, v0.2): velocity ramp into accents

### Page 10 — General

| Knob | Param | Range |
|------|-------|-------|
| 1 | BPM | 20–500 |
| 2 | Drift | 0–1 |
| 3 | Jitter | 0–1 |
| 4 | Bit Crush | 1–16 bits |
| 5 | Bit Rate | 0–1 (sample-rate decimation) |
| 6 | Stereo Width | 0–1 |
| **7** | **Scene Morph** | **0–1 (A ↔ B)** |
| 8 | Master Vol | 0–1 |

**Menu:**
- Tempo Sync (Free / Sync) — moved here from K2 in v0.1
- DC Filter (Off / On)
- Out Mode (Stereo / Mono / Spread)
- **Save → Scene A** *(tap to fire, captures live state into Scene A)*
- **Save → Scene B** *(tap to fire, captures live state into Scene B)*
- **Morph Curve** (Linear / Exp / Stepped)
- **Morph Smooth** (0–200 ms — one-pole on the Morph knob)
- **Phrase Length** (Off / 2 / 4 / 8 / 16 bars)
- **Fill Shape** (Snare Roll / Tom Run / Kick Fill / Hat Splatter / Crash Drop / Random)
- **Step Grid** (8 / 16 / 32 — breakbeat step resolution per bar; default 32)
- **Retrig Rate** (2× / 3× / 4× / 8× / Random)
- **Drummer Brain** (Off / Light / Heavy — cross-voice influence)

---

## Scene A / B + Morph

Each scene captures the full per-voice configuration (rhythm preset, synth preset, vol, freq, decay, wave, tone, detune, pan, attack, sub div, sweep, tone rnd, improv, push_pull, bank mode + 5 pitches, ghost crescendo) plus global rhythm / breakbeat / modulation params.

**Not captured** (these stay fixed across morph): BPM, Tempo Sync, Master Vol, Bit Crush, Bit Rate, Stereo Width, Drift, Jitter, the Morph knob itself.

**Morph behaviour:**
- Continuous params (vol, freq, decay, anchor weights, etc.) → linear interpolation A ↔ B
- Discrete enums (presets, waveforms, fill shape, etc.) → hard switch at morph ≥ 0.5
- Morph value is one-pole smoothed (configurable 0–200 ms) to prevent zipper noise
- Three morph curves: **Linear** (uniform), **Exp** (slow start/end, fast middle), **Stepped** (instant flip at 0.5)

**Workflow:**
1. Build a patch on Generate, jog to General → **Save → Scene A**
2. Build another patch, jog to General → **Save → Scene B**
3. Perform: General page open, twist K7 between extremes (or sit in the hybrid middle zone)

Hybrid territory between 0.3–0.7 is musically valuable — it's where the Microsound Jungle / Bernier Jungle / Mathcore Break aesthetic lives as positions on the knob rather than separate presets.

---

## Patch library (50 presets)

| # | Name | Vibe |
|---|------|------|
| 0–39 | Init, Ikeda Grid, Bernier, Morse CQ, Mathcore, ... Aleatoric | v0.1 patches (unchanged) |
| **40** | **Amen** | Canonical halftime kit reconstruction |
| **41** | **Think** | Lyn Collins snare anchor + DnB hat |
| **42** | **Funky Drummer** | Clyde Stubblefield syncopated kick |
| **43** | **Piccolo Jungle** | High-pitched DnB with snare bank |
| **44** | **Liquid DnB** | Rolling jungle, soft snare + ride |
| **45** | **Holzman Live** | Improv-heavy, multi-pitch snare, Drummer Brain Heavy |
| **46** | **Microsound Jungle** | Breakbeat rhythms + Ikeda sine pips (hybrid) |
| **47** | **Bernier Jungle** | Breakbeat rhythms + damped bell synthesis (hybrid) |
| **48** | **Mathcore Break** | 7/8 pattern kick + Amen ghost (hybrid) |
| **49** | **Bretschneider Roll** | Breakbeat + heavy spectral scatter (hybrid) |

---

## Artist features summary

### Ryoji Ikeda — Data sonification
Rhythm patterns from binary fractions of π, e, φ. Sparse prime-number spacing. Sub-20 ms sine pips for "test tone" clarity. Frequency quantization to musical scales.

### Nicolas Bernier — Tuning fork resonance
Damped waveform with time-domain decay (tau = 3 ms–200 ms). Sparse 2–8 step patterns. Sub-division octave shifts via pad velocity.

### Frank Bretschneider — Spectral click variation
Tone Rnd — per-impulse tone randomization. Each rhythm hit gets a random brightness offset, seeded and pattern-repeatable.

### Hainbach — Heterodyne, sweep, sub-harmonics
Detune (0–20 Hz heterodyne). Sweep (pitch envelope arc). Sub Div (1–8 sub-harmonic divider per voice).

### The Dillinger Escape Plan / Converge — Polymetric complexity
Independent voice rhythms at different tempos. Phase relationships evolve over bars (gravity locks sync points). Mathcore odd-meter patterns.

### Russell Holzman — Live jazz breakbeat (v0.2 NEW)
**Improv** — per-bar anchor-curve micro-mutation. **Push/Pull** — directional shuffle on odd steps. **Multi-pitch snare bank** — Rule-based or Random pitch selection per snare hit. **Ghost Crescendo** — velocity ramp into accent steps. **Drummer Brain** — cross-voice influence so kick/snare/hat react to each other like a live ensemble.

### mestela/schwung-breakbeat — Algorithm reference (v0.2 NEW)
Stochastic step-trigger decision tree with anchor weight curves. Algorithm re-implemented from the documented description; no code copied.

---

## Technical specifications

### DSP
- Sample rate: 44.1 kHz (fixed)
- Buffer size: 128 frames / block (~3 ms latency)
- Voices: 4 monophonic (independent sequencers)
- Waveforms: 16 types
- Synthesis: phase accumulator, FM index modulation, multi-stage envelopes, PolyBLEP anti-aliasing on square/pulse
- Output: soft-limiter (±0.85), DC blocker, bit-crusher, sample-rate decimation, M/S width modes

### CPU & Memory
- CPU usage: ~3–4% of available budget per block
- Memory per instance: ~5 KB (scaled up from ~1 KB in v0.1 to hold two scene snapshots + breakbeat state)
- Max polyphony: 4 voices per module (can stack multiple modules)

### Audio quality
- Anti-aliasing: PolyBLEP on square/pulse waveforms
- Denormal protection: 1e-20f guards on filter states
- Clipping prevention: soft-knee limiter at ±0.85
- DC offset removal: 1-pole highpass ~20 Hz

---

## Building & installation

### Requirements
- Docker (for cross-compilation to ARM64), OR `aarch64-linux-gnu-gcc` cross-compiler
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
Installs to Move at `modules/sound_generators/signal/`.

### Via Schwung Module Store
- Launch Schwung on Move → Module Store → search "Signal" → download.

---

## Release notes

### v0.2.0 (2026-05-13)

**Major features:**
- Breakbeat rhythm engine (10 presets) with anchor-curve stochastic decision algorithm.
- Scene A/B + Morph system on General K7.
- Holzman live-drummer layer (Improv, Push/Pull, Snare Bank, Ghost Crescendo, Drummer Brain).
- Phrase tracking + 6 fill shapes.
- Retrigger stutter bursts.
- 10 new patches (40–49), including 4 hybrid breakbeat × microsound patches.
- 10 new kit synth presets + 3 new layered waveforms (Kick / Snare / Hat).
- General page knob layout reorganized; Tempo Sync moves to menu.

**Other changes:**
- Tap-to-fire action knobs (Rnd *, Save Scene *, Mod Reset, Same Voice) now fire on every right-turn instead of getting stuck at 1.
- `seq` range extended to 0–35; `syn` range extended to 0–50.
- Boot defaults: Scene A = Ikeda Grid, Scene B = Amen, Step Grid = 32, Phrase = 8 bars, Drummer Brain = Off.
- Scene morph + breakbeat globals + per-voice Holzman state persist across module reload via Schwung's state mechanism.

**Backward compatibility:**
- All v0.1 patches (0–39) play unchanged.
- All v0.1 parameter keys preserved — host-side state from v0.1 loads cleanly.
- General page knob bindings remapped, but the underlying keys (`master_vol`, `bpm`, etc.) are unchanged so saved chain-param assignments still work.

### v0.1.1 (2026-04-09)
- Tone Rnd parameter display format fix.

### v0.1.0 (2026-04-09)
- Initial release: 4 voices, 25 rhythm presets, 40 synth presets, 40 patches, modulation LFO, Tone Rnd.

---

## File structure

```
signal-move/
├── src/
│   ├── dsp/
│   │   └── signal.c           # All DSP logic (4 voices, sequencer, synths, breakbeat, scenes)
│   └── module.json            # Schwung metadata + chain_params + ui_hierarchy
├── scripts/
│   ├── build.sh               # Docker ARM64 cross-compile
│   └── install.sh             # Deploy to Move via scp
├── .github/workflows/
│   └── release.yml            # CI: verify tag matches version, build, create release, update release.json
├── CLAUDE.md                  # Development context
├── README.md                  # This file
├── LICENSE                    # GPL-3.0
└── dist/                      # Build artifacts (tar.gz)
```

---

## Examples

### Spastic Ikeda → Amen breakbeat (Scene morph)
1. Open Signal — boots with Scene A = Ikeda Grid loaded
2. Jog to General page, K7 = 0 (pure Scene A) — hear Ikeda microsound rhythms
3. Twist K7 to 1.0 (pure Scene B) — instant flip to Amen breakbeat
4. Park K7 around 0.5 — hybrid territory: Amen rhythms played by sine pips, or Ikeda patterns with kick/snare body

### Holzman Live patch
1. Patch page → select **Holzman Live** (patch 45)
2. Hear: piccolo-snare with bank pitch variation, per-bar improv mutations, ghost crescendos into main snares, kick drops when snare is busy (Drummer Brain Heavy)
3. Modulate live: Voice 2 menu → Improv (push higher for more bar-to-bar variation), Ghost Cresc on/off

### Microsound Jungle (hybrid)
1. Patch page → select **Microsound Jungle** (patch 46)
2. Hear: Amen rhythm skeleton played entirely by Ikeda sine pips — kick is a 0.5 ms pip, snare is a 2 ms pip
3. Try changing Voice 1 syn to "Kick Synth" (preset 41) for a hybrid: pip ghost + body kick

### Polymetric Holzman
1. Generate → Voice 1 seq = 8 (mathcore 11-feel), syn = 41 (Kick Synth)
2. Voice 2 seq = 27 (Amen Snare), syn = 43 (Snare Body), menu → Improv = 0.7
3. Voice 3 seq = 9 (mathcore 13-feel), syn = 46 (Closed Hat)
4. Voice 4 seq = 29 (Amen Ghost), syn = 50 (Ghost Snare), menu → Ghost Cresc = On
5. General menu → Drummer Brain = Heavy
6. Result: polymetric kick + improvising snare + cross-voice influence

---

## Credits

**Concept, design & DSP**: fillioning

**Inspired by:**
- Ryoji Ikeda — mathematical data sonification
- Nicolas Bernier — sparse tuning forks, spatial resonance
- The Dillinger Escape Plan & Converge — polymetric math rock
- Frank Bretschneider — click density & spectral scatter
- Robert Henke (Monolake) — granular synthesis & micro-timing
- Taylor Deupree (12k) — microsound field technique
- Russell Holzman ([@starpowerdrummer](https://www.instagram.com/starpowerdrummer/)) — live jazz-breakbeat drumming
- [mestela/schwung-breakbeat](https://github.com/mestela/schwung-breakbeat) — breakbeat decision algorithm reference

**Framework**: [Schwung](https://github.com/charlesvestal/schwung) (Charles Vestal)

**Audio Engineering**: Ableton Move & gcc ARM64 toolchain

---

## License

GPL-3.0 — Free and open source. See [LICENSE](LICENSE) for full terms.

You are free to use, modify, and distribute Signal. You must license derivative works under GPL-3.0 and provide source code with any binary distribution.

---

## Troubleshooting

**Module doesn't load / changes don't appear?**
Remove Signal from the FX slot and re-add it. After module.json changes (new params, new pages, new ranges), Schwung needs a fresh load to pick up the manifest.

**Breakbeat voice silent?**
- Check `seq` is in the 26–35 range
- Check `syn` is set (0 = OFF)
- Try Patch → Amen for a known-good reference
- The breakbeat algorithm is probabilistic — with low `Anchor` and `Roll` values, some bars might not produce many fires

**Scene morph doesn't smoothly crossfade?**
- Check Morph Smooth in the General menu (default 50 ms; lower = snappier)
- Check Morph Curve (Stepped curve hard-flips at 0.5 — set to Linear for smooth)

**Rnd / Save knob shows counting numbers?**
That's expected — see the note on Page 2 above. Each right-turn fires the action regardless of the displayed value.

**CPU usage spiking?**
- Reduce active voices (set unused to OFF)
- Lower Density global parameter
- Use simpler waveforms (Sine instead of FM)

---

## Contact & contributing

- Issues / bugs: [GitHub Issues](https://github.com/fillioning/signal-move/issues)
- Discussion: [Schwung Community](https://github.com/charlesvestal/schwung/discussions)
</content>
</invoke>