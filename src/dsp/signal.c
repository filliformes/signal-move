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
#define NUM_SYN_PRESETS 25

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
       WAVE_CLICK, WAVE_SQUARE, WAVE_TRI, WAVE_FM, WAVE_COUNT };
static const char *WAVE_NAMES[] = {
    "Sine","Impulse","Noise","Damped","Click","Square","Tri","FM"
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

enum { PAGE_ROOT=0, PAGE_GENERATE, PAGE_MIX, PAGE_PARAMS,
       PAGE_VOICE1, PAGE_VOICE2, PAGE_VOICE3, PAGE_VOICE4, PAGE_GENERAL };
static const char *PAGE_NAMES[] = {
    "Signal","generate","mix","params","voice1","voice2","voice3","voice4","general"
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

/* 25 shared micro-synth presets */
static const synth_preset_t SYN_PRESETS[NUM_SYN_PRESETS] = {
    /* 0-4: Pure sine pips — Ikeda test tone aesthetic */
    {WAVE_SINE,    0.0001f, 0.001f, 1.0f, 0, 0},
    {WAVE_SINE,    0.0001f, 0.003f, 0.9f, 0, 0},
    {WAVE_SINE,    0.0001f, 0.007f, 0.8f, 0, 0},
    {WAVE_SINE,    0.0002f, 0.012f, 0.7f, 0, 0},
    {WAVE_SINE,    0.0005f, 0.020f, 0.6f, 0, 0},
    /* 5-9: Damped sinusoids — Bernier tuning fork / frequencies */
    {WAVE_DAMPED,  0.0001f, 0.008f, 0.2f, 0, 0},
    {WAVE_DAMPED,  0.0001f, 0.015f, 0.3f, 0, 0},
    {WAVE_DAMPED,  0.0001f, 0.025f, 0.4f, 0, 0},
    {WAVE_DAMPED,  0.0001f, 0.040f, 0.6f, 0, 0},
    {WAVE_DAMPED,  0.0001f, 0.070f, 0.8f, 0, 0},
    /* 10-14: Noise/click bursts — percussion atoms */
    {WAVE_CLICK,   0.0001f, 0.0005f, 0.5f, 0, 0},
    {WAVE_NOISE,   0.0001f, 0.001f,  0.3f, 0, 0},
    {WAVE_NOISE,   0.0001f, 0.003f,  0.5f, 0, 0},
    {WAVE_NOISE,   0.0001f, 0.006f,  0.7f, 0, 0},
    {WAVE_IMPULSE, 0.0001f, 0.002f,  0.9f, 0, 0},
    /* 15-19: FM micro-textures — metallic/bell */
    {WAVE_FM, 0.0002f, 0.004f, 0.5f, 1.0f, 0.5f},
    {WAVE_FM, 0.0002f, 0.006f, 0.6f, 1.5f, 1.0f},
    {WAVE_FM, 0.0002f, 0.010f, 0.7f, 2.0f, 1.5f},
    {WAVE_FM, 0.0002f, 0.018f, 0.8f, 3.0f, 2.0f},
    {WAVE_FM, 0.0005f, 0.030f, 0.9f, 5.0f, 3.0f},
    /* 20-24: Square/triangle digital glitches */
    {WAVE_SQUARE, 0.0001f, 0.0005f, 0.8f, 0, 0},
    {WAVE_SQUARE, 0.0001f, 0.001f,  0.9f, 0, 0},
    {WAVE_TRI,    0.0001f, 0.001f,  0.6f, 0, 0},
    {WAVE_TRI,    0.0001f, 0.003f,  0.7f, 0, 0},
    {WAVE_TRI,    0.0002f, 0.006f,  0.8f, 0, 0}
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

    /* Tone LP filter state */
    float  lp_state;

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

    /* Clock/transport */
    float    bpm;           /* host-provided or default */
    int      midi_running;  /* MIDI transport running */

    /* DC blocker state (per channel) */
    float  dc_x[2];

    /* PRNG for density/chaos/jitter */
    uint32_t rng;

    /* Current UI page (for knob overlay) */
    int    current_page;
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
    v->env_stage = 0; /* attack */
    v->phase     = 0.0f;
    v->fm_phase  = 0.0f;
    v->lp_state  = 0.0f;
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
            s = s * 1664525u + 1013904223u; /* LCG */
            v->noise_state = s;
            sample = (float)(int32_t)s * (1.0f / 2147483648.0f);
            break;
        }
        case WAVE_DAMPED:
            /* Tuning-fork style: damping controlled by tone (0=fast, 1=slow) */
            sample = sinf(v->phase * TWO_PI) *
                     expf(-v->phase * (1.0f - v->tone) * 30.0f);
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
        default:
            sample = 0.0f;
    }

    v->phase += p_inc;
    if (v->phase >= 1.0f) v->phase -= 1.0f;
    return sample;
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
        vp->tone       = 0.5f;
        vp->pan        = (v - 1.5f) * 0.4f; /* spread -0.6 -0.2 +0.2 +0.6 */
        vp->noise_state = 12345u + (uint32_t)v * 111u;
        vp->env_stage  = -1; /* idle */
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
                    /* Density + chaos gate */
                    float prob = inst->density;
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

    /* Note On: re-sync all voices */
    if (ch_status == 0x90 && msg[2] > 0) {
        for (int v = 0; v < NUM_VOICES; v++) {
            inst->voices[v].step       = 0;
            inst->voices[v].step_accum = 0.0f;
            inst->voices[v].tick_accum = 0.0f;
        }
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
        if (strcmp(key, k) == 0) { inst->voices[v].decay = clampf(atof(val), 0.0001f, 0.1f); return; }
        snprintf(k, sizeof(k), "v%d_pan", v + 1);
        if (strcmp(key, k) == 0) { inst->voices[v].pan = clampf(atof(val), -1, 1); return; }
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
    "{\"key\":\"syn1\",\"name\":\"Syn 1\",\"type\":\"int\",\"min\":0,\"max\":25,\"step\":1},"
    "{\"key\":\"syn2\",\"name\":\"Syn 2\",\"type\":\"int\",\"min\":0,\"max\":25,\"step\":1},"
    "{\"key\":\"syn3\",\"name\":\"Syn 3\",\"type\":\"int\",\"min\":0,\"max\":25,\"step\":1},"
    "{\"key\":\"syn4\",\"name\":\"Syn 4\",\"type\":\"int\",\"min\":0,\"max\":25,\"step\":1},"
    "{\"key\":\"v1_level\",\"name\":\"V1 Level\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v2_level\",\"name\":\"V2 Level\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v3_level\",\"name\":\"V3 Level\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v4_level\",\"name\":\"V4 Level\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v1_freq\",\"name\":\"V1 Freq\",\"type\":\"float\",\"min\":20,\"max\":20000,\"step\":0.01},"
    "{\"key\":\"v2_freq\",\"name\":\"V2 Freq\",\"type\":\"float\",\"min\":20,\"max\":20000,\"step\":0.01},"
    "{\"key\":\"v3_freq\",\"name\":\"V3 Freq\",\"type\":\"float\",\"min\":20,\"max\":20000,\"step\":0.01},"
    "{\"key\":\"v4_freq\",\"name\":\"V4 Freq\",\"type\":\"float\",\"min\":20,\"max\":20000,\"step\":0.01},"
    "{\"key\":\"root\",\"name\":\"Root\",\"type\":\"enum\",\"options\":[\"Free\",\"C\",\"C#\",\"D\",\"D#\",\"E\",\"F\",\"F#\",\"G\",\"G#\",\"A\",\"A#\",\"B\"]},"
    "{\"key\":\"scale\",\"name\":\"Scale\",\"type\":\"enum\",\"options\":[\"Free\",\"Chromatic\",\"Major\",\"Minor\",\"Pentatonic\",\"WholeTone\",\"Diminished\",\"HarmMin\"]},"
    "{\"key\":\"density\",\"name\":\"Density\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"chaos\",\"name\":\"Chaos\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"gravity\",\"name\":\"Gravity\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"clk_div\",\"name\":\"Clk Div\",\"type\":\"int\",\"min\":1,\"max\":32,\"step\":1},"
    "{\"key\":\"morse_spd\",\"name\":\"Morse Spd\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"swing\",\"name\":\"Swing\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v1_preset\",\"name\":\"V1 Preset\",\"type\":\"int\",\"min\":0,\"max\":25,\"step\":1},"
    "{\"key\":\"v1_vol\",\"name\":\"V1 Vol\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v1_vfreq\",\"name\":\"V1 Freq\",\"type\":\"float\",\"min\":20,\"max\":20000,\"step\":0.01},"
    "{\"key\":\"v1_wave\",\"name\":\"V1 Wave\",\"type\":\"enum\",\"options\":[\"Sine\",\"Impulse\",\"Noise\",\"Damped\",\"Click\",\"Square\",\"Tri\",\"FM\"]},"
    "{\"key\":\"v1_tone\",\"name\":\"V1 Tone\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v1_attack\",\"name\":\"V1 Attack\",\"type\":\"float\",\"min\":0.0001,\"max\":0.05,\"step\":0.0001},"
    "{\"key\":\"v1_decay\",\"name\":\"V1 Decay\",\"type\":\"float\",\"min\":0.0001,\"max\":0.1,\"step\":0.0001},"
    "{\"key\":\"v1_pan\",\"name\":\"V1 Pan\",\"type\":\"float\",\"min\":-1,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v2_preset\",\"name\":\"V2 Preset\",\"type\":\"int\",\"min\":0,\"max\":25,\"step\":1},"
    "{\"key\":\"v2_vol\",\"name\":\"V2 Vol\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v2_vfreq\",\"name\":\"V2 Freq\",\"type\":\"float\",\"min\":20,\"max\":20000,\"step\":0.01},"
    "{\"key\":\"v2_wave\",\"name\":\"V2 Wave\",\"type\":\"enum\",\"options\":[\"Sine\",\"Impulse\",\"Noise\",\"Damped\",\"Click\",\"Square\",\"Tri\",\"FM\"]},"
    "{\"key\":\"v2_tone\",\"name\":\"V2 Tone\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v2_attack\",\"name\":\"V2 Attack\",\"type\":\"float\",\"min\":0.0001,\"max\":0.05,\"step\":0.0001},"
    "{\"key\":\"v2_decay\",\"name\":\"V2 Decay\",\"type\":\"float\",\"min\":0.0001,\"max\":0.1,\"step\":0.0001},"
    "{\"key\":\"v2_pan\",\"name\":\"V2 Pan\",\"type\":\"float\",\"min\":-1,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v3_preset\",\"name\":\"V3 Preset\",\"type\":\"int\",\"min\":0,\"max\":25,\"step\":1},"
    "{\"key\":\"v3_vol\",\"name\":\"V3 Vol\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v3_vfreq\",\"name\":\"V3 Freq\",\"type\":\"float\",\"min\":20,\"max\":20000,\"step\":0.01},"
    "{\"key\":\"v3_wave\",\"name\":\"V3 Wave\",\"type\":\"enum\",\"options\":[\"Sine\",\"Impulse\",\"Noise\",\"Damped\",\"Click\",\"Square\",\"Tri\",\"FM\"]},"
    "{\"key\":\"v3_tone\",\"name\":\"V3 Tone\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v3_attack\",\"name\":\"V3 Attack\",\"type\":\"float\",\"min\":0.0001,\"max\":0.05,\"step\":0.0001},"
    "{\"key\":\"v3_decay\",\"name\":\"V3 Decay\",\"type\":\"float\",\"min\":0.0001,\"max\":0.1,\"step\":0.0001},"
    "{\"key\":\"v3_pan\",\"name\":\"V3 Pan\",\"type\":\"float\",\"min\":-1,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v4_preset\",\"name\":\"V4 Preset\",\"type\":\"int\",\"min\":0,\"max\":25,\"step\":1},"
    "{\"key\":\"v4_vol\",\"name\":\"V4 Vol\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v4_vfreq\",\"name\":\"V4 Freq\",\"type\":\"float\",\"min\":20,\"max\":20000,\"step\":0.01},"
    "{\"key\":\"v4_wave\",\"name\":\"V4 Wave\",\"type\":\"enum\",\"options\":[\"Sine\",\"Impulse\",\"Noise\",\"Damped\",\"Click\",\"Square\",\"Tri\",\"FM\"]},"
    "{\"key\":\"v4_tone\",\"name\":\"V4 Tone\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
    "{\"key\":\"v4_attack\",\"name\":\"V4 Attack\",\"type\":\"float\",\"min\":0.0001,\"max\":0.05,\"step\":0.0001},"
    "{\"key\":\"v4_decay\",\"name\":\"V4 Decay\",\"type\":\"float\",\"min\":0.0001,\"max\":0.1,\"step\":0.0001},"
    "{\"key\":\"v4_pan\",\"name\":\"V4 Pan\",\"type\":\"float\",\"min\":-1,\"max\":1,\"step\":0.01},"
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
static const char *KNOB_KEYS[9][8] = {
    /* PAGE_ROOT — mirrors generate so chain_edit hover still controls seq/syn */
    {"seq1","seq2","seq3","seq4","syn1","syn2","syn3","syn4"},
    /* PAGE_GENERATE */
    {"seq1","seq2","seq3","seq4","syn1","syn2","syn3","syn4"},
    /* PAGE_MIX */
    {"v1_level","v2_level","v3_level","v4_level","v1_freq","v2_freq","v3_freq","v4_freq"},
    /* PAGE_PARAMS */
    {"root","scale","density","chaos","gravity","clk_div","morse_spd","swing"},
    /* PAGE_VOICE1 */
    {"v1_preset","v1_vol","v1_vfreq","v1_wave","v1_tone","v1_attack","v1_decay","v1_pan"},
    /* PAGE_VOICE2 */
    {"v2_preset","v2_vol","v2_vfreq","v2_wave","v2_tone","v2_attack","v2_decay","v2_pan"},
    /* PAGE_VOICE3 */
    {"v3_preset","v3_vol","v3_vfreq","v3_wave","v3_tone","v3_attack","v3_decay","v3_pan"},
    /* PAGE_VOICE4 */
    {"v4_preset","v4_vol","v4_vfreq","v4_wave","v4_tone","v4_attack","v4_decay","v4_pan"},
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
            "{\"level\":\"mix\",\"label\":\"Mix\"},"
            "{\"level\":\"params\",\"label\":\"Params\"},"
            "{\"level\":\"voice1\",\"label\":\"Voix 1\"},"
            "{\"level\":\"voice2\",\"label\":\"Voix 2\"},"
            "{\"level\":\"voice3\",\"label\":\"Voix 3\"},"
            "{\"level\":\"voice4\",\"label\":\"Voix 4\"},"
            "{\"level\":\"general\",\"label\":\"General\"}"
            "]},"
            "\"generate\":{\"name\":\"Generate\","
            "\"knobs\":[\"seq1\",\"seq2\",\"seq3\",\"seq4\",\"syn1\",\"syn2\",\"syn3\",\"syn4\"],"
            "\"params\":[\"seq1\",\"seq2\",\"seq3\",\"seq4\",\"syn1\",\"syn2\",\"syn3\",\"syn4\"]},"
            "\"mix\":{\"name\":\"Mix\","
            "\"knobs\":[\"v1_level\",\"v2_level\",\"v3_level\",\"v4_level\",\"v1_freq\",\"v2_freq\",\"v3_freq\",\"v4_freq\"],"
            "\"params\":[\"v1_level\",\"v2_level\",\"v3_level\",\"v4_level\",\"v1_freq\",\"v2_freq\",\"v3_freq\",\"v4_freq\"]},"
            "\"params\":{\"name\":\"Params\","
            "\"knobs\":[\"root\",\"scale\",\"density\",\"chaos\",\"gravity\",\"clk_div\",\"morse_spd\",\"swing\"],"
            "\"params\":[\"root\",\"scale\",\"density\",\"chaos\",\"gravity\",\"clk_div\",\"morse_spd\",\"swing\"]},"
            "\"voice1\":{\"name\":\"Voix 1\","
            "\"knobs\":[\"v1_preset\",\"v1_vol\",\"v1_vfreq\",\"v1_wave\",\"v1_tone\",\"v1_attack\",\"v1_decay\",\"v1_pan\"],"
            "\"params\":[\"v1_preset\",\"v1_vol\",\"v1_vfreq\",\"v1_wave\",\"v1_tone\",\"v1_attack\",\"v1_decay\",\"v1_pan\"]},"
            "\"voice2\":{\"name\":\"Voix 2\","
            "\"knobs\":[\"v2_preset\",\"v2_vol\",\"v2_vfreq\",\"v2_wave\",\"v2_tone\",\"v2_attack\",\"v2_decay\",\"v2_pan\"],"
            "\"params\":[\"v2_preset\",\"v2_vol\",\"v2_vfreq\",\"v2_wave\",\"v2_tone\",\"v2_attack\",\"v2_decay\",\"v2_pan\"]},"
            "\"voice3\":{\"name\":\"Voix 3\","
            "\"knobs\":[\"v3_preset\",\"v3_vol\",\"v3_vfreq\",\"v3_wave\",\"v3_tone\",\"v3_attack\",\"v3_decay\",\"v3_pan\"],"
            "\"params\":[\"v3_preset\",\"v3_vol\",\"v3_vfreq\",\"v3_wave\",\"v3_tone\",\"v3_attack\",\"v3_decay\",\"v3_pan\"]},"
            "\"voice4\":{\"name\":\"Voix 4\","
            "\"knobs\":[\"v4_preset\",\"v4_vol\",\"v4_vfreq\",\"v4_wave\",\"v4_tone\",\"v4_attack\",\"v4_decay\",\"v4_pan\"],"
            "\"params\":[\"v4_preset\",\"v4_vol\",\"v4_vfreq\",\"v4_wave\",\"v4_tone\",\"v4_attack\",\"v4_decay\",\"v4_pan\"]},"
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
        if (pg < 0 || pg > 8) pg = 0;
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
    }

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
        STATE_W("root=%s;scale=%s;tempo_sync=%s;", ROOT_NAMES[inst->root], SCALE_NAMES[inst->scale], inst->tempo_sync ? "Sync" : "Free");
        for (int v = 0; v < NUM_VOICES; v++) {
            const voice_t *vp = &inst->voices[v];
            STATE_W("seq%d=%d;syn%d=%d;v%d_vol=%.4f;v%d_vfreq=%.2f;v%d_wave=%s;",
                v+1, vp->seq_preset, v+1, vp->syn_preset,
                v+1, vp->vol, v+1, vp->freq, v+1, WAVE_NAMES[vp->wave]);
            STATE_W("v%d_tone=%.4f;v%d_attack=%.4f;v%d_decay=%.4f;v%d_pan=%.4f;",
                v+1, vp->tone, v+1, vp->attack, v+1, vp->decay, v+1, vp->pan);
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

        for (int v = 0; v < NUM_VOICES; v++) {
            voice_t *vp = &inst->voices[v];

            /* Skip if sequencer or synth off */
            if (vp->seq_preset == 0 || vp->syn_preset == 0) continue;

            /* ── Free-running clock (used when NOT sync'd) ── */
            if (!sync) {
                int idx = vp->seq_preset - 1;
                if (idx < 0 || idx >= NUM_SEQ_PRESETS) continue;
                const rhythm_pattern_t *pat = &PATTERNS[v][idx];

                float spd_mult = 1.0f;
                if (idx < 5) spd_mult = 0.25f + inst->morse_spd * 1.75f;

                /* samples per step = (60/BPM) * SR / (subdivision * clk_div) */
                float sps = samples_per_beat / (float)pat->subdivision
                          / (float)inst->clk_div * spd_mult;
                if (sps < 1.0f) sps = 1.0f;

                /* Swing: delay odd steps */
                float swing_off = 0.0f;
                if ((vp->step & 1) && inst->swing > 0.0f)
                    swing_off = sps * inst->swing * 0.5f;

                vp->step_accum += 1.0f;
                if (vp->step_accum >= sps + swing_off) {
                    vp->step_accum -= (sps + swing_off);
                    vp->step = (vp->step + 1) % pat->length;

                    if (pat->hits[vp->step]) {
                        float prob = inst->density;
                        if (inst->chaos > 0.0f)
                            prob += (randf(&inst->rng) - 0.5f) * inst->chaos;
                        prob = clampf(prob, 0.0f, 1.0f);
                        if (randf(&inst->rng) < prob) {
                            /* Gravity: on trigger, maybe re-sync other voices */
                            if (inst->gravity > 0.0f && randf(&inst->rng) < inst->gravity) {
                                for (int ov = 0; ov < NUM_VOICES; ov++) {
                                    if (ov != v) inst->voices[ov].step_accum = 0.0f;
                                }
                            }
                            /* Jitter: randomize timing slightly */
                            if (inst->jitter > 0.0f)
                                vp->step_accum += (randf(&inst->rng) - 0.5f) * sps * inst->jitter * 0.5f;
                            voice_trigger(vp);
                        }
                    }
                }
            }

            /* ── Envelope ── */
            if (vp->env_stage == 0) {
                float rate = 1.0f / (vp->attack * SAMPLE_RATE);
                vp->env += rate;
                if (vp->env >= 1.0f) { vp->env = 1.0f; vp->env_stage = 1; }
            } else if (vp->env_stage == 1) {
                /* Exponential decay */
                float rate = 1.0f / (vp->decay * SAMPLE_RATE);
                vp->env -= vp->env * rate * 4.0f; /* ~4 time-constants */
                if (vp->env < 0.0001f) { vp->env = 0.0f; vp->env_stage = -1; }
            }

            if (vp->env <= 0.0f) continue;

            /* ── Frequency: use vp->freq (voice detail page), quantize if needed ── */
            float freq = vp->freq;

            /* Apply drift (analog-style pitch instability) */
            if (inst->drift > 0.0f)
                freq *= 1.0f + (randf(&inst->rng) - 0.5f) * inst->drift * 0.005f;

            /* Scale quantization */
            freq = quantize_freq(freq, inst->root, inst->scale);
            freq = clampf(freq, 20.0f, 20000.0f);

            /* ── Generate sample ── */
            float sample = voice_generate(vp, freq);

            /* ── One-pole LP tone filter (0=dark, 1=bright) ── */
            /* At tone=1: coeff=1 (bypass), tone=0: coeff≈0.001 (very dark) */
            float lp_coeff = vp->tone * 0.999f + 0.001f;
            vp->lp_state += lp_coeff * (sample - vp->lp_state) + 1e-20f;
            sample = vp->lp_state;

            /* ── Apply envelope + volume ── */
            sample *= vp->env * vp->vol * vp->level;

            /* ── Constant-power pan ── */
            float angle = (vp->pan + 1.0f) * 0.25f * 3.14159265f;
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
