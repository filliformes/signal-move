/**
 * Signal — Polymetric microsound rhythm generator
 * Author: fillioning  |  License: GPL-3.0
 *
 * Inspired by Ryoji Ikeda, Nicolas Bernier (frequencies/sound quanta),
 * The Dillinger Escape Plan, and Converge.
 *
 * Architecture: 4 independent self-sequencing micro-synth voices.
 * Each voice = rhythm engine (one of 25 presets) + micro-synth (one of 25 presets).
 * 25^4 = 390,625 unique rhythmic combinations.
 *
 * API: plugin_api_v2_t, 44100 Hz, 128 frames/block, stereo int16 output.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#define SAMPLE_RATE   44100.0f
#define SR_INV        (1.0f / 44100.0f)
#define TWO_PI        6.283185307f
#define NUM_VOICES    4
#define NUM_SEQ_PRESETS 25
#define NUM_SYN_PRESETS 40

/* ── Schwung API typedefs ──────────────────────────────────────────────────── */
typedef struct {
    uint32_t api_version;
    void* (*create_instance)(const char *, const char *);
    void  (*destroy_instance)(void *);
    void  (*on_midi)(void *, const uint8_t *, int, int);
    void  (*set_param)(void *, const char *, const char *);
    int   (*get_param)(void *, const char *, char *, int);
    int   (*get_error)(void *, char *, int);
    void  (*render_block)(void *, int16_t *, int);
} plugin_api_v2_t;

/* ── Enumerations ─────────────────────────────────────────────────────────── */
enum { WAVE_SINE=0, WAVE_IMPULSE, WAVE_NOISE, WAVE_DAMPED,
       WAVE_CLICK, WAVE_SQUARE, WAVE_TRI, WAVE_FM,
       WAVE_PINK, WAVE_BROWN, WAVE_AM, WAVE_COUNT };
static const char *WAVE_NAMES[] = {
    "Sine","Impulse","Noise","Damped","Click","Square","Tri","FM",
    "Pink","Brown","AM"
};

enum { ROOT_FREE=0, ROOT_C, ROOT_CS, ROOT_D, ROOT_DS, ROOT_E,
       ROOT_F, ROOT_FS, ROOT_G, ROOT_GS, ROOT_A, ROOT_AS, ROOT_B, ROOT_COUNT };
static const char *ROOT_NAMES[] = {
    "Free","C","C#","D","D#","E","F","F#","G","G#","A","A#","B"
};

enum { SCALE_FREE=0, SCALE_CHROMATIC, SCALE_MAJOR, SCALE_MINOR,
       SCALE_PENTATONIC, SCALE_WHOLETONE, SCALE_DIMINISHED, SCALE_HARM_MIN,
       SCALE_COUNT };
static const char *SCALE_NAMES[] = {
    "Free","Chromatic","Major","Minor","Pentatonic","WholeTone","Diminished","HarmMin"
};

enum { PAGE_ROOT=0, PAGE_GENERATE, PAGE_PARAMS, PAGE_MODULATION,
       PAGE_PATCH, PAGE_MIX, PAGE_VOICE1, PAGE_VOICE2, PAGE_VOICE3, PAGE_VOICE4, PAGE_GENERAL };
static const char *PAGE_NAMES[] = {
    "Signal","generate","params","modulation","patch","mix",
    "voice1","voice2","voice3","voice4","general"
};

/* ── Rhythm pattern data ──────────────────────────────────────────────────── */
/*
 * subdivision = notes per beat (1=quarter, 2=8th, 4=16th, 8=32nd, 16=64th)
 * Morse dit=1 step, dah=3 steps, inter-element=1, inter-char=3, inter-word=7
 * At sub=8 and 120 BPM: 1 step = 62.5 ms (fast Morse, fine for rhythm)
 */
typedef struct {
    const uint8_t *hits;
    int            length;
    int            subdivision; /* notes per beat */
} rhythm_pattern_t;

/* ─── Voice 0: Morse focus + mathcore 7-feel + Ikeda-3 + prime sparse + CA ─ */
/* Morse SOS (sub=8) */
static const uint8_t V0P00[] = {1,0,1,0,1,0,0,0,1,1,1,0,1,1,1,0,1,1,1,0,0,0,1,0,1,0,1,0,0,0,0,0};
/* Morse DE (sub=8) */
static const uint8_t V0P01[] = {1,1,1,0,1,0,1,0,0,1,0,0,0,0,0,0};
/* Morse CQ (sub=8) */
static const uint8_t V0P02[] = {1,1,1,0,1,0,1,1,1,0,1,0,0,1,1,1,0,1,1,1,0,1,0,1,1,1,0,0,0,0,0,0};
/* Morse K / invitation (sub=8) */
static const uint8_t V0P03[] = {1,1,1,0,1,0,1,1,1,0,0,0,0,0,0,0};
/* Morse AR / end of message (sub=8) */
static const uint8_t V0P04[] = {1,0,1,1,1,0,1,0,1,0,1,1,1,0,0,0};
/* Mathcore [3+2+2] 7-feel (sub=4) */
static const uint8_t V0P05[] = {1,0,0,1,0,1,0};
/* Mathcore [2+3+2] 7-feel variant (sub=4) */
static const uint8_t V0P06[] = {1,0,1,0,0,1,0};
/* Mathcore [5+3+3+4] 15-feel (sub=4) */
static const uint8_t V0P07[] = {1,0,0,0,0,1,0,0,1,0,0,1,0,0,0};
/* Mathcore [4+7] 11-feel (sub=4) */
static const uint8_t V0P08[] = {1,0,0,0,1,0,0,0,0,0,0};
/* Mathcore [5+4+4] 13-feel (sub=4) */
static const uint8_t V0P09[] = {1,0,0,0,0,1,0,0,0,1,0,0,0};
/* Ikeda every-3-dropout 32nd grid (sub=8) */
static const uint8_t V0P10[] = {1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0};
/* Ikeda alternating pairs (sub=8) */
static const uint8_t V0P11[] = {1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0};
/* Ikeda dense burst then silence (sub=8) */
static const uint8_t V0P12[] = {1,1,1,1,0,0,0,0,1,1,1,0,0,0,0,0};
/* Ikeda triplet 32nds (sub=8) */
static const uint8_t V0P13[] = {1,0,0,1,0,0,1,0,0,1,0,0,1,0,0,1,0,0,1,0,0,1,0,0};
/* Ikeda prime-position dropout (sub=8) — rests at prime steps 2,3,5,7,11,13 */
static const uint8_t V0P14[] = {1,1,0,0,1,0,1,0,1,1,1,0,1,0,1,1,1,1,1,0,1,1,1,0,1,1,1,1,1,0,1,0};
/* Bernier primes [2,3,5,7,11] (sub=2) */
static const uint8_t V0P15[] = {1,0,1,0,0,1,0,0,0,0,1,0,1,0,0,0};
/* Bernier Fibonacci [1,2,3,5,8,13] (sub=2) */
static const uint8_t V0P16[] = {1,1,0,1,0,0,1,0,0,0,0,1,0,0,0,0};
/* Bernier single ping 7-step (sub=2) */
static const uint8_t V0P17[] = {1,0,0,0,0,0,0};
/* Bernier two pings 11-step (sub=2) */
static const uint8_t V0P18[] = {1,0,0,0,0,0,1,0,0,0,0};
/* Bernier ultra-sparse quarter notes (sub=1) */
static const uint8_t V0P19[] = {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
/* Pi binary fraction (sub=4) */
static const uint8_t V0P20[] = {1,0,0,1,0,0,0,0,1,1,1,1,1,1,0,0};
/* e binary fraction (sub=4) */
static const uint8_t V0P21[] = {1,0,1,1,0,1,1,1,1,1,1,0,0,0,0,1};
/* Phi binary fraction (sub=4) */
static const uint8_t V0P22[] = {1,0,0,1,1,1,1,0,0,0,1,1,0,1,1,1};
/* Rule 30 CA sequence (sub=4) */
static const uint8_t V0P23[] = {1,1,0,1,1,0,1,0,1,1,1,0,1,0,1,0};
/* Rule 110 CA sequence (sub=4) */
static const uint8_t V0P24[] = {1,1,0,1,1,1,0,0,1,0,1,1,1,1,0,0};

/* ─── Voice 1: Morse QRZ + mathcore 11-feel + Ikeda burst + different sparse ─ */
/* Morse QR (sub=8) */
static const uint8_t V1P00[] = {1,1,1,0,1,1,1,0,1,0,1,1,1,0,0,1,0,1,1,1,0,1,0,0};
/* Morse 73 best regards (sub=8) */
static const uint8_t V1P01[] = {1,1,1,0,1,1,1,0,1,0,1,0,1,0,0,1,0,1,0,1,0,1,1,1,0,1,1,1,0,0,0,0};
/* Morse RST (sub=8) */
static const uint8_t V1P02[] = {1,0,1,1,1,0,1,0,0,1,0,1,0,1,0,0,1,1,1,0,0,0,0,0};
/* Morse PSE (please) (sub=8) */
static const uint8_t V1P03[] = {1,0,1,1,1,0,1,1,1,0,1,0,0,1,0,1,0,1,0,0,1,0,0,0};
/* Morse HH (error) — 8 dits (sub=8) */
static const uint8_t V1P04[] = {1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0};
/* Mathcore [5+3] 8-feel (sub=4) */
static const uint8_t V1P05[] = {1,0,0,0,0,1,0,0};
/* Mathcore [3+4] 7-feel (sub=4) */
static const uint8_t V1P06[] = {1,0,0,1,0,0,0};
/* Mathcore [3+4+4] 11-feel (sub=4) */
static const uint8_t V1P07[] = {1,0,0,1,0,0,0,1,0,0,0};
/* Mathcore [5+4+4] 13-feel (sub=4) */
static const uint8_t V1P08[] = {1,0,0,0,0,1,0,0,0,1,0,0,0};
/* Mathcore [5+7+5] 17-feel (sub=4) */
static const uint8_t V1P09[] = {1,0,0,0,0,1,0,0,0,0,0,0,1,0,0,0,0};
/* Ikeda 64th note grid (sub=16) */
static const uint8_t V1P10[] = {1,0,1,0,1,0,1,0};
/* Ikeda 32nd burst groups (sub=8) */
static const uint8_t V1P11[] = {1,1,1,0,0,0,0,0,1,1,1,0,0,0,0,0,1,1,1,0,0,0,0,0,1,1,1,0,0,0,0,0};
/* Ikeda alternating 1+2 (sub=8) */
static const uint8_t V1P12[] = {1,0,1,1,0,0,1,0,1,1,0,0,1,0,1,1,0,0,1,0,1,1,0,0};
/* Ikeda quintuplet (sub=8) */
static const uint8_t V1P13[] = {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0};
/* Ikeda stutter build (sub=8) */
static const uint8_t V1P14[] = {1,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,1,0,0,0,0,0,1,1,1,1,0,0,0,0};
/* Bernier [3,5,7,11] primes (sub=2) */
static const uint8_t V1P15[] = {1,0,0,1,0,0,0,0,1,0,0,0,0,0,0,1};
/* Bernier 17-step cascade (sub=2) */
static const uint8_t V1P16[] = {1,0,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0};
/* Bernier 11-step single (sub=2) */
static const uint8_t V1P17[] = {1,0,0,0,0,0,0,0,0,0,0};
/* Bernier 13-step two pings (sub=2) */
static const uint8_t V1P18[] = {1,0,0,0,0,0,0,1,0,0,0,0,0};
/* Bernier ultra-slow 8-step (sub=1) */
static const uint8_t V1P19[] = {1,0,0,0,0,0,0,0};
/* Sqrt(2) binary fraction (sub=4) */
static const uint8_t V1P20[] = {1,1,0,1,0,1,0,0,0,0,0,1,0,0,1,0};
/* Sqrt(3) binary fraction (sub=4) */
static const uint8_t V1P21[] = {1,0,1,1,1,0,1,1,0,0,0,0,1,0,1,1};
/* ln(2) binary fraction (sub=4) */
static const uint8_t V1P22[] = {1,0,1,1,0,0,0,1,0,1,1,1,0,0,1,0};
/* Rule 30 shifted (sub=4) */
static const uint8_t V1P23[] = {0,1,1,0,1,0,1,0,1,1,1,0,1,0,1,0};
/* Rule 90 XOR pattern (sub=4) */
static const uint8_t V1P24[] = {1,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1};

/* ─── Voice 2: Morse prosigns + mathcore + Ikeda density + Bernier + chaos ── */
/* Morse VVV (attention) (sub=8) */
static const uint8_t V2P00[] = {1,0,1,0,1,0,1,1,1,0,1,0,1,0,1,0,1,1,1,0,1,0,1,0,1,1,1,0,0,0,0,0};
/* Morse SK end-of-contact prosign (sub=8) */
static const uint8_t V2P01[] = {1,0,1,0,1,0,1,1,1,0,1,0,1,1,1,0,0,0,0,0,0,0,0,0};
/* Morse BK break (sub=8) */
static const uint8_t V2P02[] = {1,1,1,0,1,0,1,0,1,0,0,1,1,1,0,1,0,1,1,1,0,0,0,0};
/* Morse BT paragraph separator (sub=8) */
static const uint8_t V2P03[] = {1,1,1,0,1,0,1,0,1,0,1,1,1,0,0,0};
/* Morse SOS backward (sub=8) — ...---... reversed */
static const uint8_t V2P04[] = {0,0,0,0,1,0,1,0,1,0,0,0,1,1,1,0,1,1,1,0,1,1,1,0,0,0,1,0,1,0,1,0};
/* Mathcore [3+2+3+2] 10-feel (sub=4) */
static const uint8_t V2P05[] = {1,0,0,1,0,1,0,0,1,0};
/* Mathcore [4+3+4] 11-feel (sub=4) */
static const uint8_t V2P06[] = {1,0,0,0,1,0,0,1,0,0,0};
/* Mathcore [4+4+5] 13-feel (sub=4) */
static const uint8_t V2P07[] = {1,0,0,0,1,0,0,0,1,0,0,0,0};
/* Mathcore [7+6+4] 17-feel (sub=4) */
static const uint8_t V2P08[] = {1,0,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0};
/* Mathcore [5+5+5+4] 19-feel (sub=4) */
static const uint8_t V2P09[] = {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0};
/* Ikeda full 32nd density (sub=8) */
static const uint8_t V2P10[] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
/* Ikeda 3-on 3-off (sub=8) */
static const uint8_t V2P11[] = {1,1,1,0,0,0,1,1,1,0,0,0,1,1,1,0,0,0,1,1,1,0,0,0};
/* Ikeda gallop 64th (sub=16) */
static const uint8_t V2P12[] = {1,1,0,1,1,0,1,0};
/* Ikeda stutter accelerando (sub=8) */
static const uint8_t V2P13[] = {1,0,0,0,0,0,1,0,0,0,0,1,0,0,0,1,0,0,1,0,1,0,1,1};
/* Ikeda hocket (sub=8) */
static const uint8_t V2P14[] = {1,0,0,1,0,1,0,0,1,0,0,1,0,1,0,0};
/* Bernier 31-step prime (sub=2) */
static const uint8_t V2P15[] = {1,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0};
/* Bernier 13-step cascade (sub=2) */
static const uint8_t V2P16[] = {1,0,1,0,0,0,0,0,0,1,0,0,0};
/* Bernier 13-step single (sub=2) */
static const uint8_t V2P17[] = {1,0,0,0,0,0,0,0,0,0,0,0,0};
/* Bernier 16-step triple (sub=2) */
static const uint8_t V2P18[] = {1,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0};
/* Bernier 3 pings quarter notes (sub=1) */
static const uint8_t V2P19[] = {1,0,0,0,0,1,0,0,0,0,0,0,1,0,0,0};
/* Logistic map r=3.8 (sub=4) */
static const uint8_t V2P20[] = {1,0,0,1,1,0,1,0,0,1,0,1,1,0,0,1};
/* Rule 60 CA (sub=4) */
static const uint8_t V2P21[] = {1,1,1,0,1,1,1,0,1,1,1,0,1,1,1,0};
/* Rule 90 pure alternating (sub=4) */
static const uint8_t V2P22[] = {1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0};
/* Cantor dust (sub=4, length=27) */
static const uint8_t V2P23[] = {1,0,0,1,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,0,0,1,0,0};
/* Stern-Brocot self-similar (sub=4) */
static const uint8_t V2P24[] = {1,1,0,1,0,1,0,0,1,1,0,1,0,1,0,0};

/* ─── Voice 3: Morse short IDs + mathcore prime lengths + Ikeda + Bernier min ─ */
/* Morse UR (sub=8) */
static const uint8_t V3P00[] = {1,0,1,0,1,1,1,0,0,1,0,1,1,1,0,1,0,0,0,0,0,0,0,0};
/* Morse NW (sub=8) */
static const uint8_t V3P01[] = {1,1,1,0,1,0,0,1,0,1,1,1,0,1,1,1,0,0,0,0,0,0,0,0};
/* Morse TU (sub=8) */
static const uint8_t V3P02[] = {1,1,1,0,0,1,0,1,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0};
/* Morse RR (repeat) (sub=8) */
static const uint8_t V3P03[] = {1,0,1,1,1,0,1,0,0,1,0,1,1,1,0,1,0,0,0,0,0,0,0,0};
/* Morse RIG (rigid) (sub=8) */
static const uint8_t V3P04[] = {1,0,1,1,1,0,1,0,0,1,0,1,0,0,1,1,1,0,1,1,1,0,1,0,0,0,0,0,0,0,0,0};
/* Mathcore [2+2+3] 7-feel (sub=4) */
static const uint8_t V3P05[] = {1,0,1,0,1,0,0};
/* Mathcore [6+5] 11-feel (sub=4) */
static const uint8_t V3P06[] = {1,0,0,0,0,0,1,0,0,0,0};
/* Mathcore [6+7] 13-feel (sub=4) */
static const uint8_t V3P07[] = {1,0,0,0,0,0,1,0,0,0,0,0,0};
/* Mathcore [9+8] 17-feel (sub=4) */
static const uint8_t V3P08[] = {1,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0};
/* Mathcore [7+12] 19-feel (sub=4) */
static const uint8_t V3P09[] = {1,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0};
/* Ikeda triplet 32nds (sub=8) — same as V0 but different length */
static const uint8_t V3P10[] = {1,0,0,1,0,0,1,0,0,1,0,0,1,0,0,1,0,0,1,0,0,1,0,0};
/* Ikeda every 5 (sub=8) */
static const uint8_t V3P11[] = {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0};
/* Ikeda accelerando (sub=8) */
static const uint8_t V3P12[] = {1,0,0,0,0,0,1,0,0,0,0,1,0,0,0,1,0,0,1,0,1,0,1,1};
/* Ikeda syncopated 16ths (sub=4) */
static const uint8_t V3P13[] = {1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0};
/* Ikeda XOR grid (sub=8) */
static const uint8_t V3P14[] = {1,0,1,0,0,1,0,1,1,0,1,0,0,1,0,1};
/* Bernier 16-step primes mod (sub=2) */
static const uint8_t V3P15[] = {1,1,0,1,0,1,0,0,0,1,0,1,0,0,1,0};
/* Bernier near-asymmetric (sub=2) */
static const uint8_t V3P16[] = {1,0,0,0,1,0,0,0,0,1,0,0,0,0,0,0};
/* Bernier minimal 3-step (sub=2) */
static const uint8_t V3P17[] = {1,0,0};
/* Bernier 17-step single (sub=2) */
static const uint8_t V3P18[] = {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
/* Bernier 16-step two pings quarter notes (sub=1) */
static const uint8_t V3P19[] = {1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0};
/* Thue-Morse sequence (sub=4) */
static const uint8_t V3P20[] = {1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1};
/* Fibonacci word (sub=4) */
static const uint8_t V3P21[] = {1,0,1,1,0,1,0,1,1,0,1,1,0,1,0,1};
/* Regular paperfolding (sub=4) */
static const uint8_t V3P22[] = {1,1,0,1,1,0,0,1,1,1,0,0,1,0,0,1};
/* Kolakoski sequence (sub=4) */
static const uint8_t V3P23[] = {1,0,0,1,1,0,1,0,0,1,0,0,1,1,0,1};
/* pi^2 binary fraction (sub=4) */
static const uint8_t V3P24[] = {1,0,1,0,0,1,1,0,1,1,1,0,0,1,1,1};

/* ── Pattern lookup table [4 voices][25 presets] ─────────────────────────── */
static const rhythm_pattern_t PATTERNS[NUM_VOICES][NUM_SEQ_PRESETS] = {
    { /* Voice 0 */
        {V0P00,32,8},{V0P01,16,8},{V0P02,32,8},{V0P03,16,8},{V0P04,16,8},
        {V0P05, 7,4},{V0P06, 7,4},{V0P07,15,4},{V0P08,11,4},{V0P09,13,4},
        {V0P10,24,8},{V0P11,16,8},{V0P12,16,8},{V0P13,24,8},{V0P14,32,8},
        {V0P15,16,2},{V0P16,16,2},{V0P17, 7,2},{V0P18,11,2},{V0P19,16,1},
        {V0P20,16,4},{V0P21,16,4},{V0P22,16,4},{V0P23,16,4},{V0P24,16,4}
    },
    { /* Voice 1 */
        {V1P00,24,8},{V1P01,32,8},{V1P02,24,8},{V1P03,24,8},{V1P04,16,8},
        {V1P05, 8,4},{V1P06, 7,4},{V1P07,11,4},{V1P08,13,4},{V1P09,17,4},
        {V1P10, 8,16},{V1P11,32,8},{V1P12,24,8},{V1P13,20,8},{V1P14,32,8},
        {V1P15,16,2},{V1P16,17,2},{V1P17,11,2},{V1P18,13,2},{V1P19, 8,1},
        {V1P20,16,4},{V1P21,16,4},{V1P22,16,4},{V1P23,16,4},{V1P24,16,4}
    },
    { /* Voice 2 */
        {V2P00,32,8},{V2P01,24,8},{V2P02,24,8},{V2P03,16,8},{V2P04,32,8},
        {V2P05,10,4},{V2P06,11,4},{V2P07,13,4},{V2P08,17,4},{V2P09,19,4},
        {V2P10,16,8},{V2P11,24,8},{V2P12, 8,16},{V2P13,24,8},{V2P14,16,8},
        {V2P15,31,2},{V2P16,13,2},{V2P17,13,2},{V2P18,16,2},{V2P19,16,1},
        {V2P20,16,4},{V2P21,16,4},{V2P22,16,4},{V2P23,27,4},{V2P24,16,4}
    },
    { /* Voice 3 */
        {V3P00,24,8},{V3P01,24,8},{V3P02,24,8},{V3P03,24,8},{V3P04,32,8},
        {V3P05, 7,4},{V3P06,11,4},{V3P07,13,4},{V3P08,17,4},{V3P09,19,4},
        {V3P10,24,8},{V3P11,20,8},{V3P12,24,8},{V3P13,16,4},{V3P14,16,8},
        {V3P15,16,2},{V3P16,16,2},{V3P17, 3,2},{V3P18,17,2},{V3P19,16,1},
        {V3P20,16,4},{V3P21,16,4},{V3P22,16,4},{V3P23,16,4},{V3P24,16,4}
    }
};

/* ── Synth preset data ────────────────────────────────────────────────────── */
typedef struct {
    int   wave;
    float attack;    /* seconds */
    float decay;     /* seconds */
    float tone;      /* brightness 0–1 */
    float fm_ratio;  /* FM mod/car ratio */
    float fm_depth;  /* FM index */
} synth_preset_t;

/* 40 shared micro-synth presets — all 11 waveforms covered */
static const synth_preset_t SYN_PRESETS[NUM_SYN_PRESETS] = {
    /* 0-4: Pure sine pips — Ikeda test tone */
    {WAVE_SINE,    0.0001f, 0.001f,  1.0f, 0,   0},
    {WAVE_SINE,    0.0001f, 0.003f,  0.9f, 0,   0},
    {WAVE_SINE,    0.0001f, 0.008f,  0.8f, 0,   0},
    {WAVE_SINE,    0.0002f, 0.020f,  0.7f, 0,   0},
    {WAVE_SINE,    0.0005f, 0.050f,  0.6f, 0,   0},
    /* 5-9: Damped sinusoids — Bernier tuning fork */
    {WAVE_DAMPED,  0.0001f, 0.008f,  0.1f, 0,   0},
    {WAVE_DAMPED,  0.0001f, 0.020f,  0.2f, 0,   0},
    {WAVE_DAMPED,  0.0001f, 0.040f,  0.4f, 0,   0},
    {WAVE_DAMPED,  0.0001f, 0.100f,  0.6f, 0,   0},
    {WAVE_DAMPED,  0.0001f, 0.250f,  0.9f, 0,   0},
    /* 10-12: Noise bursts */
    {WAVE_NOISE,   0.0001f, 0.001f,  0.5f, 0,   0},
    {WAVE_NOISE,   0.0001f, 0.005f,  0.4f, 0,   0},
    {WAVE_NOISE,   0.0001f, 0.015f,  0.3f, 0,   0},
    /* 13-14: Click / Impulse transients */
    {WAVE_CLICK,   0.0001f, 0.0005f, 1.0f, 0,   0},
    {WAVE_IMPULSE, 0.0001f, 0.002f,  0.9f, 0,   0},
    /* 15-19: FM micro-textures — metallic/bell */
    {WAVE_FM, 0.0002f, 0.005f, 0.5f, 1.0f, 0.5f},
    {WAVE_FM, 0.0002f, 0.008f, 0.6f, 1.5f, 1.2f},
    {WAVE_FM, 0.0002f, 0.015f, 0.7f, 2.0f, 2.0f},
    {WAVE_FM, 0.0002f, 0.025f, 0.8f, 3.0f, 3.0f},
    {WAVE_FM, 0.0005f, 0.040f, 0.9f, 5.0f, 4.5f},
    /* 20-22: Square / Triangle glitches */
    {WAVE_SQUARE, 0.0001f, 0.0005f, 0.9f, 0,   0},
    {WAVE_SQUARE, 0.0001f, 0.003f,  0.8f, 0,   0},
    {WAVE_TRI,    0.0001f, 0.004f,  0.7f, 0,   0},
    /* 23-25: Pink noise bursts */
    {WAVE_PINK,   0.0001f, 0.003f,  0.8f, 0,   0},
    {WAVE_PINK,   0.0001f, 0.010f,  0.6f, 0,   0},
    {WAVE_PINK,   0.0002f, 0.030f,  0.4f, 0,   0},
    /* 26-27: Brown noise — warm */
    {WAVE_BROWN,  0.0001f, 0.006f,  0.7f, 0,   0},
    {WAVE_BROWN,  0.0002f, 0.025f,  0.5f, 0,   0},
    /* 28-32: AM textures — tone→AM rate 100–5000 Hz */
    {WAVE_AM, 0.0002f, 0.006f,  0.05f, 0,  0},  /* fast AM ~350Hz */
    {WAVE_AM, 0.0002f, 0.008f,  0.2f,  0,  0},  /* ~1k AM */
    {WAVE_AM, 0.0002f, 0.012f,  0.4f,  0,  0},  /* ~2k AM */
    {WAVE_AM, 0.0003f, 0.018f,  0.6f,  0,  0},  /* ~3k AM */
    {WAVE_AM, 0.0005f, 0.030f,  0.9f,  0,  0},  /* ~4.6k AM very metallic */
    /* 33-36: Special / long */
    {WAVE_DAMPED,  0.0001f, 0.350f, 0.95f, 0,   0},  /* very long bell */
    {WAVE_FM,      0.0001f, 0.003f, 0.3f, 7.0f, 6.0f}, /* extreme FM crunch */
    {WAVE_SINE,    0.0001f, 0.100f, 1.0f, 0,   0},  /* long pure sine */
    {WAVE_NOISE,   0.0002f, 0.080f, 0.2f, 0,   0},  /* long filtered noise */
    /* 37-39: Ultra-short transients */
    {WAVE_CLICK,   0.0001f, 0.0002f, 1.0f, 0,  0},  /* sharpest click */
    {WAVE_IMPULSE, 0.0001f, 0.0003f, 1.0f, 0,  0},  /* sharpest impulse */
    {WAVE_SQUARE,  0.0001f, 0.0004f, 1.0f, 0,  0},  /* digital spike */
};

/* ── Scale quantization ───────────────────────────────────────────────────── */
/* Semitone intervals from root for each scale (0=root ... 11=major7th) */
static const int SCALE_INTERVALS[SCALE_COUNT][12] = {
    {0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},  /* FREE  — unused */
    {0,1,2,3,4,5,6,7,8,9,10,11},           /* CHROMATIC */
    {0,2,4,5,7,9,11,-1,-1,-1,-1,-1},       /* MAJOR */
    {0,2,3,5,7,8,10,-1,-1,-1,-1,-1},       /* MINOR */
    {0,2,4,7,9,-1,-1,-1,-1,-1,-1,-1},      /* PENTATONIC */
    {0,2,4,6,8,10,-1,-1,-1,-1,-1,-1},      /* WHOLETONE */
    {0,2,3,5,6,8,9,11,-1,-1,-1,-1},        /* DIMINISHED */
    {0,2,3,5,7,8,11,-1,-1,-1,-1,-1}        /* HARM_MIN */
};
static const int SCALE_NOTES[SCALE_COUNT] = {0,12,7,7,5,6,8,7};

static float quantize_freq(float freq, int root, int scale) {
    if (root == ROOT_FREE || scale == SCALE_FREE) return freq;
    if (freq < 16.0f) return freq;

    /* MIDI note number (float) */
    float midi = 12.0f * log2f(freq / 440.0f) + 69.0f;
    int midi_i = (int)roundf(midi);

    /* root semitone: ROOT_C=1 → 0, ROOT_CS=2 → 1, ... */
    int root_st = (root - 1) % 12;

    /* Distance of current pitch from root, mod 12 */
    int dist = ((midi_i - root_st) % 12 + 12) % 12;

    /* Find nearest scale degree (with octave wrapping) */
    const int *ivals = SCALE_INTERVALS[scale];
    int n = SCALE_NOTES[scale];
    int best_diff = 13, best_delta = 0;
    for (int i = 0; i < n; i++) {
        if (ivals[i] < 0) break;
        int d = dist - ivals[i];
        /* wrap to -6..+6 */
        if (d >  6) d -= 12;
        if (d < -6) d += 12;
        int ad = d < 0 ? -d : d;
        if (ad < best_diff) {
            best_diff = ad;
            best_delta = ivals[i] - dist; /* how many semitones to shift */
        }
    }
    float q_midi = (float)(midi_i + best_delta);
    return 440.0f * powf(2.0f, (q_midi - 69.0f) / 12.0f);
}

/* ── Voice state ──────────────────────────────────────────────────────────── */
typedef struct {
    /* Sequencer */
    int    seq_preset;      /* 0=OFF, 1–25 */
    int    syn_preset;      /* 0=OFF, 1–25 */
    int    step;
    float  step_accum;      /* sample accumulator for free-running clock */
    float  tick_accum;      /* tick accumulator for MIDI clock */

    /* Envelope */
    float  env;
    int    env_stage;       /* 0=attack, 1=decay, -1=idle */

    /* Oscillator */
    float  phase;           /* 0–1 */
    float  fm_phase;        /* FM modulator phase */
    uint32_t noise_state;

    /* Tone LP filter state + 20ms smoothed tone coefficient */
    float  lp_state;
    float  tone_smooth;     /* 20ms-smoothed tone for click-free LP coeff */
    float  vel;             /* pad velocity scale 0–1 (1.0 for sequencer hits) */

    /* Noise LP filter state (cutoff = freq, applied to Noise/Pink/Brown) */
    float  noise_lp;

    /* Heterodyne second oscillator */
    float  phase2;

    /* Hainbach features */
    float  sweep;           /* pitch arc depth 0–1 (peaks 2^sweep octaves up at env=1) */
    float  detune;          /* heterodyne Hz offset 0–20 Hz */
    int    sub_div;         /* sub-harmonic divider 1–8 */

    /* Colored noise state */
    float  pink_b[7];       /* Paul Kellett 7-stage pink noise filter */
    float  brown_last;      /* brown noise integrator */

    /* Per-voice params (pages 4–7 and Mix) */
    float  vol;
    float  freq;            /* Hz — shared by v_freq (Mix) and v_vfreq (Voice) */
    float  level;           /* Mix page level */
    int    wave;
    float  attack;
    float  decay;
    float  tone;
    float  pan;
} voice_t;

/* ── Instance state ───────────────────────────────────────────────────────── */
typedef struct {
    voice_t voices[NUM_VOICES];

    /* Page 3 — Params */
    int    root;
    int    scale;
    float  density;
    float  chaos;
    float  gravity;
    int    clk_div;
    float  morse_spd;
    float  swing;

    /* Page 8 — General */
    float  master_vol;
    int    tempo_sync;      /* 0=Free, 1=Sync */
    float  stereo_w;
    int    bit_crush;
    float  bit_rate;        /* 0=off, 1=max decimation (sample-rate reduction) */
    float  drift;
    float  jitter;
    int    dc_filter;
    int    out_mode;

    /* Bit-rate decimation state */
    float  bit_rate_accum;
    float  bit_rate_hold_l;
    float  bit_rate_hold_r;

    /* Modulation LFO */
    float    mod_amount;     /* 0–1 global LFO depth multiplier */
    float    mod_speed;      /* 0–1 → 0–10 Hz */
    float    mod_offset;     /* 0–1 phase stagger between voices */
    float    mod_freq;       /* freq mod depth */
    float    mod_decay;      /* decay mod depth */
    float    mod_pan;        /* pan mod depth */
    float    mod_density;    /* density (trigger probability) mod depth */
    int      mod_shape;      /* 0=Sine 1=Tri 2=Saw 3=Square 4=S&H 5=Random */
    float    vlfo_phase[NUM_VOICES];
    float    vlfo_sh_val[NUM_VOICES];
    uint32_t vlfo_rng[NUM_VOICES];
    float    vlfo_value[NUM_VOICES];
    int      vlfo_shape[NUM_VOICES];

    /* Clock/transport */
    float    bpm;           /* host-provided or default */
    int      midi_running;  /* MIDI transport running */

    /* DC blocker state (per channel) */
    float  dc_x[2];

    /* PRNG for density/chaos/jitter */
    uint32_t rng;

    /* Current UI page (for knob overlay) */
    int    current_page;

    /* Current patch index (0-29) */
    int    patch_idx;
} signal_instance_t;

/* ── Helpers ──────────────────────────────────────────────────────────────── */
static inline float clampf(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

static inline uint32_t xorshift32(uint32_t *s) {
    uint32_t x = *s;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    *s = x;
    return x;
}

static inline float randf(uint32_t *s) {
    return (float)(xorshift32(s) & 0xFFFF) / 65535.0f;
}

/* ── LFO shapes ───────────────────────────────────────────────────────────── */
#define MOD_SHAPE_SINE    0
#define MOD_SHAPE_TRI     1
#define MOD_SHAPE_SAW     2
#define MOD_SHAPE_SQUARE  3
#define MOD_SHAPE_SH      4
#define MOD_SHAPE_RANDOM  5
#define MOD_NUM_SHAPES    6
static const char *MOD_SHAPE_NAMES[] = {"Sine","Tri","Saw","Square","S&H","Random"};

static float mod_lfo_value(float phase, int shape,
                            float *sh_val, uint32_t *rng) {
    switch (shape) {
        case MOD_SHAPE_SINE:
            return sinf(phase * TWO_PI);
        case MOD_SHAPE_TRI:
            return (phase < 0.5f) ? (4.0f*phase - 1.0f) : (3.0f - 4.0f*phase);
        case MOD_SHAPE_SAW:
            return 2.0f * phase - 1.0f;
        case MOD_SHAPE_SQUARE:
            return (phase < 0.5f) ? 1.0f : -1.0f;
        case MOD_SHAPE_SH:
            return *sh_val; /* value sampled on zero-crossing, held until next */
        case MOD_SHAPE_RANDOM: {
            uint32_t x = *rng;
            x = x * 1664525u + 1013904223u;
            *rng = x;
            return (float)(int32_t)x / 2147483648.0f;
        }
        default: return 0.0f;
    }
}

/* ── Voice helpers ────────────────────────────────────────────────────────── */
static void voice_apply_synth_preset(voice_t *v, int preset_idx) {
    if (preset_idx < 0 || preset_idx >= NUM_SYN_PRESETS) return;
    const synth_preset_t *p = &SYN_PRESETS[preset_idx];
    v->wave   = p->wave;
    v->attack = p->attack;
    v->decay  = p->decay;
    v->tone   = p->tone;
}

static void voice_trigger(voice_t *v) {
    v->env       = 0.0f;
    v->env_stage = 0;
    v->phase     = 0.0f;
    v->phase2    = 0.0f;
    v->fm_phase  = 0.0f;
    v->lp_state  = 0.0f;
    v->vel       = 1.0f; /* sequencer hits are always full velocity */
    /* brown_last kept — avoids click from hard reset */
}

static void voice_trigger_pad(voice_t *v, float velocity) {
    voice_trigger(v);
    v->vel = velocity; /* pad hits scale by MIDI velocity */
}

static float voice_generate(voice_t *v, float freq) {
    float sample;
    float p_inc = freq * SR_INV;

    switch (v->wave) {
        case WAVE_SINE:
            sample = sinf(v->phase * TWO_PI);
            break;
        case WAVE_IMPULSE:
            sample = (v->phase < p_inc * 2.0f) ? 1.0f : 0.0f;
            break;
        case WAVE_NOISE: {
            uint32_t s = v->noise_state;
            s = s * 1664525u + 1013904223u;
            v->noise_state = s;
            sample = (float)(int32_t)s * (1.0f / 2147483648.0f);
            /* Apply LP filter — freq sets cutoff */
            { float c = 1.0f - expf(-TWO_PI * freq * SR_INV);
              v->noise_lp += c * (sample - v->noise_lp);
              sample = v->noise_lp; }
            break;
        }
        case WAVE_DAMPED:
            /* Tuning-fork: damping speed = tone (0=fast damp, 1=slow/long) */
            sample = sinf(v->phase * TWO_PI) *
                     expf(-v->phase * (1.0f - v->tone_smooth) * 30.0f);
            break;
        case WAVE_CLICK:
            sample = (v->phase < 0.005f) ?
                     (1.0f - v->phase / 0.005f) : 0.0f;
            break;
        case WAVE_SQUARE:
            sample = (v->phase < 0.5f) ? 1.0f : -1.0f;
            break;
        case WAVE_TRI:
            sample = 4.0f * fabsf(v->phase - 0.5f) - 1.0f;
            break;
        case WAVE_FM: {
            const synth_preset_t *sp = &SYN_PRESETS[v->syn_preset > 0 ?
                                                     (v->syn_preset - 1) : 0];
            float mod_freq = freq * sp->fm_ratio;
            float mod = sinf(v->fm_phase * TWO_PI) * sp->fm_depth;
            sample = sinf((v->phase + mod) * TWO_PI);
            v->fm_phase += mod_freq * SR_INV;
            if (v->fm_phase >= 1.0f) v->fm_phase -= 1.0f;
            break;
        }
        case WAVE_PINK: {
            /* Paul Kellett 7-stage pink noise (MIT) */
            uint32_t s = v->noise_state;
            s = s * 1664525u + 1013904223u;
            v->noise_state = s;
            float w = (float)(int32_t)s / 2147483648.0f;
            v->pink_b[0] =  0.99886f * v->pink_b[0] + w * 0.0555179f;
            v->pink_b[1] =  0.99332f * v->pink_b[1] + w * 0.0750759f;
            v->pink_b[2] =  0.96900f * v->pink_b[2] + w * 0.1538520f;
            v->pink_b[3] =  0.86650f * v->pink_b[3] + w * 0.3104856f;
            v->pink_b[4] =  0.55000f * v->pink_b[4] + w * 0.5329522f;
            v->pink_b[5] = -0.76160f * v->pink_b[5] - w * 0.0168980f;
            sample = (v->pink_b[0] + v->pink_b[1] + v->pink_b[2] + v->pink_b[3]
                    + v->pink_b[4] + v->pink_b[5] + v->pink_b[6] + w * 0.5362f) * 0.11f;
            v->pink_b[6] = w * 0.115926f;
            /* Apply LP filter — freq sets cutoff */
            { float c = 1.0f - expf(-TWO_PI * freq * SR_INV);
              v->noise_lp += c * (sample - v->noise_lp);
              sample = v->noise_lp; }
            break;
        }
        case WAVE_BROWN: {
            /* Brown noise: integrated white noise */
            uint32_t s = v->noise_state;
            s = s * 1664525u + 1013904223u;
            v->noise_state = s;
            float w = (float)(int32_t)s / 2147483648.0f * 0.02f;
            v->brown_last = clampf(v->brown_last + w, -1.0f, 1.0f);
            sample = v->brown_last;
            /* Apply LP filter — freq sets cutoff */
            { float c = 1.0f - expf(-TWO_PI * freq * SR_INV);
              v->noise_lp += c * (sample - v->noise_lp);
              sample = v->noise_lp; }
            break;
        }
        case WAVE_AM: {
            /* AM carrier × modulator; tone_smooth → AM rate 100–5000 Hz */
            float am_rate = 100.0f + v->tone_smooth * 4900.0f;
            v->fm_phase += am_rate * SR_INV;
            if (v->fm_phase >= 1.0f) v->fm_phase -= 1.0f;
            float am_mod = 0.5f + 0.5f * sinf(v->fm_phase * TWO_PI); /* 0..1 */
            sample = sinf(v->phase * TWO_PI) * am_mod;
            break;
        }
        default:
            sample = 0.0f;
    }

    v->phase += p_inc;
    if (v->phase >= 1.0f) v->phase -= 1.0f;
    return sample;
}

/* ── Patch system ─────────────────────────────────────────────────────────── */
#define NUM_PATCHES 30

typedef struct {
    const char *name;
    int   seq[4];         /* rhythm preset 1-25 (0=off) */
    int   syn[4];         /* synth preset 1-40 (0=off) */
    float freq[4];        /* Hz */
    float pan[4];         /* -1..1 */
    float decay[4];       /* s */
    float sweep[4];       /* 0..1 */
    float detune[4];      /* Hz */
    int   sub_div[4];     /* 1..8 */
    float density;
    float chaos;
    float gravity;
    float mod_speed;
    float mod_freq;
    float mod_density;
    int   mod_shape;
} patch_def_t;

static const patch_def_t PATCHES[NUM_PATCHES] = {
/*00: Init */      {"Init",      {1,1,1,1},{1,1,1,1},{440,880,1320,2200},{-0.6f,-0.2f,0.2f,0.6f},{0.005f,0.005f,0.005f,0.005f},{0,0,0,0},{0,0,0,0},{1,1,1,1},1.0f,0.0f,0.0f,0.0f,0.0f,0.0f,MOD_SHAPE_SINE},
/*01: Ikeda Grid*/ {"Ikeda Grid",{11,12,13,14},{1,2,3,1},{1000,2000,4000,500},{-0.5f,0.5f,-0.3f,0.3f},{0.002f,0.001f,0.003f,0.001f},{0,0,0,0},{0,0,0,0},{1,1,1,1},0.8f,0.1f,0.0f,0.0f,0.0f,0.0f,MOD_SHAPE_SINE},
/*02: Bernier*/    {"Bernier",   {16,17,18,19},{6,7,8,9},{330,440,528,660},{-0.4f,0.4f,-0.2f,0.2f},{0.040f,0.025f,0.060f,0.020f},{0,0,0,0},{0,0,0,0},{1,1,1,1},0.9f,0.05f,0.0f,0.0f,0.0f,0.0f,MOD_SHAPE_SINE},
/*03: Morse CQ*/   {"Morse CQ",  {1,2,3,4},{1,3,5,2},{800,1000,600,1200},{-0.3f,0.3f,-0.5f,0.5f},{0.003f,0.003f,0.003f,0.003f},{0,0,0,0},{0,0,0,0},{1,1,1,1},1.0f,0.0f,0.0f,0.0f,0.0f,0.0f,MOD_SHAPE_SINE},
/*04: Mathcore*/   {"Mathcore",  {6,7,8,9},{14,14,13,13},{200,400,800,1600},{-0.7f,-0.2f,0.2f,0.7f},{0.0005f,0.0005f,0.0005f,0.0005f},{0,0,0,0},{0,0,0,0},{1,1,1,1},0.9f,0.2f,0.0f,0.0f,0.0f,0.0f,MOD_SHAPE_SINE},
/*05: Pink Rain*/  {"Pink Rain", {11,15,16,20},{23,24,25,23},{2000,3000,1500,4000},{-0.6f,0.6f,-0.4f,0.4f},{0.008f,0.005f,0.012f,0.006f},{0,0,0,0},{0,0,0,0},{1,1,1,1},0.7f,0.2f,0.0f,0.15f,0.0f,0.3f,MOD_SHAPE_SINE},
/*06: Heterodyne*/ {"Heterodyne",{16,17,18,19},{1,1,1,1},{440,440,440,440},{-0.5f,-0.15f,0.15f,0.5f},{0.020f,0.025f,0.018f,0.022f},{0,0,0,0},{3.0f,5.0f,7.0f,11.0f},{1,1,1,1},0.8f,0.0f,0.3f,0.0f,0.0f,0.0f,MOD_SHAPE_SINE},
/*07: SubHarmonic*/{"Sub Harm",  {11,12,15,16},{1,5,4,3},{880,660,1320,440},{-0.4f,0.4f,-0.6f,0.6f},{0.040f,0.050f,0.030f,0.045f},{0,0,0,0},{0,0,0,0},{2,3,4,5},0.8f,0.0f,0.0f,0.0f,0.0f,0.0f,MOD_SHAPE_SINE},
/*08: CA Automata*/{"CA Automata",{21,22,23,24},{1,21,22,14},{500,1000,750,2000},{-0.3f,0.3f,-0.5f,0.5f},{0.004f,0.003f,0.005f,0.002f},{0,0,0,0},{0,0,0,0},{1,1,1,1},0.85f,0.15f,0.0f,0.0f,0.0f,0.0f,MOD_SHAPE_SINE},
/*09: FM Bell*/    {"FM Bell",   {13,14,16,17},{16,17,18,19},{440,880,660,1100},{-0.5f,0.5f,-0.3f,0.3f},{0.025f,0.020f,0.030f,0.015f},{0,0,0,0},{0,0,0,0},{1,1,1,1},0.7f,0.1f,0.0f,0.0f,0.0f,0.0f,MOD_SHAPE_SINE},
/*10: AM Texture*/ {"AM Texture",{11,16,20,21},{28,29,30,31},{1000,2000,500,3000},{-0.4f,0.4f,-0.7f,0.7f},{0.015f,0.012f,0.018f,0.010f},{0,0,0,0},{0,0,0,0},{1,1,1,1},0.8f,0.1f,0.0f,0.2f,0.2f,0.0f,MOD_SHAPE_TRI},
/*11: Brown Pulse*/{"Brown Pulse",{6,8,10,12},{26,27,26,27},{200,150,300,100},{-0.3f,0.3f,-0.5f,0.5f},{0.060f,0.040f,0.080f,0.035f},{0,0,0,0},{0,0,0,0},{1,1,1,1},0.85f,0.0f,0.0f,0.0f,0.0f,0.0f,MOD_SHAPE_SINE},
/*12: Click Matrix*/{"Clk Matrix",{10,11,12,13},{13,14,13,14},{1000,2000,4000,500},{-0.6f,-0.2f,0.2f,0.6f},{0.0005f,0.0005f,0.0005f,0.0005f},{0,0,0,0},{0,0,0,0},{1,1,1,1},1.0f,0.3f,0.0f,0.0f,0.0f,0.0f,MOD_SHAPE_SINE},
/*13: Sweep Casc*/ {"Sweep Casc",{16,18,17,19},{1,4,3,2},{220,330,440,660},{-0.5f,0.5f,-0.3f,0.3f},{0.040f,0.035f,0.050f,0.030f},{0.8f,0.6f,0.5f,0.4f},{0,0,0,0},{1,1,1,1},0.7f,0.05f,0.0f,0.0f,0.0f,0.0f,MOD_SHAPE_SINE},
/*14: Fibonacci*/  {"Fibonacci", {16,17,18,19},{6,7,9,8},{528,396,660,792},{-0.3f,0.3f,-0.6f,0.6f},{0.035f,0.040f,0.028f,0.045f},{0,0,0,0},{0,0,0,0},{1,1,1,1},0.9f,0.0f,0.0f,0.0f,0.0f,0.0f,MOD_SHAPE_SINE},
/*15: Digi Glitch*/{"Digi Glitch",{6,9,7,8},{20,21,22,20},{440,880,220,1760},{-0.5f,0.5f,-0.8f,0.8f},{0.003f,0.002f,0.004f,0.002f},{0,0,0,0},{0,0,0,0},{1,1,1,1},0.8f,0.3f,0.0f,0.0f,0.0f,0.0f,MOD_SHAPE_SQUARE},
/*16: Chirp Field*/{"Chirp Field",{11,14,21,16},{32,35,31,30},{500,1000,750,2000},{-0.4f,0.4f,-0.6f,0.6f},{0.020f,0.015f,0.025f,0.012f},{0.5f,0.7f,0.3f,0.6f},{0,0,0,0},{1,1,1,1},0.75f,0.1f,0.0f,0.15f,0.1f,0.0f,MOD_SHAPE_SINE},
/*17: Phase Cloud*/{"Phase Cloud",{16,17,18,19},{1,2,3,4},{220,330,440,550},{-0.6f,0.6f,-0.4f,0.4f},{0.060f,0.040f,0.080f,0.045f},{0.2f,0.15f,0.25f,0.1f},{1.0f,2.0f,0.5f,3.0f},{1,1,1,1},0.8f,0.0f,0.5f,0.3f,0.4f,0.2f,MOD_SHAPE_SINE},
/*18: Test Signal*/{"Test Signal",{11,11,11,11},{14,14,14,14},{1000,2000,4000,500},{-0.8f,-0.3f,0.3f,0.8f},{0.001f,0.001f,0.001f,0.001f},{0,0,0,0},{0,0,0,0},{1,1,1,1},1.0f,0.0f,0.0f,0.0f,0.0f,0.0f,MOD_SHAPE_SINE},
/*19: CW Radio*/   {"CW Radio",  {1,2,5,3},{1,1,1,1},{700,700,700,700},{-0.5f,-0.5f,0.5f,0.5f},{0.005f,0.004f,0.006f,0.005f},{0,0,0,0},{3.0f,0.0f,5.0f,0.0f},{1,1,1,1},1.0f,0.0f,0.0f,0.0f,0.0f,0.0f,MOD_SHAPE_SINE},
/*20: Cantor*/     {"Cantor",    {23,24,21,22},{1,6,4,8},{880,440,1760,220},{-0.4f,0.4f,-0.2f,0.2f},{0.008f,0.020f,0.005f,0.030f},{0,0.3f,0,0.5f},{0,0,0,0},{1,1,2,1},0.9f,0.1f,0.0f,0.0f,0.0f,0.0f,MOD_SHAPE_SINE},
/*21: Noise Gate*/ {"Noise Gate",{11,12,13,14},{10,11,14,12},{3000,5000,2000,8000},{-0.5f,0.5f,-0.3f,0.3f},{0.010f,0.005f,0.012f,0.004f},{0,0,0,0},{0,0,0,0},{1,1,1,1},0.8f,0.2f,0.0f,0.0f,0.0f,0.0f,MOD_SHAPE_SINE},
/*22: Sub Bass*/   {"Sub Bass",  {16,19,15,20},{26,27,5,6},{220,330,440,110},{0.0f,-0.2f,0.2f,0.0f},{0.100f,0.080f,0.060f,0.120f},{0,0,0,0},{0,0,0,0},{4,5,3,6},0.7f,0.0f,0.0f,0.0f,0.0f,0.0f,MOD_SHAPE_SINE},
/*23: Thue-Morse*/ {"Thue-Morse",{20,20,21,21},{20,21,22,20},{440,550,660,330},{-0.3f,0.3f,-0.6f,0.6f},{0.004f,0.005f,0.003f,0.006f},{0,0,0,0},{0,0,0,0},{1,1,1,1},0.85f,0.1f,0.0f,0.0f,0.0f,0.0f,MOD_SHAPE_SINE},
/*24: Pink Grid*/  {"Pink Grid", {10,11,12,13},{23,24,25,23},{4000,6000,3000,8000},{-0.6f,-0.2f,0.2f,0.6f},{0.010f,0.008f,0.012f,0.006f},{0,0,0,0},{0,0,0,0},{1,1,1,1},0.9f,0.2f,0.0f,0.1f,0.0f,0.2f,MOD_SHAPE_SINE},
/*25: Metallic FM*/{"Metallic FM",{6,7,8,9},{17,18,19,34},{300,600,900,1200},{-0.5f,0.5f,-0.3f,0.3f},{0.015f,0.012f,0.020f,0.010f},{0,0,0,0},{0,0,0,0},{1,1,1,1},0.75f,0.15f,0.0f,0.0f,0.0f,0.0f,MOD_SHAPE_SINE},
/*26: Minimal*/    {"Minimal",   {19,0,0,0},{5,0,0,0},{440,440,440,440},{0,0,0,0},{0.040f,0.005f,0.005f,0.005f},{0,0,0,0},{0,0,0,0},{1,1,1,1},0.8f,0.0f,0.0f,0.0f,0.0f,0.0f,MOD_SHAPE_SINE},
/*27: Maximum*/    {"Maximum",   {10,11,6,7},{1,8,16,26},{220,440,880,1760},{-0.7f,-0.2f,0.2f,0.7f},{0.005f,0.010f,0.003f,0.015f},{0.3f,0.0f,0.5f,0.0f},{0,2.0f,0,5.0f},{1,1,1,2},0.9f,0.2f,0.3f,0.2f,0.3f,0.2f,MOD_SHAPE_TRI},
/*28: Rule 30*/    {"Rule 30",   {21,22,23,24},{1,6,16,10},{660,440,880,330},{-0.4f,0.4f,-0.2f,0.2f},{0.008f,0.025f,0.005f,0.035f},{0,0.3f,0,0.5f},{0,0,0,0},{1,1,1,1},0.85f,0.05f,0.0f,0.0f,0.0f,0.0f,MOD_SHAPE_SINE},
/*29: Freq Lattice*/{"Freq Lat.", {11,13,15,17},{1,1,1,1},{200,400,600,800},{-0.6f,-0.2f,0.2f,0.6f},{0.010f,0.008f,0.012f,0.006f},{0,0,0,0},{0,0,0,0},{1,1,1,1},0.95f,0.0f,0.0f,0.0f,0.0f,0.0f,MOD_SHAPE_SINE},
};

static const char *PATCH_NAMES[NUM_PATCHES] = {
    "Init","Ikeda Grid","Bernier","Morse CQ","Mathcore",
    "Pink Rain","Heterodyne","Sub Harm","CA Automata","FM Bell",
    "AM Texture","Brown Pulse","Clk Matrix","Sweep Casc","Fibonacci",
    "Digi Glitch","Chirp Field","Phase Cloud","Test Signal","CW Radio",
    "Cantor","Noise Gate","Sub Bass","Thue-Morse","Pink Grid",
    "Metallic FM","Minimal","Maximum","Rule 30","Freq Lat."
};

static void apply_patch(signal_instance_t *inst, int idx) {
    if (idx < 0 || idx >= NUM_PATCHES) return;
    const patch_def_t *p = &PATCHES[idx];
    inst->patch_idx = idx;
    for (int v = 0; v < NUM_VOICES; v++) {
        voice_t *vp = &inst->voices[v];
        vp->seq_preset = p->seq[v];
        vp->syn_preset = p->syn[v];
        if (p->syn[v] > 0) voice_apply_synth_preset(vp, p->syn[v] - 1);
        vp->freq    = p->freq[v];
        vp->pan     = p->pan[v];
        vp->decay   = p->decay[v];
        vp->sweep   = p->sweep[v];
        vp->detune  = p->detune[v];
        vp->sub_div = p->sub_div[v];
    }
    inst->density     = p->density;
    inst->chaos       = p->chaos;
    inst->gravity     = p->gravity;
    inst->mod_speed   = p->mod_speed;
    inst->mod_freq    = p->mod_freq;
    inst->mod_density = p->mod_density;
    inst->mod_shape   = p->mod_shape;
    for (int v = 0; v < NUM_VOICES; v++) inst->vlfo_shape[v] = p->mod_shape;
}

static float rand_scale_freq(signal_instance_t *inst) {
    /* Pick a random MIDI note in musical range and quantize to current scale */
    int midi = 36 + (int)(randf(&inst->rng) * 48.99f); /* C2–C6, capped to 48 */
    if (midi < 21) midi = 21;
    if (midi > 108) midi = 108;
    float freq = 440.0f * powf(2.0f, (float)(midi - 69) / 12.0f);
    freq = quantize_freq(freq, inst->root, inst->scale);
    /* NaN/Inf guard */
    if (freq < 20.0f || freq > 20000.0f || freq != freq) freq = 440.0f;
    return freq;
}

/* Safe bounded random integer: returns 0..count-1 */
static inline int rnd_int(uint32_t *rng, int count) {
    int v = (int)(randf(rng) * (float)count);
    if (v < 0) v = 0;
    if (v >= count) v = count - 1;
    return v;
}

static void do_rnd_patch(signal_instance_t *inst) {
    apply_patch(inst, rnd_int(&inst->rng, NUM_PATCHES));
}

static void do_rnd_rytm(signal_instance_t *inst) {
    for (int v = 0; v < NUM_VOICES; v++)
        inst->voices[v].seq_preset = 1 + rnd_int(&inst->rng, NUM_SEQ_PRESETS);
}

static void do_rnd_voices(signal_instance_t *inst) {
    for (int v = 0; v < NUM_VOICES; v++) {
        voice_t *vp = &inst->voices[v];
        int syn = 1 + rnd_int(&inst->rng, NUM_SYN_PRESETS);
        vp->syn_preset = syn;
        voice_apply_synth_preset(vp, syn - 1);
        vp->freq  = 100.0f * powf(80.0f, randf(&inst->rng)); /* 100–8000 Hz exp */
        vp->decay = 0.001f + randf(&inst->rng) * 0.149f;
        vp->tone  = randf(&inst->rng);
        vp->pan   = clampf((randf(&inst->rng) - 0.5f) * 2.0f, -1.0f, 1.0f);
    }
}

static void do_same_voice(signal_instance_t *inst) {
    int syn = 1 + rnd_int(&inst->rng, NUM_SYN_PRESETS);
    float freq  = 100.0f * powf(80.0f, randf(&inst->rng));
    float decay = 0.001f + randf(&inst->rng) * 0.149f;
    float tone  = randf(&inst->rng);
    for (int v = 0; v < NUM_VOICES; v++) {
        voice_t *vp = &inst->voices[v];
        vp->syn_preset = syn;
        voice_apply_synth_preset(vp, syn - 1);
        vp->freq  = freq;
        vp->decay = decay;
        vp->tone  = tone;
        vp->pan   = clampf((randf(&inst->rng) - 0.5f) * 0.4f, -1.0f, 1.0f);
    }
}

static void do_rnd_mod(signal_instance_t *inst) {
    inst->mod_speed   = randf(&inst->rng) * 0.5f;
    inst->mod_offset  = randf(&inst->rng);
    inst->mod_freq    = randf(&inst->rng) * 0.4f;
    inst->mod_decay   = randf(&inst->rng) * 0.3f;
    inst->mod_pan     = randf(&inst->rng) * 0.5f;
    inst->mod_density = randf(&inst->rng) * 0.4f;
    int sh = rnd_int(&inst->rng, MOD_NUM_SHAPES); /* bounded — was OOB causing NaN */
    inst->mod_shape = sh;
    for (int v = 0; v < NUM_VOICES; v++) inst->vlfo_shape[v] = sh;
}

static void do_rnd_pitch(signal_instance_t *inst) {
    for (int v = 0; v < NUM_VOICES; v++)
        inst->voices[v].freq = rand_scale_freq(inst);
}

static void do_rnd_pan(signal_instance_t *inst) {
    for (int v = 0; v < NUM_VOICES; v++)
        inst->voices[v].pan = clampf((randf(&inst->rng) - 0.5f) * 2.0f, -1.0f, 1.0f);
}

/* ── Lifecycle ────────────────────────────────────────────────────────────── */
static void *create_instance(const char *module_dir, const char *json_defaults) {
    (void)module_dir; (void)json_defaults;

    signal_instance_t *inst = calloc(1, sizeof(signal_instance_t));
    if (!inst) return NULL;

    inst->master_vol = 1.0f;
    inst->bpm        = 120.0f;
    inst->tempo_sync = 1;
    inst->clk_div    = 1;
    inst->density    = 1.0f;
    inst->stereo_w   = 1.0f;
    inst->bit_crush  = 16;
    inst->bit_rate   = 0.0f;
    inst->dc_filter  = 1;
    inst->rng        = 0xDEADBEEFu;
    inst->patch_idx  = 0;

    for (int v = 0; v < NUM_VOICES; v++) {
        voice_t *vp = &inst->voices[v];
        vp->vol        = 0.8f;
        vp->level      = 0.8f;
        /* Default voice frequencies: 440, 880, 1320, 2200 Hz */
        float freqs[4] = {440.0f, 880.0f, 1320.0f, 2200.0f};
        vp->freq       = freqs[v];
        vp->wave       = WAVE_SINE;
        vp->attack     = 0.0001f;
        vp->decay      = 0.005f;
        vp->tone        = 0.5f;
        vp->tone_smooth = 0.5f;
        vp->noise_lp    = 0.0f;
        vp->vel         = 1.0f;
        vp->pan         = (v - 1.5f) * 0.4f; /* spread -0.6 -0.2 +0.2 +0.6 */
        vp->noise_state = 12345u + (uint32_t)v * 111u;
        vp->env_stage  = -1; /* idle */
        vp->phase2     = 0.0f;
        vp->sweep      = 0.0f;
        vp->detune     = 0.0f;
        vp->sub_div    = 1;
        memset(vp->pink_b, 0, sizeof(vp->pink_b));
        vp->brown_last = 0.0f;
    }

    /* Modulation LFO defaults */
    inst->mod_amount  = 1.0f; /* full depth by default */
    inst->mod_speed   = 0.0f;
    inst->mod_offset  = 0.0f;
    inst->mod_freq    = 0.0f;
    inst->mod_decay   = 0.0f;
    inst->mod_pan     = 0.0f;
    inst->mod_density = 0.0f;
    inst->mod_shape   = MOD_SHAPE_SINE;
    for (int v = 0; v < NUM_VOICES; v++) {
        inst->vlfo_phase[v]  = 0.0f;
        inst->vlfo_sh_val[v] = 0.0f;
        inst->vlfo_rng[v]    = 111111u * (uint32_t)(v + 1);
        inst->vlfo_value[v]  = 0.0f;
        inst->vlfo_shape[v]  = MOD_SHAPE_SINE;
    }

    return inst;
}

static void destroy_instance(void *instance) { free(instance); }

/* ── MIDI ─────────────────────────────────────────────────────────────────── */
static void on_midi(void *instance, const uint8_t *msg, int len, int source) {
    signal_instance_t *inst = (signal_instance_t *)instance;
    if (!inst || len < 1) return;
    (void)source;

    uint8_t status = msg[0];

    /* MIDI clock — advance sequencer when in Sync mode */
    if (status == 0xF8) { /* tick */
        if (!inst->tempo_sync) return;
        for (int v = 0; v < NUM_VOICES; v++) {
            voice_t *vp = &inst->voices[v];
            if (vp->seq_preset == 0) continue;
            int idx = vp->seq_preset - 1;
            if (idx >= NUM_SEQ_PRESETS) continue;
            const rhythm_pattern_t *pat = &PATTERNS[v][idx];

            /* ticks_per_step = 24 / subdivision / clk_div */
            float tps = 24.0f / (float)pat->subdivision / (float)inst->clk_div;

            /* Morse speed modifier: scale tps for Morse presets (idx 0-4) */
            if (idx < 5) {
                float spd = 0.25f + inst->morse_spd * 1.75f; /* 0.25× to 2× */
                tps *= spd;
            }

            vp->tick_accum += 1.0f;
            if (vp->tick_accum >= tps) {
                vp->tick_accum -= tps;
                vp->step = (vp->step + 1) % pat->length;
                if (pat->hits[vp->step]) {
                    /* Density + mod_density + chaos gate */
                    float prob = inst->density;
                    if (inst->mod_density > 0.0f) {
                        float lnorm = (inst->vlfo_value[v] * inst->mod_amount + 1.0f) * 0.5f;
                        prob *= (1.0f - inst->mod_density + inst->mod_density * lnorm);
                    }
                    if (inst->chaos > 0.0f)
                        prob += (randf(&inst->rng) - 0.5f) * inst->chaos;
                    prob = clampf(prob, 0.0f, 1.0f);
                    if (randf(&inst->rng) < prob) {
                        /* Gravity: with probability=gravity, sync all other voices */
                        if (inst->gravity > 0.0f && randf(&inst->rng) < inst->gravity) {
                            for (int ov = 0; ov < NUM_VOICES; ov++) {
                                if (ov != v) inst->voices[ov].tick_accum = 0.0f;
                            }
                        }
                        voice_trigger(vp);
                    }
                }
            }
        }
        return;
    }

    if (status == 0xFA || status == 0xFB) { /* Start / Continue */
        inst->midi_running = 1;
        for (int v = 0; v < NUM_VOICES; v++) {
            inst->voices[v].step       = 0;
            inst->voices[v].step_accum = 0.0f;
            inst->voices[v].tick_accum = 0.0f;
        }
        return;
    }

    if (status == 0xFC) { /* Stop */
        inst->midi_running = 0;
        return;
    }

    if (len < 3) return;
    uint8_t ch_status = status & 0xF0;

    /* Drum kit pad layout — note % 4 maps to voice (4 identical rows) */
    if (ch_status == 0x90 && msg[2] > 0) {
        int vi = (int)msg[1] % NUM_VOICES;
        voice_trigger_pad(&inst->voices[vi], (float)msg[2] / 127.0f);
        return;
    }
    if (ch_status == 0x80 || (ch_status == 0x90 && msg[2] == 0)) {
        /* Note Off — voices are one-shot envelopes, nothing to do */
        return;
    }

    /* CC 1 (mod wheel) → chaos */
    if (ch_status == 0xB0 && msg[1] == 1)
        inst->chaos = msg[2] / 127.0f;
}

/* ── Parameters ───────────────────────────────────────────────────────────── */
/* Forward declaration */
static void signal_set_param(signal_instance_t *inst, const char *key, const char *val);

static void set_param(void *instance, const char *key, const char *val) {
    signal_instance_t *inst = (signal_instance_t *)instance;
    if (!inst || !key || !val) return;
    signal_set_param(inst, key, val);
}

static void signal_set_param(signal_instance_t *inst, const char *key, const char *val) {
    char k[24];

    /* Page navigation */
    if (strcmp(key, "_level") == 0 || strcmp(key, "current_level") == 0) {
        for (int p = 0; p < 8; p++) {
            if (strcmp(val, PAGE_NAMES[p]) == 0) { inst->current_page = p; return; }
        }
        return;
    }

    /* BPM from host */
    if (strcmp(key, "bpm") == 0) { inst->bpm = clampf(atof(val), 20.0f, 500.0f); return; }

    /* Page 1: seq/syn presets */
    for (int v = 0; v < NUM_VOICES; v++) {
        snprintf(k, sizeof(k), "seq%d", v + 1);
        if (strcmp(key, k) == 0) {
            inst->voices[v].seq_preset = (int)clampf(atof(val), 0, 25);
            return;
        }
        snprintf(k, sizeof(k), "syn%d", v + 1);
        if (strcmp(key, k) == 0) {
            int p = (int)clampf(atof(val), 0, 25);
            inst->voices[v].syn_preset = p;
            if (p > 0) voice_apply_synth_preset(&inst->voices[v], p - 1);
            return;
        }
    }

    /* Page 2: Mix */
    for (int v = 0; v < NUM_VOICES; v++) {
        snprintf(k, sizeof(k), "v%d_level", v + 1);
        if (strcmp(key, k) == 0) { inst->voices[v].level = clampf(atof(val), 0, 1); return; }
        snprintf(k, sizeof(k), "v%d_freq", v + 1);
        if (strcmp(key, k) == 0) { inst->voices[v].freq = clampf(atof(val), 20, 20000); return; }
    }

    /* Page 3: Params */
    if (strcmp(key, "root") == 0) {
        for (int i = 0; i < ROOT_COUNT; i++) {
            if (strcmp(val, ROOT_NAMES[i]) == 0) { inst->root = i; return; }
        }
        int iv = atoi(val);
        if (iv >= 0 && iv < ROOT_COUNT) inst->root = iv;
        return;
    }
    if (strcmp(key, "scale") == 0) {
        for (int i = 0; i < SCALE_COUNT; i++) {
            if (strcmp(val, SCALE_NAMES[i]) == 0) { inst->scale = i; return; }
        }
        int iv = atoi(val);
        if (iv >= 0 && iv < SCALE_COUNT) inst->scale = iv;
        return;
    }
    if (strcmp(key, "density")   == 0) { inst->density   = clampf(atof(val), 0, 1); return; }
    if (strcmp(key, "chaos")     == 0) { inst->chaos     = clampf(atof(val), 0, 1); return; }
    if (strcmp(key, "gravity")   == 0) { inst->gravity   = clampf(atof(val), 0, 1); return; }
    if (strcmp(key, "clk_div")   == 0) { inst->clk_div   = (int)clampf(atof(val), 1, 32); return; }
    if (strcmp(key, "morse_spd") == 0) { inst->morse_spd = clampf(atof(val), 0, 1); return; }
    if (strcmp(key, "swing")     == 0) { inst->swing     = clampf(atof(val), 0, 1); return; }

    /* Pages 4–7: Per-voice params */
    for (int v = 0; v < NUM_VOICES; v++) {
        snprintf(k, sizeof(k), "v%d_preset", v + 1);
        if (strcmp(key, k) == 0) {
            int p = (int)clampf(atof(val), 0, 25);
            inst->voices[v].syn_preset = p;
            if (p > 0) voice_apply_synth_preset(&inst->voices[v], p - 1);
            return;
        }
        snprintf(k, sizeof(k), "v%d_vol", v + 1);
        if (strcmp(key, k) == 0) { inst->voices[v].vol = clampf(atof(val), 0, 1); return; }
        snprintf(k, sizeof(k), "v%d_vfreq", v + 1);
        if (strcmp(key, k) == 0) { inst->voices[v].freq = clampf(atof(val), 20, 20000); return; }
        snprintf(k, sizeof(k), "v%d_wave", v + 1);
        if (strcmp(key, k) == 0) {
            for (int w = 0; w < WAVE_COUNT; w++) {
                if (strcmp(val, WAVE_NAMES[w]) == 0) { inst->voices[v].wave = w; return; }
            }
            int iw = atoi(val);
            if (iw >= 0 && iw < WAVE_COUNT) inst->voices[v].wave = iw;
            return;
        }
        snprintf(k, sizeof(k), "v%d_tone", v + 1);
        if (strcmp(key, k) == 0) { inst->voices[v].tone = clampf(atof(val), 0, 1); return; }
        snprintf(k, sizeof(k), "v%d_attack", v + 1);
        if (strcmp(key, k) == 0) { inst->voices[v].attack = clampf(atof(val), 0.0001f, 0.05f); return; }
        snprintf(k, sizeof(k), "v%d_decay", v + 1);
        if (strcmp(key, k) == 0) { inst->voices[v].decay = clampf(atof(val), 0.0001f, 0.5f); return; }
        snprintf(k, sizeof(k), "v%d_pan", v + 1);
        if (strcmp(key, k) == 0) { inst->voices[v].pan = clampf(atof(val), -1, 1); return; }
        /* Hainbach / Modulation-applied per-voice params */
        snprintf(k, sizeof(k), "v%d_sweep", v + 1);
        if (strcmp(key, k) == 0) { inst->voices[v].sweep   = clampf(atof(val), 0, 1); return; }
        snprintf(k, sizeof(k), "v%d_detune", v + 1);
        if (strcmp(key, k) == 0) { inst->voices[v].detune  = clampf(atof(val), 0, 20); return; }
        snprintf(k, sizeof(k), "v%d_sub_div", v + 1);
        if (strcmp(key, k) == 0) { inst->voices[v].sub_div = (int)clampf(atof(val), 1, 8); return; }
    }

    /* Patch page */
    if (strcmp(key, "patch") == 0) {
        for (int i = 0; i < NUM_PATCHES; i++) {
            if (strcmp(val, PATCH_NAMES[i]) == 0) { apply_patch(inst, i); return; }
        }
        int iv = atoi(val);
        if (iv >= 0 && iv < NUM_PATCHES) apply_patch(inst, iv);
        return;
    }
    if (strcmp(key, "rnd_patch")  == 0) { if (atoi(val)==1) do_rnd_patch(inst);  return; }
    if (strcmp(key, "rnd_rytm")   == 0) { if (atoi(val)==1) do_rnd_rytm(inst);   return; }
    if (strcmp(key, "rnd_voices") == 0) { if (atoi(val)==1) do_rnd_voices(inst); return; }
    if (strcmp(key, "same_voice") == 0) { if (atoi(val)==1) do_same_voice(inst); return; }
    if (strcmp(key, "rnd_mod")    == 0) { if (atoi(val)==1) do_rnd_mod(inst);    return; }
    if (strcmp(key, "rnd_pitch")  == 0) { if (atoi(val)==1) do_rnd_pitch(inst);  return; }
    if (strcmp(key, "rnd_pan")    == 0) { if (atoi(val)==1) do_rnd_pan(inst);    return; }

    /* Modulation page */
    if (strcmp(key, "mod_amount")  == 0) { inst->mod_amount  = clampf(atof(val), 0, 1); return; }
    if (strcmp(key, "mod_speed")   == 0) { inst->mod_speed   = clampf(atof(val), 0, 1); return; }
    if (strcmp(key, "mod_offset")  == 0) { inst->mod_offset  = clampf(atof(val), 0, 1); return; }
    if (strcmp(key, "mod_freq")    == 0) { inst->mod_freq    = clampf(atof(val), 0, 1); return; }
    if (strcmp(key, "mod_decay")   == 0) { inst->mod_decay   = clampf(atof(val), 0, 1); return; }
    if (strcmp(key, "mod_pan")     == 0) { inst->mod_pan     = clampf(atof(val), 0, 1); return; }
    if (strcmp(key, "mod_density") == 0) { inst->mod_density = clampf(atof(val), 0, 1); return; }
    if (strcmp(key, "mod_shape") == 0) {
        for (int i = 0; i < MOD_NUM_SHAPES; i++) {
            if (strcmp(val, MOD_SHAPE_NAMES[i]) == 0) {
                inst->mod_shape = i;
                for (int v = 0; v < NUM_VOICES; v++) {
                    if (i == MOD_SHAPE_RANDOM) {
                        inst->vlfo_rng[v] = inst->vlfo_rng[v] * 1664525u + 1013904223u;
                        inst->vlfo_shape[v] = (int)(inst->vlfo_rng[v] % MOD_NUM_SHAPES);
                    } else {
                        inst->vlfo_shape[v] = i;
                    }
                }
                return;
            }
        }
        int iv = atoi(val);
        if (iv >= 0 && iv < MOD_NUM_SHAPES) {
            inst->mod_shape = iv;
            for (int v = 0; v < NUM_VOICES; v++) inst->vlfo_shape[v] = iv;
        }
        return;
    }
    if (strcmp(key, "mod_reset") == 0) {
        if (atoi(val) != 1) return; /* only fire on 1 */
        for (int v = 0; v < NUM_VOICES; v++) {
            inst->vlfo_phase[v]  = 0.0f;
            inst->vlfo_sh_val[v] = 0.0f;
            inst->vlfo_value[v]  = 0.0f;
        }
        return;
    }

    /* Page 8: General */
    if (strcmp(key, "master_vol") == 0) { inst->master_vol = clampf(atof(val), 0, 1); return; }
    if (strcmp(key, "bit_rate")   == 0) { inst->bit_rate   = clampf(atof(val), 0, 1); return; }
    if (strcmp(key, "tempo_sync") == 0) {
        if (strcmp(val, "Sync") == 0 || atoi(val) == 1) inst->tempo_sync = 1;
        else inst->tempo_sync = 0;
        return;
    }
    if (strcmp(key, "stereo_w")  == 0) { inst->stereo_w  = clampf(atof(val), 0, 1); return; }
    if (strcmp(key, "bit_crush") == 0) { inst->bit_crush = (int)clampf(atof(val), 1, 16); return; }
    if (strcmp(key, "drift")     == 0) { inst->drift     = clampf(atof(val), 0, 1); return; }
    if (strcmp(key, "jitter")    == 0) { inst->jitter    = clampf(atof(val), 0, 1); return; }
    if (strcmp(key, "dc_filter") == 0) {
        inst->dc_filter = (strcmp(val, "On") == 0 || atoi(val) == 1) ? 1 : 0;
        return;
    }
    if (strcmp(key, "out_mode") == 0) {
        if (strcmp(val, "Mono")   == 0 || atoi(val) == 1) inst->out_mode = 1;
        else if (strcmp(val, "Spread") == 0 || atoi(val) == 2) inst->out_mode = 2;
        else inst->out_mode = 0;
        return;
    }

    /* State restore */
    if (strcmp(key, "state") == 0) {
        /* Simple key=value CSV parsing */
        char buf[4096];
        snprintf(buf, sizeof(buf), "%s", val);
        char *p = buf;
        while (*p) {
            /* Find key */
            char *kstart = p;
            while (*p && *p != '=') p++;
            if (!*p) break;
            *p = '\0'; p++;
            char *vstart = p;
            while (*p && *p != ';') p++;
            if (*p) { *p = '\0'; p++; }
            signal_set_param(inst, kstart, vstart);
        }
    }
}

/* ── chain_params JSON ────────────────────────────────────────────────────── */
static const char CHAIN_PARAMS_JSON[] =
    "[{\"key\":\"seq1\",\"name\":\"Seq 1\",\"type\":\"int\",\"min\":0,\"max\":25,\"step\":1},"
    "{\"key\":\"seq2\",\"name\":\"Seq 2\",\"type\":\"int\",\"min\":0,\"max\":25,\"step\":1},"
    "{\"key\":\"seq3\",\"name\":\"Seq 3\",\"type\":\"int\",\"min\":0,\"max\":25,\"step\":1},"
    "{\"key\":\"seq4\",\"name\":\"Seq 4\",\"type\":\"int\",\"min\":0,\"max\":25,\"step\":1},"
    "{\"key\":\"syn1\",\"name\":\"Syn 1\",\"type\":\"int\",\"min\":0,\"max\":40,\"step\":1},"
    "{\"key\":\"syn2\",\"name\":\"Syn 2\",\"type\":\"int\",\"min\":0,\"max\":40,\"step\":1},"
    "{\"key\":\"syn3\",\"name\":\"Syn 3\",\"type\":\"int\",\"min\":0,\"max\":40,\"step\":1},"
    "{\"key\":\"syn4\",\"name\":\"Syn 4\",\"type\":\"int\",\"min\":0,\"max\":40,\"step\":1},"
    "{\"key\":\"v1_level\",\"name\":\"V1 Level\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v2_level\",\"name\":\"V2 Level\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v3_level\",\"name\":\"V3 Level\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v4_level\",\"name\":\"V4 Level\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v1_freq\",\"name\":\"V1 Freq\",\"type\":\"float\",\"min\":20,\"max\":20000,\"step\":1},"
    "{\"key\":\"v2_freq\",\"name\":\"V2 Freq\",\"type\":\"float\",\"min\":20,\"max\":20000,\"step\":1},"
    "{\"key\":\"v3_freq\",\"name\":\"V3 Freq\",\"type\":\"float\",\"min\":20,\"max\":20000,\"step\":1},"
    "{\"key\":\"v4_freq\",\"name\":\"V4 Freq\",\"type\":\"float\",\"min\":20,\"max\":20000,\"step\":1},"
    "{\"key\":\"root\",\"name\":\"Root\",\"type\":\"enum\",\"options\":[\"Free\",\"C\",\"C#\",\"D\",\"D#\",\"E\",\"F\",\"F#\",\"G\",\"G#\",\"A\",\"A#\",\"B\"]},"
    "{\"key\":\"scale\",\"name\":\"Scale\",\"type\":\"enum\",\"options\":[\"Free\",\"Chromatic\",\"Major\",\"Minor\",\"Pentatonic\",\"WholeTone\",\"Diminished\",\"HarmMin\"]},"
    "{\"key\":\"density\",\"name\":\"Density\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"chaos\",\"name\":\"Chaos\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"gravity\",\"name\":\"Gravity\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"clk_div\",\"name\":\"Clk Div\",\"type\":\"int\",\"min\":1,\"max\":32,\"step\":1},"
    "{\"key\":\"morse_spd\",\"name\":\"Morse Spd\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"swing\",\"name\":\"Swing\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v1_preset\",\"name\":\"V1 Preset\",\"type\":\"int\",\"min\":0,\"max\":40,\"step\":1},"
    "{\"key\":\"v1_vol\",\"name\":\"V1 Vol\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v1_vfreq\",\"name\":\"V1 Freq\",\"type\":\"float\",\"min\":20,\"max\":20000,\"step\":1},"
    "{\"key\":\"v1_wave\",\"name\":\"V1 Wave\",\"type\":\"enum\",\"options\":[\"Sine\",\"Impulse\",\"Noise\",\"Damped\",\"Click\",\"Square\",\"Tri\",\"FM\",\"Pink\",\"Brown\",\"AM\"]},"
    "{\"key\":\"v1_tone\",\"name\":\"V1 Tone\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v1_attack\",\"name\":\"V1 Attack\",\"type\":\"float\",\"min\":0.0001,\"max\":0.05,\"step\":0.0001},"
    "{\"key\":\"v1_decay\",\"name\":\"V1 Decay\",\"type\":\"float\",\"min\":0.0001,\"max\":0.5,\"step\":0.001},"
    "{\"key\":\"v1_pan\",\"name\":\"V1 Pan\",\"type\":\"float\",\"min\":-1,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v1_sub_div\",\"name\":\"V1 Sub Div\",\"type\":\"int\",\"min\":1,\"max\":8,\"step\":1},"
    "{\"key\":\"v1_sweep\",\"name\":\"V1 Sweep\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v1_detune\",\"name\":\"V1 Detune\",\"type\":\"float\",\"min\":0,\"max\":20,\"step\":0.1},"
    "{\"key\":\"v2_preset\",\"name\":\"V2 Preset\",\"type\":\"int\",\"min\":0,\"max\":40,\"step\":1},"
    "{\"key\":\"v2_vol\",\"name\":\"V2 Vol\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v2_vfreq\",\"name\":\"V2 Freq\",\"type\":\"float\",\"min\":20,\"max\":20000,\"step\":1},"
    "{\"key\":\"v2_wave\",\"name\":\"V2 Wave\",\"type\":\"enum\",\"options\":[\"Sine\",\"Impulse\",\"Noise\",\"Damped\",\"Click\",\"Square\",\"Tri\",\"FM\",\"Pink\",\"Brown\",\"AM\"]},"
    "{\"key\":\"v2_tone\",\"name\":\"V2 Tone\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v2_attack\",\"name\":\"V2 Attack\",\"type\":\"float\",\"min\":0.0001,\"max\":0.05,\"step\":0.0001},"
    "{\"key\":\"v2_decay\",\"name\":\"V2 Decay\",\"type\":\"float\",\"min\":0.0001,\"max\":0.5,\"step\":0.001},"
    "{\"key\":\"v2_pan\",\"name\":\"V2 Pan\",\"type\":\"float\",\"min\":-1,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v2_sub_div\",\"name\":\"V2 Sub Div\",\"type\":\"int\",\"min\":1,\"max\":8,\"step\":1},"
    "{\"key\":\"v2_sweep\",\"name\":\"V2 Sweep\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v2_detune\",\"name\":\"V2 Detune\",\"type\":\"float\",\"min\":0,\"max\":20,\"step\":0.1},"
    "{\"key\":\"v3_preset\",\"name\":\"V3 Preset\",\"type\":\"int\",\"min\":0,\"max\":40,\"step\":1},"
    "{\"key\":\"v3_vol\",\"name\":\"V3 Vol\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v3_vfreq\",\"name\":\"V3 Freq\",\"type\":\"float\",\"min\":20,\"max\":20000,\"step\":1},"
    "{\"key\":\"v3_wave\",\"name\":\"V3 Wave\",\"type\":\"enum\",\"options\":[\"Sine\",\"Impulse\",\"Noise\",\"Damped\",\"Click\",\"Square\",\"Tri\",\"FM\",\"Pink\",\"Brown\",\"AM\"]},"
    "{\"key\":\"v3_tone\",\"name\":\"V3 Tone\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v3_attack\",\"name\":\"V3 Attack\",\"type\":\"float\",\"min\":0.0001,\"max\":0.05,\"step\":0.0001},"
    "{\"key\":\"v3_decay\",\"name\":\"V3 Decay\",\"type\":\"float\",\"min\":0.0001,\"max\":0.5,\"step\":0.001},"
    "{\"key\":\"v3_pan\",\"name\":\"V3 Pan\",\"type\":\"float\",\"min\":-1,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v3_sub_div\",\"name\":\"V3 Sub Div\",\"type\":\"int\",\"min\":1,\"max\":8,\"step\":1},"
    "{\"key\":\"v3_sweep\",\"name\":\"V3 Sweep\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v3_detune\",\"name\":\"V3 Detune\",\"type\":\"float\",\"min\":0,\"max\":20,\"step\":0.1},"
    "{\"key\":\"v4_preset\",\"name\":\"V4 Preset\",\"type\":\"int\",\"min\":0,\"max\":40,\"step\":1},"
    "{\"key\":\"v4_vol\",\"name\":\"V4 Vol\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v4_vfreq\",\"name\":\"V4 Freq\",\"type\":\"float\",\"min\":20,\"max\":20000,\"step\":1},"
    "{\"key\":\"v4_wave\",\"name\":\"V4 Wave\",\"type\":\"enum\",\"options\":[\"Sine\",\"Impulse\",\"Noise\",\"Damped\",\"Click\",\"Square\",\"Tri\",\"FM\",\"Pink\",\"Brown\",\"AM\"]},"
    "{\"key\":\"v4_tone\",\"name\":\"V4 Tone\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v4_attack\",\"name\":\"V4 Attack\",\"type\":\"float\",\"min\":0.0001,\"max\":0.05,\"step\":0.0001},"
    "{\"key\":\"v4_decay\",\"name\":\"V4 Decay\",\"type\":\"float\",\"min\":0.0001,\"max\":0.5,\"step\":0.001},"
    "{\"key\":\"v4_pan\",\"name\":\"V4 Pan\",\"type\":\"float\",\"min\":-1,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v4_sub_div\",\"name\":\"V4 Sub Div\",\"type\":\"int\",\"min\":1,\"max\":8,\"step\":1},"
    "{\"key\":\"v4_sweep\",\"name\":\"V4 Sweep\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v4_detune\",\"name\":\"V4 Detune\",\"type\":\"float\",\"min\":0,\"max\":20,\"step\":0.1},"
    "{\"key\":\"patch\",\"name\":\"Patch\",\"type\":\"enum\",\"options\":[\"Init\",\"Ikeda Grid\",\"Bernier\",\"Morse CQ\",\"Mathcore\",\"Pink Rain\",\"Heterodyne\",\"Sub Harm\",\"CA Automata\",\"FM Bell\",\"AM Texture\",\"Brown Pulse\",\"Clk Matrix\",\"Sweep Casc\",\"Fibonacci\",\"Digi Glitch\",\"Chirp Field\",\"Phase Cloud\",\"Test Signal\",\"CW Radio\",\"Cantor\",\"Noise Gate\",\"Sub Bass\",\"Thue-Morse\",\"Pink Grid\",\"Metallic FM\",\"Minimal\",\"Maximum\",\"Rule 30\",\"Freq Lat.\"]},"
    "{\"key\":\"rnd_patch\",\"name\":\"Rnd Patch\",\"type\":\"int\",\"min\":0,\"max\":1,\"step\":1},"
    "{\"key\":\"rnd_rytm\",\"name\":\"Rnd Rytm\",\"type\":\"int\",\"min\":0,\"max\":1,\"step\":1},"
    "{\"key\":\"rnd_voices\",\"name\":\"Rnd Voices\",\"type\":\"int\",\"min\":0,\"max\":1,\"step\":1},"
    "{\"key\":\"same_voice\",\"name\":\"Same Voice\",\"type\":\"int\",\"min\":0,\"max\":1,\"step\":1},"
    "{\"key\":\"rnd_mod\",\"name\":\"Rnd Mod\",\"type\":\"int\",\"min\":0,\"max\":1,\"step\":1},"
    "{\"key\":\"rnd_pitch\",\"name\":\"Rnd Pitch\",\"type\":\"int\",\"min\":0,\"max\":1,\"step\":1},"
    "{\"key\":\"rnd_pan\",\"name\":\"Rnd Pan\",\"type\":\"int\",\"min\":0,\"max\":1,\"step\":1},"
    "{\"key\":\"mod_amount\",\"name\":\"Mod Amt\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"mod_speed\",\"name\":\"Mod Speed\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"mod_offset\",\"name\":\"Mod Offset\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"mod_freq\",\"name\":\"Freq Mod\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"mod_decay\",\"name\":\"Decay Mod\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"mod_pan\",\"name\":\"Pan Mod\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"mod_density\",\"name\":\"Density Mod\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"mod_shape\",\"name\":\"Mod Shape\",\"type\":\"enum\",\"options\":[\"Sine\",\"Tri\",\"Saw\",\"Square\",\"S&H\",\"Random\"]},"
    "{\"key\":\"mod_reset\",\"name\":\"Mod Reset\",\"type\":\"int\",\"min\":0,\"max\":1,\"step\":1},"
    "{\"key\":\"master_vol\",\"name\":\"Volume\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"tempo_sync\",\"name\":\"Sync\",\"type\":\"enum\",\"options\":[\"Free\",\"Sync\"]},"
    "{\"key\":\"bpm\",\"name\":\"BPM\",\"type\":\"float\",\"min\":20,\"max\":500,\"step\":1},"
    "{\"key\":\"bit_crush\",\"name\":\"Bit Crush\",\"type\":\"int\",\"min\":1,\"max\":16,\"step\":1},"
    "{\"key\":\"bit_rate\",\"name\":\"Bit Rate\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"stereo_w\",\"name\":\"Stereo W\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"drift\",\"name\":\"Drift\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"jitter\",\"name\":\"Jitter\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"dc_filter\",\"name\":\"DC Filt\",\"type\":\"enum\",\"options\":[\"Off\",\"On\"]},"
    "{\"key\":\"out_mode\",\"name\":\"Out Mode\",\"type\":\"enum\",\"options\":[\"Stereo\",\"Mono\",\"Spread\"]}]";

/* ── Knob page maps ───────────────────────────────────────────────────────── */
static const char *KNOB_KEYS[11][8] = {
    /* PAGE_ROOT — mirrors generate for chain_edit hover */
    {"seq1","seq2","seq3","seq4","syn1","syn2","syn3","syn4"},
    /* PAGE_GENERATE */
    {"seq1","seq2","seq3","seq4","syn1","syn2","syn3","syn4"},
    /* PAGE_PARAMS */
    {"root","scale","density","chaos","gravity","clk_div","morse_spd","swing"},
    /* PAGE_MODULATION — Mod Offset is menu-only (param 9) */
    {"mod_amount","mod_speed","mod_freq","mod_decay","mod_pan","mod_density","mod_shape","mod_reset"},
    /* PAGE_PATCH */
    {"patch","rnd_patch","rnd_rytm","rnd_voices","same_voice","rnd_mod","rnd_pitch","rnd_pan"},
    /* PAGE_MIX */
    {"v1_level","v2_level","v3_level","v4_level","v1_freq","v2_freq","v3_freq","v4_freq"},
    /* PAGE_VOICE1: Preset,Vol,Freq,Wave,Tone,Decay,Detune,Pan — Attack menu-only */
    {"v1_preset","v1_vol","v1_vfreq","v1_wave","v1_tone","v1_decay","v1_detune","v1_pan"},
    /* PAGE_VOICE2 */
    {"v2_preset","v2_vol","v2_vfreq","v2_wave","v2_tone","v2_decay","v2_detune","v2_pan"},
    /* PAGE_VOICE3 */
    {"v3_preset","v3_vol","v3_vfreq","v3_wave","v3_tone","v3_decay","v3_detune","v3_pan"},
    /* PAGE_VOICE4 */
    {"v4_preset","v4_vol","v4_vfreq","v4_wave","v4_tone","v4_decay","v4_detune","v4_pan"},
    /* PAGE_GENERAL */
    {"master_vol","tempo_sync","bpm","bit_crush","bit_rate","stereo_w","drift","jitter"}
};

/* ── get_param ────────────────────────────────────────────────────────────── */
static int get_param(void *instance, const char *key, char *buf, int buf_len) {
    signal_instance_t *inst = (signal_instance_t *)instance;
    if (!inst || !key || !buf || buf_len <= 0) return -1;

    if (strcmp(key, "name")         == 0) return snprintf(buf, buf_len, "Signal");
    if (strcmp(key, "chain_params") == 0) return snprintf(buf, buf_len, "%s", CHAIN_PARAMS_JSON);

    /* ui_hierarchy — Schwung calls this to discover page structure.
     * Without it, getComponentHierarchy() returns null and Schwung falls
     * back to the preset browser instead of showing the menu. */
    if (strcmp(key, "ui_hierarchy") == 0) {
        return snprintf(buf, buf_len,
            "{\"modes\":null,\"levels\":{"
            "\"root\":{\"name\":\"Signal\","
            "\"knobs\":[\"seq1\",\"seq2\",\"seq3\",\"seq4\",\"syn1\",\"syn2\",\"syn3\",\"syn4\"],"
            "\"params\":["
            "{\"level\":\"generate\",\"label\":\"Generate\"},"
            "{\"level\":\"params\",\"label\":\"Params\"},"
            "{\"level\":\"modulation\",\"label\":\"Modulation\"},"
            "{\"level\":\"patch\",\"label\":\"Patch\"},"
            "{\"level\":\"mix\",\"label\":\"Mix\"},"
            "{\"level\":\"voice1\",\"label\":\"Voice 1\"},"
            "{\"level\":\"voice2\",\"label\":\"Voice 2\"},"
            "{\"level\":\"voice3\",\"label\":\"Voice 3\"},"
            "{\"level\":\"voice4\",\"label\":\"Voice 4\"},"
            "{\"level\":\"general\",\"label\":\"General\"}"
            "]},"
            "\"generate\":{\"name\":\"Generate\","
            "\"knobs\":[\"seq1\",\"seq2\",\"seq3\",\"seq4\",\"syn1\",\"syn2\",\"syn3\",\"syn4\"],"
            "\"params\":[\"seq1\",\"seq2\",\"seq3\",\"seq4\",\"syn1\",\"syn2\",\"syn3\",\"syn4\"]},"
            "\"params\":{\"name\":\"Params\","
            "\"knobs\":[\"root\",\"scale\",\"density\",\"chaos\",\"gravity\",\"clk_div\",\"morse_spd\",\"swing\"],"
            "\"params\":[\"root\",\"scale\",\"density\",\"chaos\",\"gravity\",\"clk_div\",\"morse_spd\",\"swing\"]},"
            "\"modulation\":{\"name\":\"Modulation\","
            "\"knobs\":[\"mod_amount\",\"mod_speed\",\"mod_freq\",\"mod_decay\",\"mod_pan\",\"mod_density\",\"mod_shape\",\"mod_reset\"],"
            "\"params\":[\"mod_amount\",\"mod_speed\",\"mod_freq\",\"mod_decay\",\"mod_pan\",\"mod_density\",\"mod_shape\",\"mod_reset\",\"mod_offset\"]},"
            "\"patch\":{\"name\":\"Patch\","
            "\"knobs\":[\"patch\",\"rnd_patch\",\"rnd_rytm\",\"rnd_voices\",\"same_voice\",\"rnd_mod\",\"rnd_pitch\",\"rnd_pan\"],"
            "\"params\":[\"patch\",\"rnd_patch\",\"rnd_rytm\",\"rnd_voices\",\"same_voice\",\"rnd_mod\",\"rnd_pitch\",\"rnd_pan\"]},"
            "\"mix\":{\"name\":\"Mix\","
            "\"knobs\":[\"v1_level\",\"v2_level\",\"v3_level\",\"v4_level\",\"v1_freq\",\"v2_freq\",\"v3_freq\",\"v4_freq\"],"
            "\"params\":[\"v1_level\",\"v2_level\",\"v3_level\",\"v4_level\",\"v1_freq\",\"v2_freq\",\"v3_freq\",\"v4_freq\"]},"
            "\"voice1\":{\"name\":\"Voice 1\","
            "\"knobs\":[\"v1_preset\",\"v1_vol\",\"v1_vfreq\",\"v1_wave\",\"v1_tone\",\"v1_decay\",\"v1_detune\",\"v1_pan\"],"
            "\"params\":[\"v1_preset\",\"v1_vol\",\"v1_vfreq\",\"v1_wave\",\"v1_tone\",\"v1_decay\",\"v1_detune\",\"v1_pan\","
            "\"v1_attack\",\"v1_sub_div\",\"v1_sweep\"]},"
            "\"voice2\":{\"name\":\"Voice 2\","
            "\"knobs\":[\"v2_preset\",\"v2_vol\",\"v2_vfreq\",\"v2_wave\",\"v2_tone\",\"v2_decay\",\"v2_detune\",\"v2_pan\"],"
            "\"params\":[\"v2_preset\",\"v2_vol\",\"v2_vfreq\",\"v2_wave\",\"v2_tone\",\"v2_decay\",\"v2_detune\",\"v2_pan\","
            "\"v2_attack\",\"v2_sub_div\",\"v2_sweep\"]},"
            "\"voice3\":{\"name\":\"Voice 3\","
            "\"knobs\":[\"v3_preset\",\"v3_vol\",\"v3_vfreq\",\"v3_wave\",\"v3_tone\",\"v3_decay\",\"v3_detune\",\"v3_pan\"],"
            "\"params\":[\"v3_preset\",\"v3_vol\",\"v3_vfreq\",\"v3_wave\",\"v3_tone\",\"v3_decay\",\"v3_detune\",\"v3_pan\","
            "\"v3_attack\",\"v3_sub_div\",\"v3_sweep\"]},"
            "\"voice4\":{\"name\":\"Voice 4\","
            "\"knobs\":[\"v4_preset\",\"v4_vol\",\"v4_vfreq\",\"v4_wave\",\"v4_tone\",\"v4_decay\",\"v4_detune\",\"v4_pan\"],"
            "\"params\":[\"v4_preset\",\"v4_vol\",\"v4_vfreq\",\"v4_wave\",\"v4_tone\",\"v4_decay\",\"v4_detune\",\"v4_pan\","
            "\"v4_attack\",\"v4_sub_div\",\"v4_sweep\"]},"
            "\"general\":{\"name\":\"General\","
            "\"knobs\":[\"master_vol\",\"tempo_sync\",\"bpm\",\"bit_crush\",\"bit_rate\",\"stereo_w\",\"drift\",\"jitter\"],"
            "\"params\":[\"master_vol\",\"tempo_sync\",\"bpm\",\"bit_crush\",\"bit_rate\",\"stereo_w\",\"drift\",\"jitter\",\"dc_filter\",\"out_mode\"]}"
            "}}");
    }

    /* Knob overlay (page-aware) */
    if (strncmp(key, "knob_", 5) == 0) {
        int kn = atoi(key + 5) - 1; /* 0-based */
        if (kn < 0 || kn > 7) return -1;
        int pg = inst->current_page;
        if (pg < 0 || pg > 10) pg = 0;
        const char *kkey = KNOB_KEYS[pg][kn];
        if (strstr(key, "_name"))
            return snprintf(buf, buf_len, "%s", kkey);
        if (strstr(key, "_value"))
            return get_param(inst, kkey, buf, buf_len);
        return -1;
    }

    char k[24];

    /* Page 1 */
    for (int v = 0; v < NUM_VOICES; v++) {
        snprintf(k, sizeof(k), "seq%d", v + 1);
        if (strcmp(key, k) == 0) return snprintf(buf, buf_len, "%d", inst->voices[v].seq_preset);
        snprintf(k, sizeof(k), "syn%d", v + 1);
        if (strcmp(key, k) == 0) return snprintf(buf, buf_len, "%d", inst->voices[v].syn_preset);
    }

    /* Page 2 */
    for (int v = 0; v < NUM_VOICES; v++) {
        snprintf(k, sizeof(k), "v%d_level", v + 1);
        if (strcmp(key, k) == 0) return snprintf(buf, buf_len, "%.4f", inst->voices[v].level);
        snprintf(k, sizeof(k), "v%d_freq", v + 1);
        if (strcmp(key, k) == 0) return snprintf(buf, buf_len, "%.2f", inst->voices[v].freq);
    }

    /* Page 3 */
    if (strcmp(key, "root")      == 0) return snprintf(buf, buf_len, "%s", ROOT_NAMES[inst->root]);
    if (strcmp(key, "scale")     == 0) return snprintf(buf, buf_len, "%s", SCALE_NAMES[inst->scale]);
    if (strcmp(key, "density")   == 0) return snprintf(buf, buf_len, "%.4f", inst->density);
    if (strcmp(key, "chaos")     == 0) return snprintf(buf, buf_len, "%.4f", inst->chaos);
    if (strcmp(key, "gravity")   == 0) return snprintf(buf, buf_len, "%.4f", inst->gravity);
    if (strcmp(key, "clk_div")   == 0) return snprintf(buf, buf_len, "%d", inst->clk_div);
    if (strcmp(key, "morse_spd") == 0) return snprintf(buf, buf_len, "%.4f", inst->morse_spd);
    if (strcmp(key, "swing")     == 0) return snprintf(buf, buf_len, "%.4f", inst->swing);

    /* Pages 4–7 */
    for (int v = 0; v < NUM_VOICES; v++) {
        const voice_t *vp = &inst->voices[v];
        snprintf(k, sizeof(k), "v%d_preset", v + 1);
        if (strcmp(key, k) == 0) return snprintf(buf, buf_len, "%d", vp->syn_preset);
        snprintf(k, sizeof(k), "v%d_vol", v + 1);
        if (strcmp(key, k) == 0) return snprintf(buf, buf_len, "%.4f", vp->vol);
        snprintf(k, sizeof(k), "v%d_vfreq", v + 1);
        if (strcmp(key, k) == 0) return snprintf(buf, buf_len, "%.2f", vp->freq);
        snprintf(k, sizeof(k), "v%d_wave", v + 1);
        if (strcmp(key, k) == 0) return snprintf(buf, buf_len, "%s", WAVE_NAMES[vp->wave]);
        snprintf(k, sizeof(k), "v%d_tone", v + 1);
        if (strcmp(key, k) == 0) return snprintf(buf, buf_len, "%.4f", vp->tone);
        snprintf(k, sizeof(k), "v%d_attack", v + 1);
        if (strcmp(key, k) == 0) return snprintf(buf, buf_len, "%.4f", vp->attack);
        snprintf(k, sizeof(k), "v%d_decay", v + 1);
        if (strcmp(key, k) == 0) return snprintf(buf, buf_len, "%.4f", vp->decay);
        snprintf(k, sizeof(k), "v%d_pan", v + 1);
        if (strcmp(key, k) == 0) return snprintf(buf, buf_len, "%.4f", vp->pan);
        snprintf(k, sizeof(k), "v%d_sweep", v + 1);
        if (strcmp(key, k) == 0) return snprintf(buf, buf_len, "%.4f", vp->sweep);
        snprintf(k, sizeof(k), "v%d_detune", v + 1);
        if (strcmp(key, k) == 0) return snprintf(buf, buf_len, "%.4f", vp->detune);
        snprintf(k, sizeof(k), "v%d_sub_div", v + 1);
        if (strcmp(key, k) == 0) return snprintf(buf, buf_len, "%d", vp->sub_div);
    }

    /* Patch page */
    if (strcmp(key, "patch")      == 0) return snprintf(buf, buf_len, "%s", PATCH_NAMES[inst->patch_idx]);
    if (strcmp(key, "rnd_patch")  == 0) return snprintf(buf, buf_len, "0");
    if (strcmp(key, "rnd_rytm")   == 0) return snprintf(buf, buf_len, "0");
    if (strcmp(key, "rnd_voices") == 0) return snprintf(buf, buf_len, "0");
    if (strcmp(key, "same_voice") == 0) return snprintf(buf, buf_len, "0");
    if (strcmp(key, "rnd_mod")    == 0) return snprintf(buf, buf_len, "0");
    if (strcmp(key, "rnd_pitch")  == 0) return snprintf(buf, buf_len, "0");
    if (strcmp(key, "rnd_pan")    == 0) return snprintf(buf, buf_len, "0");

    /* Modulation */
    if (strcmp(key, "mod_amount")  == 0) return snprintf(buf, buf_len, "%.4f", inst->mod_amount);
    if (strcmp(key, "mod_speed")   == 0) return snprintf(buf, buf_len, "%.4f", inst->mod_speed);
    if (strcmp(key, "mod_offset")  == 0) return snprintf(buf, buf_len, "%.4f", inst->mod_offset);
    if (strcmp(key, "mod_freq")    == 0) return snprintf(buf, buf_len, "%.4f", inst->mod_freq);
    if (strcmp(key, "mod_decay")   == 0) return snprintf(buf, buf_len, "%.4f", inst->mod_decay);
    if (strcmp(key, "mod_pan")     == 0) return snprintf(buf, buf_len, "%.4f", inst->mod_pan);
    if (strcmp(key, "mod_density") == 0) return snprintf(buf, buf_len, "%.4f", inst->mod_density);
    if (strcmp(key, "mod_shape")   == 0) return snprintf(buf, buf_len, "%s", MOD_SHAPE_NAMES[inst->mod_shape]);
    if (strcmp(key, "mod_reset")   == 0) return snprintf(buf, buf_len, "0");

    /* Page 8 */
    if (strcmp(key, "master_vol") == 0) return snprintf(buf, buf_len, "%.4f", inst->master_vol);
    if (strcmp(key, "tempo_sync") == 0) return snprintf(buf, buf_len, "%s", inst->tempo_sync ? "Sync" : "Free");
    if (strcmp(key, "bpm")        == 0) return snprintf(buf, buf_len, "%.1f", inst->bpm);
    if (strcmp(key, "stereo_w")   == 0) return snprintf(buf, buf_len, "%.4f", inst->stereo_w);
    if (strcmp(key, "bit_crush")  == 0) return snprintf(buf, buf_len, "%d", inst->bit_crush);
    if (strcmp(key, "bit_rate")   == 0) return snprintf(buf, buf_len, "%.4f", inst->bit_rate);
    if (strcmp(key, "drift")      == 0) return snprintf(buf, buf_len, "%.4f", inst->drift);
    if (strcmp(key, "jitter")     == 0) return snprintf(buf, buf_len, "%.4f", inst->jitter);
    if (strcmp(key, "dc_filter")  == 0) return snprintf(buf, buf_len, "%s", inst->dc_filter ? "On" : "Off");
    if (strcmp(key, "out_mode")   == 0) {
        static const char *modes[] = {"Stereo","Mono","Spread"};
        return snprintf(buf, buf_len, "%s", modes[inst->out_mode]);
    }

    /* State serialization (key=val;key=val;...) */
    if (strcmp(key, "state") == 0) {
        int n = 0;
        int rem;
#define STATE_W(fmt, ...) \
    do { rem = buf_len - n - 1; \
         if (rem <= 0) return n; \
         n += snprintf(buf + n, (size_t)(rem), fmt, ##__VA_ARGS__); } while(0)

        STATE_W("density=%.4f;chaos=%.4f;gravity=%.4f;", inst->density, inst->chaos, inst->gravity);
        STATE_W("clk_div=%d;morse_spd=%.4f;swing=%.4f;", inst->clk_div, inst->morse_spd, inst->swing);
        STATE_W("master_vol=%.4f;stereo_w=%.4f;bit_crush=%d;bit_rate=%.4f;bpm=%.1f;", inst->master_vol, inst->stereo_w, inst->bit_crush, inst->bit_rate, inst->bpm);
        STATE_W("drift=%.4f;jitter=%.4f;", inst->drift, inst->jitter);
        STATE_W("mod_amount=%.4f;mod_speed=%.4f;mod_offset=%.4f;mod_freq=%.4f;mod_decay=%.4f;mod_pan=%.4f;mod_density=%.4f;mod_shape=%s;",
            inst->mod_amount,
            inst->mod_speed, inst->mod_offset, inst->mod_freq, inst->mod_decay,
            inst->mod_pan, inst->mod_density, MOD_SHAPE_NAMES[inst->mod_shape]);
        STATE_W("root=%s;scale=%s;tempo_sync=%s;", ROOT_NAMES[inst->root], SCALE_NAMES[inst->scale], inst->tempo_sync ? "Sync" : "Free");
        for (int v = 0; v < NUM_VOICES; v++) {
            const voice_t *vp = &inst->voices[v];
            STATE_W("seq%d=%d;syn%d=%d;v%d_vol=%.4f;v%d_vfreq=%.2f;v%d_wave=%s;",
                v+1, vp->seq_preset, v+1, vp->syn_preset,
                v+1, vp->vol, v+1, vp->freq, v+1, WAVE_NAMES[vp->wave]);
            STATE_W("v%d_tone=%.4f;v%d_attack=%.4f;v%d_decay=%.4f;v%d_pan=%.4f;v%d_sweep=%.4f;v%d_detune=%.4f;v%d_sub_div=%d;",
                v+1, vp->tone, v+1, vp->attack, v+1, vp->decay, v+1, vp->pan,
                v+1, vp->sweep, v+1, vp->detune, v+1, vp->sub_div);
            STATE_W("v%d_level=%.4f;v%d_freq=%.2f;",
                v+1, vp->level, v+1, vp->freq);
        }
#undef STATE_W
        return n;
    }

    return -1;
}

/* ── render_block ─────────────────────────────────────────────────────────── */
static void render_block(void *instance, int16_t *out_lr, int frames) {
    signal_instance_t *inst = (signal_instance_t *)instance;
    if (!inst) { memset(out_lr, 0, frames * 4); return; }

    float bpm = inst->bpm > 0 ? inst->bpm : 120.0f;
    float samples_per_beat = SAMPLE_RATE * 60.0f / bpm;
    int   sync = inst->tempo_sync;

    for (int i = 0; i < frames; i++) {
        float mix_l = 0.0f, mix_r = 0.0f;

        /* ── Advance per-voice LFOs ── */
        if (inst->mod_speed > 0.0f) {
            float mod_hz   = inst->mod_speed * 10.0f;
            float phase_inc = mod_hz * SR_INV;
            for (int vl = 0; vl < NUM_VOICES; vl++) {
                float prev = inst->vlfo_phase[vl];
                inst->vlfo_phase[vl] += phase_inc;
                if (inst->vlfo_phase[vl] >= 1.0f) {
                    inst->vlfo_phase[vl] -= 1.0f;
                    /* S&H: new random value on zero-crossing */
                    if (inst->vlfo_shape[vl] == MOD_SHAPE_SH) {
                        uint32_t r = xorshift32(&inst->vlfo_rng[vl]);
                        inst->vlfo_sh_val[vl] = (float)(int32_t)r / 2147483648.0f;
                    }
                }
                (void)prev;
                float eff = inst->vlfo_phase[vl]
                          + (float)vl * inst->mod_offset / (float)NUM_VOICES;
                while (eff >= 1.0f) eff -= 1.0f;
                inst->vlfo_value[vl] = mod_lfo_value(eff, inst->vlfo_shape[vl],
                                           &inst->vlfo_sh_val[vl], &inst->vlfo_rng[vl]);
            }
        } else {
            for (int vl = 0; vl < NUM_VOICES; vl++) inst->vlfo_value[vl] = 0.0f;
        }

        for (int v = 0; v < NUM_VOICES; v++) {
            voice_t *vp = &inst->voices[v];
            float lfo_v = inst->vlfo_value[v] * inst->mod_amount;

            /* Skip if sequencer or synth off */
            if (vp->seq_preset == 0 || vp->syn_preset == 0) continue;

            /* ── Free-running clock (used when NOT sync'd) ── */
            if (!sync) {
                int idx = vp->seq_preset - 1;
                if (idx < 0 || idx >= NUM_SEQ_PRESETS) continue;
                const rhythm_pattern_t *pat = &PATTERNS[v][idx];

                float spd_mult = 1.0f;
                if (idx < 5) spd_mult = 0.25f + inst->morse_spd * 1.75f;

                float sps = samples_per_beat / (float)pat->subdivision
                          / (float)inst->clk_div * spd_mult;
                if (sps < 1.0f) sps = 1.0f;

                float swing_off = 0.0f;
                if ((vp->step & 1) && inst->swing > 0.0f)
                    swing_off = sps * inst->swing * 0.5f;

                vp->step_accum += 1.0f;
                if (vp->step_accum >= sps + swing_off) {
                    vp->step_accum -= (sps + swing_off);
                    vp->step = (vp->step + 1) % pat->length;

                    if (pat->hits[vp->step]) {
                        /* Density with optional LFO modulation */
                        float prob = inst->density;
                        if (inst->mod_density > 0.0f) {
                            float lnorm = (lfo_v + 1.0f) * 0.5f; /* 0..1 */
                            prob *= (1.0f - inst->mod_density + inst->mod_density * lnorm);
                        }
                        if (inst->chaos > 0.0f)
                            prob += (randf(&inst->rng) - 0.5f) * inst->chaos;
                        prob = clampf(prob, 0.0f, 1.0f);
                        if (randf(&inst->rng) < prob) {
                            if (inst->gravity > 0.0f && randf(&inst->rng) < inst->gravity) {
                                for (int ov = 0; ov < NUM_VOICES; ov++) {
                                    if (ov != v) inst->voices[ov].step_accum = 0.0f;
                                }
                            }
                            if (inst->jitter > 0.0f)
                                vp->step_accum += (randf(&inst->rng) - 0.5f) * sps * inst->jitter * 0.5f;
                            voice_trigger(vp);
                        }
                    }
                }
            }

            /* ── Envelope (with LFO decay mod) ── */
            /* Decay mod: LFO scales decay from voice value up to max (0.5s).
             * At mod_decay=1, lfo_v=1: decay_t = 0.5 (full max).
             * At mod_decay=1, lfo_v=-1: decay_t = voice's own decay value (floor). */
            float decay_t = vp->decay;
            if (inst->mod_decay > 0.0f) {
                float lfo_norm = (lfo_v + 1.0f) * 0.5f; /* 0..1 */
                float target = vp->decay + (0.5f - vp->decay) * inst->mod_decay;
                decay_t = vp->decay + (target - vp->decay) * lfo_norm;
                decay_t = clampf(decay_t, 0.0001f, 0.5f);
            }

            if (vp->env_stage == 0) {
                float rate = 1.0f / (vp->attack * SAMPLE_RATE);
                vp->env += rate;
                if (vp->env >= 1.0f) { vp->env = 1.0f; vp->env_stage = 1; }
            } else if (vp->env_stage == 1) {
                float rate = 1.0f / (decay_t * SAMPLE_RATE);
                vp->env -= vp->env * rate * 4.0f;
                if (vp->env < 0.0001f) { vp->env = 0.0f; vp->env_stage = -1; }
            }

            if (vp->env <= 0.0f) continue;

            /* ── Frequency: sub_div → sweep → LFO mod → drift → quantize ── */
            float freq = vp->freq / (float)vp->sub_div;

            /* Sweep: pitch arc peaks at 2^sweep octaves up when env=1 */
            if (vp->sweep > 0.0f)
                freq *= powf(2.0f, vp->sweep * vp->env);

            /* LFO frequency modulation (±2 octaves at mod_freq=1) */
            if (inst->mod_freq > 0.0f)
                freq *= powf(2.0f, lfo_v * inst->mod_freq * 2.0f);

            if (inst->drift > 0.0f)
                freq *= 1.0f + (randf(&inst->rng) - 0.5f) * inst->drift * 0.005f;

            freq = quantize_freq(freq, inst->root, inst->scale);
            freq = clampf(freq, 20.0f, 20000.0f);

            /* ── Generate primary sample ── */
            float sample = voice_generate(vp, freq);

            /* ── Heterodyne: blend second detuned sine for beating ── */
            if (vp->detune > 0.0f) {
                float freq2 = freq + vp->detune;
                float p2inc = freq2 * SR_INV;
                float s2 = sinf(vp->phase2 * TWO_PI);
                vp->phase2 += p2inc;
                if (vp->phase2 >= 1.0f) vp->phase2 -= 1.0f;
                sample = (sample + s2) * 0.5f;
            }

            /* ── Tone LP filter (20ms smoothed coefficient) ── */
            vp->tone_smooth += 0.00113f * (vp->tone - vp->tone_smooth); /* ~20ms */
            float lp_coeff = vp->tone_smooth * 0.999f + 0.001f;
            vp->lp_state += lp_coeff * (sample - vp->lp_state) + 1e-20f;
            sample = vp->lp_state;

            /* ── Apply envelope + volume + pad velocity ── */
            sample *= vp->env * vp->vol * vp->level * vp->vel;

            /* ── Constant-power pan (with LFO mod) ── */
            float pan = clampf(vp->pan + lfo_v * inst->mod_pan, -1.0f, 1.0f);
            float angle = (pan + 1.0f) * 0.25f * 3.14159265f;
            mix_l += sample * cosf(angle);
            mix_r += sample * sinf(angle);
        }

        /* Scale for 4 voices */
        mix_l *= 0.35f;
        mix_r *= 0.35f;

        /* Master volume */
        mix_l *= inst->master_vol;
        mix_r *= inst->master_vol;

        /* Output mode */
        if (inst->out_mode == 1) { /* Mono */
            float m = (mix_l + mix_r) * 0.5f;
            mix_l = mix_r = m;
        } else if (inst->out_mode == 2) { /* Spread: M/S widen */
            float mid  = (mix_l + mix_r) * 0.5f;
            float side = (mix_l - mix_r) * 0.5f * 2.0f; /* amplify sides */
            mix_l = mid + side;
            mix_r = mid - side;
        }

        /* Stereo width (M/S) */
        if (inst->stereo_w != 0.5f) {
            float w    = inst->stereo_w * 2.0f;          /* 0=mono, 2=full wide */
            float mid  = (mix_l + mix_r) * 0.5f;
            float side = (mix_l - mix_r) * 0.5f;
            mix_l = mid + side * w;
            mix_r = mid - side * w;
        }

        /* Bit crusher (bit depth) */
        if (inst->bit_crush < 16) {
            float levels = (float)(1 << inst->bit_crush);
            mix_l = floorf(mix_l * levels + 0.5f) / levels;
            mix_r = floorf(mix_r * levels + 0.5f) / levels;
        }

        /* Bit rate (sample-rate decimation: hold 1–44 samples → 44100–1000 Hz) */
        if (inst->bit_rate > 0.0f) {
            float hold = 1.0f + inst->bit_rate * 43.0f;
            inst->bit_rate_accum += 1.0f;
            if (inst->bit_rate_accum >= hold) {
                inst->bit_rate_accum -= hold;
                inst->bit_rate_hold_l = mix_l;
                inst->bit_rate_hold_r = mix_r;
            }
            mix_l = inst->bit_rate_hold_l;
            mix_r = inst->bit_rate_hold_r;
        }

        /* DC blocker (1-pole highpass ~20 Hz) */
        if (inst->dc_filter) {
            static const float DC_COEFF = 0.99956f; /* ~20 Hz at 44100 */
            static const float DC_NORM  = 0.5f * (1.0f + DC_COEFF);
            float xl = mix_l, xr = mix_r;
            float nl = xl + DC_COEFF * inst->dc_x[0];
            float nr = xr + DC_COEFF * inst->dc_x[1];
            mix_l = DC_NORM * (nl - inst->dc_x[0]);
            mix_r = DC_NORM * (nr - inst->dc_x[1]);
            inst->dc_x[0] = nl; inst->dc_x[1] = nr;
            /* Denormal protection */
            if (fabsf(inst->dc_x[0]) < 1e-20f) inst->dc_x[0] = 0.0f;
            if (fabsf(inst->dc_x[1]) < 1e-20f) inst->dc_x[1] = 0.0f;
        }

        /* Clamp and write */
        out_lr[i * 2]     = (int16_t)(clampf(mix_l, -1.0f, 1.0f) * 32767.0f);
        out_lr[i * 2 + 1] = (int16_t)(clampf(mix_r, -1.0f, 1.0f) * 32767.0f);
    }
}

/* ── Plugin API export ────────────────────────────────────────────────────── */
plugin_api_v2_t *move_plugin_init_v2(const void *host) {
    (void)host;
    static plugin_api_v2_t api = {
        .api_version      = 2,
        .create_instance  = create_instance,
        .destroy_instance = destroy_instance,
        .on_midi          = on_midi,
        .set_param        = set_param,
        .get_param        = get_param,
        .get_error        = NULL,
        .render_block     = render_block,
    };
    return &api;
}
