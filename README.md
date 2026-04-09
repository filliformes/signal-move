# Signal

Polymetric microsound rhythm generator for [Ableton Move](https://www.ableton.com/move/),
built for the [Schwung](https://github.com/charlesvestal/schwung) framework.

Inspired by Ryoji Ikeda, Nicolas Bernier (*frequencies*), The Dillinger Escape Plan, and Converge.

## Concept

4 independent voices, each combining a **rhythm engine** (glitchy pattern generator) with a **micro-synth** (ultra-short oscillator burst). 25 rhythm presets × 25 synth presets per voice = 390,625 unique combinations across all 4 voices.

### Rhythm preset categories
- **Morse code** sequences (SOS, CQ, QRZ, binary encodings)
- **Mathcore** odd-meter bursts (7/8, 11/8, 13/16)
- **Ikeda-style** regular grids with dropout and stutter
- **Bernier-style** sparse pings at prime-number intervals
- **Glitch** patterns from mathematical constants and cellular automata

### Micro-synth categories
- Pure sine pips, damped sinusoids, noise bursts, FM textures, digital glitches

## Controls

| Page | Knobs 1-4 | Knobs 5-8 |
|------|-----------|-----------|
| Séquences | Rhythm presets V1-4 | Synth presets V1-4 |
| Mix | Levels V1-4 | Frequencies V1-4 (Hz) |
| Params | Root, Scale, Density, Chaos | Gravity, ClkDiv, MorseSpd, Swing |
| Voix 1-4 | Vol, Freq, Preset, Wave | Decay, Tone, Pan, Attack |
| General | MasterVol, Sync, StereoW, BitCrush | Drift, Jitter, DC Filt, OutMode |

## Building

```
./scripts/build.sh
```

Requires Docker or an `aarch64-linux-gnu-gcc` cross-compiler.

## Installation

```
./scripts/install.sh
```

Or install via the Module Store in Schwung.

## Credits

fillioning — concept, design, DSP

## License

GPL-3.0 — see [LICENSE](LICENSE)
