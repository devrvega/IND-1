/* SPDX-License-Identifier: GPL-3.0-only */
/* The instrument: a self-playing industrial machine, after the late-80s / 90s records of KMFDM, Nine Inch
 * Nails and Ministry. Six layers play one root (the keys move it, on the bar) through one mode, in songs
 * the machine arranges for itself:
 *
 *   DRUMS   a distorted 909-ish kick, a snare (or a clap), hats, toms for the fills: six grooves (EBM's
 *           four on the floor, a rock beat, a sampled break, half time, thrash, a stomp), through a
 *           bit and sample-rate crusher (the EMAX / SP-12 crunch). A gated reverb on the snare.
 *   BASS    a Pro-1 / Minimoog-style mono synth: a saw and a sub square through a resonant low-pass with
 *           its own envelope, driven; a sequence of sixteenths (pulses, gallops, octaves, the b2).
 *   GUITAR  two synthesized guitars, left and right: power chords through a high-gain shaper and a
 *           cabinet; palm-muted chugs and open chords on a riff.
 *   SYNTH   stabs and short lines: FM with feedback and detuned saws, morphed by Tone (as the Xpander and
 *           the Prophet VS), each note sweeping from a bright attack to its body.
 *   METAL   inharmonic FM clangs and sheet-metal noise on two Euclidean patterns (polyrhythms).
 *   NOISE   a drone (low saws and the tritone) or static, the intros' and breakdowns' bed; risers into
 *           a drop, an impact on it.
 *
 * The ARRANGER plays songs: intro, verses, builds, choruses, a breakdown, a drop, an outro; then a new
 * song from new material. Each section decides which layers play; the last bar of a section fills; a
 * build rolls the snare and opens the filters. Arrange sets the sections' lengths (or loops one).
 *
 * A piece's SONG (en_song) is all of it: three parts (A the verse, also the intro, the build and the outro;
 * B the chorus, also the drop; C the breakdown), each a bar of every layer and four chords, then the plan
 * (the sections in order, their lengths, the layers in each) and the metal's two rings. While the piece is
 * on auto the machine writes it, and rewrites it as it goes; locked, nothing changes by itself, and the
 * player edits it (the UI writes en_song directly; the render plays it from the next bar, or at once after
 * en_rebuild).
 * Then: a ping-pong delay (a dotted eighth), Dattorro's plate (from FoMni) behind a gate, and the kick
 * pumping the rest (Pump).
 *
 * Threads: en_render() runs in the audio interrupt. Everything else posts commands, which the render
 * drains at the start of a block; the state below marked (UI) is written by the render and only read
 * elsewhere. Float only, no libm: fastmath.h. */
#pragma once
#include <stdint.h>

#define EN_SR 44100.0f
#define EN_NSTEP 16                    /* a bar of sixteenths */

/* the layers (each has a page, a level and a mute) */
enum { LY_DRUMS, LY_BASS, LY_GUITAR, LY_SYNTH, LY_METAL, LY_NOISE, EN_NLAYER };
extern const char *const EN_LAYER_NAME[EN_NLAYER];

/* modes: pitch classes relative to the root (bit i: root + i semitones) */
enum { MODE_PHRYGIAN, MODE_AEOLIAN, MODE_LOCRIAN, MODE_PHRYG_DOM, MODE_HARM_MIN, MODE_DORIAN, MODE_BLUES,
       MODE_OCTATONIC, EN_NMODES };
extern const char *const EN_MODE_NAME[EN_NMODES];
extern const uint16_t EN_MODE_POOL[EN_NMODES];
extern const char *const EN_NOTE_NAME[12];

/* the sections of a song */
enum { S_INTRO, S_VERSE, S_BUILD, S_CHORUS, S_BREAK, S_DROP, S_OUTRO, EN_NSECT };
extern const char *const EN_SECT_NAME[EN_NSECT];

/* the drum grooves */
enum { G_EBM, G_ROCK, G_BREAK, G_HALF, G_THRASH, G_STOMP, EN_NGROOVE };

/* the arranger's lengths */
enum { A_LOOP, A_LONG, A_NORMAL, A_SHORT, EN_NARRANGE };

/* parameters: value ranges in en_param_info(); four a page, in the pages' order */
enum {
    P_TEMPO, P_ARRANGE, P_EVOLVE, P_INTENSITY,        /* machine */
    P_DRUMS, P_GROOVE, P_CRUSH, P_KICK,               /* drums: level, groove, crusher, the kick's drive */
    P_BASS, P_CUTOFF, P_SQUELCH, P_BDRIVE,            /* bass: level, filter, envelope and resonance, drive */
    P_GUITAR, P_GAIN, P_CHUG, P_RIFF,                 /* guitar: level, gain, palm mute, how busy */
    P_SYNTH, P_STONE, P_SDECAY, P_SBUSY,              /* synth: level, tone (saw .. FM .. clang), decay, busy */
    P_METAL, P_HITS, P_NOISE, P_TEXTURE,              /* metal: level, hits; noise: level, drone .. static */
    P_REVERB, P_GATE, P_DELAY, P_PUMP,                /* space */
    P_ROOT, P_TUNE, P_MIDI, P_MODE,                   /* setup; the mode (ALGORITHM) */
    P_NPARAMS
};
typedef struct {
    const char *name;
    int16_t lo, hi, def;
} en_param_t;
const en_param_t *en_param_info(int i);
void en_param_text(int i, int v, char *buf);   /* the value as the screen shows it (10 bytes) */
extern const uint8_t EN_LAYER_PARAM[EN_NLAYER]; /* each layer's level */

/* ------------------------------------------------------------- the song --- */
#define EN_NPARTS 3                    /* A the verse (intro, build, outro), B the chorus (drop), C the breakdown */
#define EN_PLAN_MAX 16                 /* sections in a song */
#define EN_MAX_BARS 32                 /* a section's length, at most */
#define EN_REST (-128)
enum { EN_F_ACC = 1, EN_F_SLIDE = 2, EN_F_LONG = 4, EN_F_OPEN = 8, EN_F_OCT = 16, EN_F_LEAD = 32 };
typedef struct {
    int8_t n;                          /* a scale degree above the bar's chord (-7..14), EN_REST: none */
    uint8_t f;                         /* EN_F_*: an accent; the bass slides into it; long (a held note, the
                                          synth's chord rings); the guitar's open chord (else a palm-muted chug);
                                          the bass an octave up; the synth one voice an octave up (a line) */
} en_note_t;
enum { EN_HAT_CLOSED, EN_HAT_OPEN, EN_HAT_CRASH };
enum { EN_TOM_HIGH, EN_TOM_MID, EN_TOM_LOW };
#define EN_VEL_GHOST 40
#define EN_VEL_HIT 92
#define EN_VEL_ACC 120
typedef struct {
    uint8_t kick[EN_NSTEP], snare[EN_NSTEP], hat[EN_NSTEP], tom[EN_NSTEP];   /* velocities, 0 none */
    uint8_t hat_kind[EN_NSTEP], tom_kind[EN_NSTEP];                         /* EN_HAT_*, EN_TOM_* */
    en_note_t bass[EN_NSTEP], gtr[EN_NSTEP], syn[EN_NSTEP];
    int8_t prog[4];                    /* the chords: scale degrees, each a quarter of the section */
} en_part_t;
enum { EN_TEX_KNOB, EN_TEX_DRONE, EN_TEX_STATIC, EN_TEX_BOTH, EN_NTEX };
typedef struct {
    uint8_t type;                      /* S_* */
    uint8_t bars;                      /* 1..EN_MAX_BARS; 0: as Arrange says */
    uint8_t layers;                    /* bit l: layer l plays */
    uint8_t tex;                       /* the noise layer's bed: EN_TEX_* (KNOB: as Texture says) */
} en_entry_t;
typedef struct {
    en_part_t part[EN_NPARTS];
    en_entry_t plan[EN_PLAN_MAX];      /* the sections, in order */
    uint8_t plan_n;                    /* 1..EN_PLAN_MAX */
    uint8_t locked;                    /* 0 auto: the machine writes it; 1 locked: nothing changes by itself */
    uint8_t ring_len[2], ring_hits[2], ring_turn[2];   /* the metal's two Euclidean rings: the anvil, the scrap */
    uint8_t metal_tone;                /* the clangs' pitches */
    uint8_t rsv[7];
} en_song_t;
extern en_song_t en_song;              /* the piece's song, as it plays (see above) */
int en_part_of(int sect);              /* the part a section plays (0 A, 1 B, 2 C) */
extern const char *const EN_PART_NAME[EN_NPARTS];
int en_song_valid(const en_song_t *s);
void en_song_defaults(en_song_t *s);   /* an empty song: silent parts, a plan of one verse; auto */

/* commands (main loop -> render) */
void en_init(void);
void en_lock(int on);                  /* locked: nothing changes by itself; auto: the machine writes on */
void en_song_loaded(void);             /* en_song was replaced (another piece): from the top; on auto the
                                          machine writes it anew first */
void en_rebuild(void);                 /* en_song was edited: what plays now follows it */
void en_turned(int param);             /* the player turned a knob that writes the song: Groove (the drums;
                                          on auto the bass and the guitar too), Riff (the guitar), Busy (the
                                          synth), Hits (the rings' hits). At the bar */
void en_audition(int layer, int what, int flags);  /* a sound, now: drums EN_HIT_*; bass, guitar, synth:
                                          scale degree what, EN_F_* flags; metal: 0 the anvil, 1 scrap */
enum { EN_HIT_KICK, EN_HIT_SNARE, EN_HIT_HAT, EN_HIT_OPEN, EN_HIT_CRASH, EN_HIT_TOM_HIGH, EN_HIT_TOM_MID,
       EN_HIT_TOM_LOW, EN_NHIT };
void en_set(int param, int value);             /* P_ROOT while playing: on the next bar */
void en_play(int on);                          /* run / stop (the tails ring on) */
void en_next(void);                            /* the next section: the rest of this bar fills, then it */
void en_break(void);                           /* a breakdown; in one, the build; in the build, the drop */
void en_fill(void);                            /* the rest of this bar: a fill */
void en_mutate(void);                          /* new material from the next bar: on auto all of it (riffs,
                                                  lines, patterns, chords); locked, the lines of the part
                                                  playing */
void en_mute(int layer, int on);
void en_seed(uint32_t s);                      /* stir some entropy into the generators */
void en_panic(void);                           /* everything quiet, now */
void en_regrow(void);                          /* auto, and a new song written from the top */
/* MIDI clock in (as FoMni): ticks drive the 1/16 grid (6 a step) while they keep coming */
enum { EN_CLK_TICK, EN_CLK_START, EN_CLK_CONTINUE, EN_CLK_STOP };
void en_clock(int msg);

/* the render (audio ISR): n stereo frames, 24-bit in int32; gain Q12 (the MASTER pot) */
void en_render(int32_t *out_lr, uint32_t n, uint32_t gain_q12);

/* MIDI out, from the render: the app sends it (USB); status, data 1, data 2 */
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2);

/* state for the screen (UI) */
enum { EN_ROW_KICK, EN_ROW_SNARE, EN_ROW_HAT, EN_ROW_METAL, EN_ROW_BASS, EN_ROW_GUITAR, EN_ROW_SYNTH, EN_NROW };
extern volatile uint8_t en_playing, en_ext;
extern volatile uint8_t en_sect;               /* the section playing (S_*) */
extern volatile uint8_t en_sect_bar;           /* its bar, from 0 */
extern volatile uint8_t en_sect_len;           /* its length in bars; 0 = looping (Arrange: Loop) */
extern volatile uint8_t en_plan[EN_PLAN_MAX];  /* the song: its sections, in order */
extern volatile uint8_t en_plan_len[EN_PLAN_MAX];   /* and their lengths in bars (0: it loops) */
extern volatile uint8_t en_plan_n, en_plan_pos;     /* how many; which plays (a jump plays outside the plan) */
extern volatile uint8_t en_jumped;             /* the section playing came from a button, not the plan */
extern volatile uint8_t en_part;               /* the part it plays */
extern volatile uint8_t en_sect_next;          /* the section after this one (a jump's, or the plan's) */
extern volatile uint16_t en_songs;             /* songs begun since power-on */
extern volatile uint8_t en_step;               /* the sixteenth playing, 0..15 */
extern volatile uint8_t en_root_now;           /* the root sounding (pitch class); P_ROOT may be on its way */
extern volatile uint8_t en_chord_root;         /* the progression's root now (pitch class) */
extern volatile uint8_t en_active;             /* bit l: layer l plays in this section */
extern volatile uint8_t en_muted;              /* bit l: muted by the player */
extern volatile uint8_t en_pending;            /* what waits for the bar: EN_PEND_* */
enum { EN_PEND_NEXT = 1, EN_PEND_BREAK = 2, EN_PEND_FILL = 4, EN_PEND_MUTATE = 8, EN_PEND_ROOT = 16 };
extern volatile uint16_t en_grid[EN_NROW];     /* the bar's hits, bit s = step s */
extern volatile uint8_t en_grid_vel[EN_NROW][EN_NSTEP];   /* and how hard (0..127) */
extern volatile float en_level[EN_NLAYER];     /* each layer's loudness now, 0..1 (meters) */
extern volatile float en_glow[EN_NROW];        /* 0..1: a row just played */
extern volatile uint32_t en_bars;              /* sixteenths played since start (for tests) */

/* the pure parts, for the tests and the screen */
uint32_t en_euclid(int k, int n, int rot);     /* Bjorklund's pattern, as a bitmask (bit 0 = step 0) */
int en_scale(uint32_t pool, int *steps);       /* the pool's pitch classes (relative), ascending; count */
int en_degree(uint32_t pool, int deg);         /* scale degree deg (any integer) in semitones from the root */
int en_sect_bars(int sect, int arrange);       /* a section's length at an Arrange setting (0: loops) */

/* the pieces: recipes for every parameter (Tune and MIDI out included, but they are the player's) */
#define EN_NPIECES 8
const char *en_piece_name(int i);
void en_piece(int i, int16_t *par);
