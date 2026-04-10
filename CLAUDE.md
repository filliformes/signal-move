# Signal — Claude Code context

## What this is
Polymetric microsound rhythm generator inspired by Ryoji Ikeda, Nicolas Bernier (frequencies), The Dillinger Escape Plan, and Converge.
Schwung sound generator. API: plugin_api_v2_t. Language: C.
Architecture: 4 independent self-sequencing micro-synth voices (NOT traditional MIDI polyphony).

## Concept
Each voice = rhythm engine + micro-synth. Rhythm presets generate glitchy patterns
(Morse code, mathcore odd-meters, Ikeda grids, prime-number spacing, cellular automata).
Micro-synth presets shape ultra-short oscillator bursts (0.1ms–100ms).
25 rhythm presets × 4 voices = 100 total patterns. 25^4 = 390,625 combinations.

## Repo structure
- `src/dsp/signal.c` — all DSP logic (4 voices, sequencer, micro-synth, envelopes)
- `src/module.json` — module metadata and version (must match git tag on release)
- `scripts/build.sh` — Docker ARM64 cross-compile (always use this)
- `scripts/install.sh` — deploys to Move via scp + fixes ownership
- `.github/workflows/release.yml` — CI: verifies version, builds, releases, updates release.json

## Parameter pages (8 pages × 8 knobs, jog-wheel navigation)

### Page 1 — Séquences (root)
| Knob | Param | Range | Description |
|------|-------|-------|-------------|
| 1 | seq1 | 0-25 | Voice 1 rhythm preset (0=OFF) |
| 2 | seq2 | 0-25 | Voice 2 rhythm preset (0=OFF) |
| 3 | seq3 | 0-25 | Voice 3 rhythm preset (0=OFF) |
| 4 | seq4 | 0-25 | Voice 4 rhythm preset (0=OFF) |
| 5 | syn1 | 0-25 | Voice 1 synth preset (0=OFF) |
| 6 | syn2 | 0-25 | Voice 2 synth preset (0=OFF) |
| 7 | syn3 | 0-25 | Voice 3 synth preset (0=OFF) |
| 8 | syn4 | 0-25 | Voice 4 synth preset (0=OFF) |

### Page 2 — Mix
| Knob | Param | Range | Description |
|------|-------|-------|-------------|
| 1-4 | v1-4_level | 0-1 | Voice 1-4 output levels |
| 5-8 | v1-4_freq | 20-20000 Hz | Voice 1-4 base frequency (0.01 step, displayed in Hz) |

### Page 3 — Params
| Knob | Param | Range | Description |
|------|-------|-------|-------------|
| 1 | root | Free/C/C#/.../B | Root note (Free = no quantization) |
| 2 | scale | Free/Chromatic/.../HarmMin | Scale (Free = no quantization) |
| 3 | density | 0-1 | Global event density multiplier |
| 4 | chaos | 0-1 | Randomization of timing/density |
| 5 | gravity | 0-1 | Voice phase alignment (0=free, 1=locked) |
| 6 | clk_div | 1-32 | Master clock divider |
| 7 | morse_spd | 0-1 | Morse code element speed |
| 8 | swing | 0-1 | Timing swing on even steps |

### Pages 4-7 — Voix 1-4 (per-voice detail)
**Knobs (direct):**
| Knob | Param | Range | Description |
|------|-------|-------|-------------|
| 1 | vN_preset | 0-40 | Synth preset (mirrors page 1 synN) |
| 2 | vN_vol | 0-1 | Voice volume |
| 3 | vN_vfreq | 20-20000 Hz | Voice frequency |
| 4 | vN_wave | Sine/Impulse/Noise/Damped/Click/Square/Tri/FM/Pink/Brown/AM/Ping | Waveform |
| 5 | vN_tone | 0-1 | Brightness/tone color |
| 6 | vN_decay | 0.0001-0.5s | Microsound decay time |
| 7 | vN_detune | 0-20 Hz | Heterodyne frequency offset (beat frequency) |
| 8 | vN_pan | -1 to +1 | Stereo position |

**Menu (jog-wheel accessible):**
- **vN_attack** (0.0001-0.05s): Envelope attack / click sharpness
- **vN_sub_div** (1-8): Sub-harmonic divider per voice (affects pad velocity-based octave override)
- **vN_sweep** (0-1): Pitch envelope arc — peaks 2^sweep octaves up at attack
- **vN_tone_rnd** (0-1, Bretschneider): Per-impulse tone randomization depth. Each rhythm hit gets random tone offset ±100% × vN_tone_rnd. Creates spectral variation within patterns.

### Page 8 — General
| Knob | Param | Range | Description |
|------|-------|-------|-------------|
| 1 | master_vol | 0-1 | Master output volume |
| 2 | tempo_sync | Free/Sync | Clock mode |
| 3 | stereo_w | 0-1 | Stereo width |
| 4 | bit_crush | 1-16 bits | Bit depth reduction |
| 5 | drift | 0-1 | Analog-style frequency drift |
| 6 | jitter | 0-1 | Timing jitter amount |
| 7 | dc_filter | Off/On | DC offset filter |
| 8 | out_mode | Stereo/Mono/Spread | Output routing |

## Rhythm preset categories (per voice, 25 each)
- 1-5: Morse code sequences (SOS, CQ, QRZ, custom binary encodings)
- 6-10: Mathcore odd-meter bursts (7/8, 11/8, 13/16 feel)
- 11-15: Ikeda-style regular grids with dropout/stutter
- 16-20: Bernier-style sparse pings with prime-number spacing
- 21-25: Glitch/deterministic (pi digits, Rule 30, Rule 110)

## Synth preset categories (shared, 40 total)
- 0: OFF
- 1-5: Pure sine pips (Ikeda test tone, 0.2ms–20ms)
- 6-10: Damped sinusoids (Bernier tuning fork, tau=3ms–200ms)
- 11-13: Filtered noise bursts (0.1ms–18ms)
- 14-16: Click/percussive (WAVE_CLICK)
- 17-19: FM micro-textures (metallic/bell)
- 20-22: Pink/brown noise filtered
- 23-27: Various clicks, pings, CV tones
- 28-34: PolyBLEP anti-aliased waveforms (Square, Tri, Clean AM)
- 35-40: Data & specialized (Gauss Pings, Ping bursts, Data Quanta)

## Artist-inspired features
- **Ikeda (data sonification, frequencies)**: Rhythm patterns based on pi/e/phi binary, prime spacing, Rule 30/110 CA
- **Bernier (sparse pings, tuning fork)**: Damped waveform, sparse patterns, sub-div octave control
- **Bretschneider (click density variation, spectral scatter)**: vN_tone_rnd randomizes tone per impulse ±100%
- **Hainbach (heterodyne, detune, sweep)**: Detune (beat frequency 0–20Hz), Sweep (pitch envelope arc), Sub Div (harmonics)
- **Frank Bretschneider (impulse click synthesis)**: FM synthesis, impulse waveform, sample-and-hold modulation

## Per-impulse tone randomization (vN_tone_rnd)
Each rhythm trigger applies a random offset to the voice's base tone:
```
tone_eff = clamp(tone + rand(-1..+1) × 2 × tone_rnd, 0, 1)
```
- **0.0**: No randomization (deterministic tone per voice)
- **0.5**: Tone varies ±50% per hit (Bretschneider glitch scatter)
- **1.0**: Tone completely randomized per hit (spectral chaos)

This randomization is **seeded and pattern-repeatable** — the same rhythm pattern always generates the same sequence of random tones, enabling consistent polymetric variations.

## Critical constraints
- NEVER write to `/tmp` — use `/data/UserData/` on device
- NEVER allocate memory in `render_block` — all state lives in the instance struct
- NEVER call printf/log/mutex in `render_block`
- Output path: `modules/sound_generators/signal/` (not audio_fx!)
- Files on Move must be owned by `ableton:users` — `scripts/install.sh` handles this
- `release.json` is auto-updated by CI — never edit manually
- Git tag `vX.Y.Z` must match `version` in `src/module.json` exactly
- `get_param` MUST return -1 for unknown keys (not 0)
- Enum get_param must return STRING names, not integers

## Build & deploy
```bash
./scripts/build.sh          # Docker ARM64 cross-compile
./scripts/install.sh        # Deploy to move.local
```

## Release
Use the `/move-schwung-release` skill.

## License
GPL-3.0
