/* SPDX-License-Identifier: GPL-3.0-only */
/* The instrument (ind.h). One compilation unit, -O2. */
#include "ind.h"
#include "fastmath.h"

#ifdef EN_HOST
#define EN_POOL
#else
#define EN_POOL __attribute__((section(".pool")))
#endif
#define BARRIER() __asm__ volatile("" ::: "memory")

const char *const EN_NOTE_NAME[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
const char *const EN_LAYER_NAME[EN_NLAYER] = {"Drums", "Bass", "Guitar", "Synth", "Metal", "Noise"};
const char *const EN_MODE_NAME[EN_NMODES] = {"Phrygian", "Aeolian", "Locrian", "Phryg dom", "Harm min", "Dorian",
                                             "Blues", "Octatonic"};
const char *const EN_SECT_NAME[EN_NSECT] = {"Intro", "Verse", "Build", "Chorus", "Break", "Drop", "Outro"};
#define PC(x) (1u << (x))
const uint16_t EN_MODE_POOL[EN_NMODES] = {
    PC(0) | PC(1) | PC(3) | PC(5) | PC(7) | PC(8) | PC(10),            /* Phrygian: the b2 */
    PC(0) | PC(2) | PC(3) | PC(5) | PC(7) | PC(8) | PC(10),            /* Aeolian */
    PC(0) | PC(1) | PC(3) | PC(5) | PC(6) | PC(8) | PC(10),            /* Locrian: the b2 and the b5 */
    PC(0) | PC(1) | PC(4) | PC(5) | PC(7) | PC(8) | PC(10),            /* Phrygian dominant */
    PC(0) | PC(2) | PC(3) | PC(5) | PC(7) | PC(8) | PC(11),            /* harmonic minor */
    PC(0) | PC(2) | PC(3) | PC(5) | PC(7) | PC(9) | PC(10),            /* Dorian */
    PC(0) | PC(3) | PC(5) | PC(6) | PC(7) | PC(10),                    /* blues: minor pentatonic and the b5 */
    PC(0) | PC(1) | PC(3) | PC(4) | PC(6) | PC(7) | PC(9) | PC(10)};   /* octatonic, half-whole */
#undef PC

static const en_param_t PARAMS[P_NPARAMS] = {
    {"Tempo", 60, 180, 122},  {"Arrange", 0, EN_NARRANGE - 1, A_NORMAL}, {"Evolve", 0, 100, 40}, {"Intensity", 0, 100, 60},
    {"Drums", 0, 100, 80},    {"Groove", 0, EN_NGROOVE - 1, G_EBM},     {"Crush", 0, 100, 15},  {"Kick", 0, 100, 45},
    {"Bass", 0, 100, 75},     {"Cutoff", 0, 100, 45},   {"Squelch", 0, 100, 55}, {"Bass drive", 0, 100, 30},
    {"Guitar", 0, 100, 60},   {"Gain", 0, 100, 70},     {"Chug", 0, 100, 70},    {"Riff", 0, 100, 55},
    {"Synth", 0, 100, 50},    {"Tone", 0, 100, 40},     {"Decay", 0, 100, 30},   {"Busy", 0, 100, 40},
    {"Metal", 0, 100, 45},    {"Hits", 0, 100, 40},     {"Noise", 0, 100, 35},   {"Texture", 0, 100, 30},
    {"Reverb", 0, 100, 35},   {"Gate", 0, 100, 60},     {"Delay", 0, 100, 30},   {"Pump", 0, 100, 40},
    {"Root", 0, 11, 4},       {"Tune", -50, 50, 0},     {"MIDI out", 0, 1, 1},   {"Mode", 0, EN_NMODES - 1, MODE_PHRYGIAN},
};
const en_param_t *en_param_info(int i) { return (i >= 0 && i < P_NPARAMS) ? &PARAMS[i] : &PARAMS[0]; }
const uint8_t EN_LAYER_PARAM[EN_NLAYER] = {P_DRUMS, P_BASS, P_GUITAR, P_SYNTH, P_METAL, P_NOISE};

/* the pieces: eight recipes for the whole machine (the FM-1's PRESETS). Tune and MIDI out are taken from
 * here by neither: they belong to the player, not the piece */
static const struct {
    const char *name;
    int16_t v[P_NPARAMS];
} PIECES[EN_NPIECES] = {
/*                  Tem Ar Evo Int  Dru Gv Cru Kik  Bas Cut Squ BDr  Gtr Gai Chg Rif  Syn Ton Dec Bsy  Met Hit Noi Tex  Rev Gat Dly Pmp  Rt Tu MI Md */
    {"Foundry",    {122, 2, 40, 60,  80, 0, 15, 45,  75, 45, 55, 30,  60, 70, 70, 55,  50, 40, 30, 40,  45, 40, 35, 30,  35, 60, 30, 40,  4, 0, 1, 0}},
    {"Rivethead",  {160, 2, 45, 85,  85, 4, 10, 60,  70, 40, 35, 50,  85, 90, 85, 75,  30, 70, 20, 30,  40, 50, 25, 50,  25, 70, 15, 20,  2, 0, 1, 3}},
    {"Neon Wound", {112, 2, 35, 55,  75, 2, 40, 30,  70, 55, 65, 20,  30, 55, 50, 40,  70, 60, 40, 60,  35, 30, 30, 40,  40, 40, 40, 30,  9, 0, 1, 1}},
    {"Dead Air",   { 96, 1, 55, 40,  70, 3, 70, 50,  60, 30, 40, 40,  40, 75, 40, 35,  45, 85, 70, 25,  55, 35, 75, 70,  60, 20, 45, 15,  5, 0, 1, 2}},
    {"Sledge",     { 84, 1, 30, 70,  85, 5, 30, 80,  75, 35, 30, 60,  80, 85, 60, 45,  25, 50, 50, 25,  65, 50, 40, 40,  45, 75, 20, 25,  7, 0, 1, 0}},
    {"Cold Wire",  {126, 2, 40, 55,  75, 0, 20, 35,  85, 50, 70, 25,   0, 50, 50, 50,  60, 30, 25, 55,  40, 50, 30, 20,  35, 50, 45, 50,  0, 0, 1, 1}},
    {"Scrapyard",  {108, 2, 60, 60,  70, 1, 50, 50,  55, 35, 45, 45,  45, 70, 80, 40,  35, 90, 30, 30,  85, 80, 50, 60,  50, 40, 40, 30, 10, 0, 1, 7}},
    {"Klaxon",     {140, 3, 70, 90,  85, 1, 25, 55,  75, 50, 60, 45,  75, 80, 70, 70,  60, 65, 25, 70,  50, 65, 40, 50,  35, 60, 30, 40,  6, 0, 1, 4}},
};

const char *en_piece_name(int i) { return PIECES[(unsigned)i < EN_NPIECES ? i : 0].name; }
void en_piece(int i, int16_t *p)
{
    int k;
    i = (unsigned)i < EN_NPIECES ? i : 0;
    for (k = 0; k < P_NPARAMS; k++)
        p[k] = PIECES[i].v[k];
}

static char *itoa_s(int v, char *b)
{
    char t[8];
    int n = 0, neg = v < 0;
    if (neg)
        v = -v;
    do {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v && n < 6);
    if (neg)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
    return b;
}

void en_param_text(int i, int v, char *b)
{
    static const char *const ONOFF[2] = {"Off", "On"};
    static const char *const ARR[EN_NARRANGE] = {"Loop", "Long", "Normal", "Short"};
    static const char *const GRV[EN_NGROOVE] = {"EBM", "Rock", "Break", "Half", "Thrash", "Stomp"};
    const char *s = 0;
    if (i == P_ARRANGE)
        s = ARR[(unsigned)v < EN_NARRANGE ? v : 0];
    else if (i == P_GROOVE)
        s = GRV[(unsigned)v < EN_NGROOVE ? v : 0];
    else if (i == P_MIDI)
        s = ONOFF[v ? 1 : 0];
    else if (i == P_MODE)
        s = EN_MODE_NAME[(unsigned)v < EN_NMODES ? v : 0];
    else if (i == P_ROOT)
        s = EN_NOTE_NAME[(unsigned)v < 12u ? v : 0];
    if (s) {
        int k = 0;
        while (s[k] && k < 9) {
            b[k] = s[k];
            k++;
        }
        b[k] = 0;
        return;
    }
    if (i == P_TUNE && v > 0)
        *b++ = '+';
    itoa_s(v, b);
}

/* ------------------------------------------------------- the pure parts --- */
uint32_t en_euclid(int k, int n, int rot)
{
    /* Bjorklund's algorithm (as Toussaint's "The Euclidean Algorithm Generates Traditional Musical
     * Rhythms"): k groups "x" and n - k groups ".", the remainders folded onto the heads until at most
     * one remains. Groups are bit strings, LSB first: (bits, length). */
    uint32_t gb[32], m = 0;
    uint8_t gl[32];
    int ng, heads, tails, i, pos = 0;
    if (n <= 0 || n > 32 || k <= 0)
        return 0;
    if (k > n)
        k = n;
    for (i = 0; i < n; i++) {
        gb[i] = i < k ? 1u : 0u;
        gl[i] = 1;
    }
    ng = n;
    heads = k;
    tails = n - k;
    while (tails > 1 && heads > 0) {
        int pairs = heads < tails ? heads : tails;
        for (i = 0; i < pairs; i++) {           /* a tail onto each head */
            gb[i] |= gb[ng - pairs + i] << gl[i];
            gl[i] = (uint8_t)(gl[i] + gl[ng - pairs + i]);
        }
        if (heads > tails) {                    /* the unpaired heads become the new tails */
            tails = heads - pairs;
            heads = pairs;
        } else {
            tails -= pairs;
            heads = pairs;
        }
        ng = heads + tails;
    }
    for (i = 0; i < ng; i++) {
        uint32_t j;
        for (j = 0; j < gl[i]; j++)
            if (gb[i] >> j & 1u)
                m |= 1u << (((pos + (int)j + rot) % n + n) % n);
        pos += gl[i];
    }
    return m;
}

int en_scale(uint32_t pool, int *steps)
{
    int i, m = 0;
    pool |= 1u;
    for (i = 0; i < 12; i++)
        if (pool >> i & 1u)
            steps[m++] = i;
    return m;
}

int en_degree(uint32_t pool, int deg)
{
    int st[12], m = en_scale(pool, st), o = deg >= 0 ? deg / m : -((-deg + m - 1) / m);
    return st[deg - o * m] + 12 * o;
}

/* a section's bars at Normal; Long doubles them, Short halves them. Loop holds the verse, the chorus,
 * the breakdown and the drop until a button moves on: the intro, the build and the outro still lead on */
static const uint8_t SECT_BARS[EN_NSECT] = {8, 16, 4, 16, 8, 16, 8};
int en_sect_bars(int s, int a)
{
    int b = SECT_BARS[(unsigned)s < EN_NSECT ? s : 0];
    if (a == A_LOOP)
        return (s == S_INTRO || s == S_BUILD || s == S_OUTRO) ? b : 0;
    return a == A_LONG ? 2 * b : a == A_SHORT ? (b / 2 > 2 ? b / 2 : 2) : b;
}

/* ------------------------------------------------------------ commands --- */
enum { C_SET, C_PLAY, C_NEXT, C_BREAK, C_FILL, C_MUTATE, C_MUTE, C_SEED, C_PANIC, C_CLOCK, C_LOADED, C_REBUILD,
       C_TURNED, C_AUDITION };
#define QN 64u
static uint32_t q[QN];
static volatile uint32_t q_w, q_r;
static void post(uint32_t c, int a, int b)
{
    uint32_t w = q_w;
    if (w - q_r >= QN)
        return;                                   /* full: dropped (never, at the UI's rate) */
    q[w % QN] = c << 24 | ((uint32_t)a & 0xFFu) << 16 | ((uint32_t)b & 0xFFFFu);
    BARRIER();
    q_w = w + 1;
}
void en_set(int p, int v) { post(C_SET, p, v); }
void en_play(int on) { post(C_PLAY, on, 0); }
void en_next(void) { post(C_NEXT, 0, 0); }
void en_break(void) { post(C_BREAK, 0, 0); }
void en_fill(void) { post(C_FILL, 0, 0); }
void en_mutate(void) { post(C_MUTATE, 0, 0); }
void en_mute(int l, int on) { post(C_MUTE, l, on); }
void en_seed(uint32_t s) { post(C_SEED, 0, (int)(s & 0xFFFFu)); }
void en_panic(void) { post(C_PANIC, 0, 0); }
void en_clock(int msg) { post(C_CLOCK, msg, 0); }
void en_regrow(void)
{
    en_song.locked = 0;                          /* the render writes no more of it, and the UI no less */
    post(C_LOADED, 0, 0);
}
void en_lock(int on)
{
    en_song.locked = (uint8_t)(on != 0);
    BARRIER();                                   /* before the caller's next write to en_song */
}
void en_song_loaded(void) { post(C_LOADED, 0, 0); }
void en_rebuild(void) { post(C_REBUILD, 0, 0); }
void en_turned(int p) { post(C_TURNED, p, 0); }
void en_audition(int l, int what, int f) { post(C_AUDITION, l, (what & 0xFF) | (f & 0xFF) << 8); }

/* -------------------------------------------------------------- state --- */
volatile uint8_t en_playing, en_ext, en_sect, en_sect_bar, en_sect_len;
volatile uint8_t en_plan[EN_PLAN_MAX], en_plan_len[EN_PLAN_MAX], en_plan_n, en_plan_pos, en_jumped, en_sect_next;
volatile uint8_t en_part;
volatile uint16_t en_songs;
volatile uint8_t en_step, en_root_now, en_chord_root, en_active, en_muted, en_pending;
volatile uint16_t en_grid[EN_NROW];
volatile uint8_t en_grid_vel[EN_NROW][EN_NSTEP];
volatile float en_level[EN_NLAYER], en_glow[EN_NROW];
volatile uint8_t en_hits[EN_NROW], en_tom;
uint32_t (*en_time)(void);
volatile uint32_t en_prof[PR_N];
static uint32_t prof_t;
static inline void prof(int k)                  /* the time since the last mark, to part k */
{
    if (en_time) {
        uint32_t t = en_time();
        en_prof[k] += t - prof_t;
        prof_t = t;
    }
}
volatile uint32_t en_bars;

static int16_t par[P_NPARAMS];
static uint32_t pool = 0x5ABu;                  /* Phrygian until told */
static int root = 4, root_pend = -1;
static float tunefac = 1.0f;
static uint32_t rng = 0x9E3779B9u;

static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}
static float frand(void) { return (float)(rnd() >> 8) * (1.0f / 16777216.0f); }
static int irand(int n) { return n > 0 ? (int)((rnd() >> 8) % (uint32_t)n) : 0; }

static void midi(uint32_t st, uint32_t d1, uint32_t d2)
{
    if (par[P_MIDI])
        en_midi_out(st, d1, d2);
}

static float mtof(float n) { return 440.0f * tunefac * fm_exp2f((n - 69.0f) * (1.0f / 12.0f)); }

/* a sine table: 1024 points a turn, plus one */
#define SN 1024
static float sint[SN + 1];
static inline float sine(float ph)              /* ph in turns, any value >= 0 */
{
    float x = (ph - (float)(int)ph) * (float)SN;
    int i = (int)x;
    float f = x - (float)i;
    return sint[i] + (sint[i + 1] - sint[i]) * f;
}

/* white noise (an LCG: the cheapest there is, and white enough for drums and static) */
static uint32_t nz = 22222u;
static inline float white(void)
{
    nz = nz * 1664525u + 1013904223u;
    return (float)(int32_t)nz * (1.0f / 2147483648.0f);
}

/* a soft clipper: tanh's Pade approximant, exact at the clip (|x| = 3); one divide */
static inline float sat(float x)
{
    float x2;
    if (x > 3.0f)
        return 1.0f;
    if (x < -3.0f)
        return -1.0f;
    x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

static inline float blep_saw(float t, float dt)
{
    float y = 2.0f * t - 1.0f;
    if (t < dt) {
        float x = t / dt;
        y -= x + x - x * x - 1.0f;
    } else if (t > 1.0f - dt) {
        float x = (t - 1.0f) / dt;
        y -= x * x + x + x + 1.0f;
    }
    return y;
}
static inline float blep_sq(float t, float dt)  /* a square: two saws half a turn apart */
{
    float t2 = t + 0.5f;
    if (t2 >= 1.0f)
        t2 -= 1.0f;
    return blep_saw(t, dt) - blep_saw(t2, dt);
}

/* TPT state-variable filter (Zavalishin): coefficients a1, a2, a3, k (k = 2 none .. 0 self-oscillating) */
static void svf_coefs(float *c, float f, float k)
{
    float g = fm_tanf(FM_PI * fm_clampf(f, 30.0f, 18000.0f) * (1.0f / EN_SR));
    k = fm_maxf(k, 0.08f);                      /* below 0 it would grow without end */
    c[0] = 1.0f / (1.0f + g * (g + k));
    c[1] = g * c[0];
    c[2] = g * c[1];
    c[3] = k;
}
/* one sample: the low-pass out, the band-pass in *bp */
static inline float svf(float *s, const float *c, float x, float *bp)
{
    float v3 = x - s[1], v1 = c[0] * s[0] + c[1] * v3, v2 = s[1] + c[1] * s[0] + c[2] * v3;
    s[0] = 2.0f * v1 - s[0];
    s[1] = 2.0f * v2 - s[1];
    *bp = v1;
    return v2;
}
static float onepole(float f) { return 1.0f - fm_expf(-FM_TWO_PI * f * (1.0f / EN_SR)); }
static float t60(float secs) { return fm_expf(-6.9f / (secs * EN_SR)); }   /* a decay per sample: -60 dB in secs */

/* ------------------------------------------------- slow random motion --- */
#define NMOD 4                                  /* the drone's filter, the static's band, the drone's beat, the metal */
static struct {
    float v, tgt, rate;
    uint32_t left;                              /* samples until a new target */
} mod[NMOD];

static void mods_step(uint32_t n)
{
    int i;
    for (i = 0; i < NMOD; i++) {
        if (mod[i].left <= n) {
            float secs = 3.0f + 12.0f * frand();
            mod[i].tgt = frand() * 2.0f - 1.0f;
            mod[i].left = (uint32_t)(secs * EN_SR);
            mod[i].rate = (float)n / (secs * EN_SR * 0.5f);
        } else {
            mod[i].left -= n;
        }
        mod[i].v += (mod[i].tgt - mod[i].v) * fm_minf(mod[i].rate, 1.0f);
    }
}

/* ------------------------------------------------------------- levels --- */
static float vol[EN_NLAYER];                    /* each layer's level, squared */
static float mg[EN_NLAYER] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};   /* the mutes, faded: 1 heard */
static uint8_t act = 0x3Fu, muted;              /* the section's layers, the player's mutes */
static float evolve, intensity;
static float step_len, pulse_left;              /* a sixteenth, in samples; to the next MIDI clock pulse */
static float sweep;                             /* the filters' sweep, octaves (the intro opens, the build rises) */
static float drum_vol;
static int on(int l) { return (act >> l & 1u) && !(muted >> l & 1u) && (l == LY_DRUMS ? drum_vol : vol[l]) > 0.0f; }

/* ================================================================= voices === */
/* ---------------------------------------------------------------- drums --- */
/* the kick and the toms: a sine falling to its pitch, a click; the kick through its own drive */
typedef struct {
    float ph, inc, inc_end, pk, amp, adec, click;
    uint8_t on;
} thump_t;
static thump_t kick, tom;
static float kick_g = 1.0f, kick_inv = 1.0f, kick_t60 = 0.35f;

typedef struct {
    float ph1, ph2, inc1, inc2, end1, tamp, tdec, namp, ndec, nlp, nlp2;
    int32_t clap_t;                             /* samples into a clap's three bursts; < 0 none */
    uint8_t on, clap;
} snare_t;
static snare_t snare;
#define CLAP_GAP 480                            /* 11 ms between a clap's bursts */

/* the hats (AMB-1's, after the 808): four square waves at inharmonic ratios and noise, high-passed */
typedef struct {
    float x1, x2, amp, adec, att, hf, mph[4];
    uint8_t on;
} hat_t;
static hat_t hat_c, hat_o;                      /* closed, open (or crash); the closed chokes the open */
static const float HAT_INC[4] = {304.4f / EN_SR, 369.6f / EN_SR, 522.7f / EN_SR, 800.0f / EN_SR};

/* the crusher (the drum bus): sample and hold, then fewer bits */
static int crush_hold = 1, crush_n;
static float crush_q = 32768.0f, crush_iq = 1.0f / 32768.0f, crush_y, crush_amt;

/* the gate on the plate: a snare opens it and it shuts after GATE_HOLD; a bar without one leaves it open */
#define GATE_HOLD (0.14f * EN_SR)
static float gate_amt, gate_env = 1.0f, gate_tgt = 1.0f;
static int32_t gate_hold, since_snare = 1 << 30;

/* Pump: the kick ducks the rest */
static float pump_amt, pump_env, pump_tgt, pump_rel = 0.9995f;

static void kick_fire(float vel, int boom, int tight)
{
    kick.on = 1;
    kick.ph = 0.0f;
    kick.inc_end = (boom ? 38.0f : 50.0f) * (1.0f / EN_SR);
    kick.inc = kick.inc_end * 5.0f;
    kick.pk = t60(boom ? 0.12f : 0.035f);
    kick.amp = vel;
    kick.adec = t60(boom ? 1.6f : tight ? 0.11f : kick_t60);
    kick.click = vel * 0.9f;
    pump_tgt = 1.0f;
    en_glow[EN_ROW_KICK] = 1.0f;
    en_hits[EN_ROW_KICK]++;
    midi(0x99, 36, (uint32_t)(vel * 127.0f));
    midi(0x89, 36, 0);
}

static void tom_fire(float vel, int k)          /* k: 0 high, 1 mid, 2 low */
{
    static const float F[3] = {190.0f, 140.0f, 100.0f};
    static const uint8_t GM[3] = {48, 45, 43};
    tom.on = 1;
    tom.ph = 0.0f;
    tom.inc_end = F[k] * (1.0f / EN_SR);
    tom.inc = tom.inc_end * 1.8f;
    tom.pk = t60(0.08f);
    tom.amp = vel;
    tom.adec = t60(0.55f);
    tom.click = vel * 0.25f;
    en_glow[EN_ROW_SNARE] = 1.0f;
    en_hits[EN_ROW_SNARE]++;
    en_tom = 1;
    midi(0x99, GM[k], (uint32_t)(vel * 127.0f));
    midi(0x89, GM[k], 0);
}

static void snare_fire(float vel)
{
    snare.on = 1;
    snare.clap = par[P_GROOVE] == G_EBM || par[P_GROOVE] == G_STOMP;
    snare.inc1 = 240.0f * (1.0f / EN_SR);
    snare.end1 = 185.0f * (1.0f / EN_SR);
    snare.inc2 = 330.0f * (1.0f / EN_SR);
    snare.tamp = vel * (snare.clap ? 0.35f : 0.8f);
    snare.tdec = t60(0.12f);
    snare.namp = vel;
    snare.ndec = t60(snare.clap ? 0.3f : 0.22f);
    snare.clap_t = snare.clap ? 0 : -1;
    gate_tgt = 1.0f;
    gate_hold = (int32_t)GATE_HOLD;
    since_snare = 0;
    en_glow[EN_ROW_SNARE] = 1.0f;
    en_hits[EN_ROW_SNARE]++;
    en_tom = 0;
    midi(0x99, snare.clap ? 39 : 38, (uint32_t)(vel * 127.0f));
    midi(0x89, snare.clap ? 39 : 38, 0);
}

static void hat_fire(int kind, float vel)       /* 0 closed, 1 open, 2 crash */
{
    static const uint8_t GM[3] = {42, 46, 49};
    hat_t *v = kind ? &hat_o : &hat_c;
    v->on = 1;
    v->amp = vel;
    v->att = 0.0f;
    v->hf = kind == 2 ? 0.62f : 0.92f;
    v->adec = t60(kind == 2 ? 1.8f : kind ? 0.35f : 0.05f);
    if (!kind && hat_o.on && hat_o.hf > 0.8f)
        hat_o.adec = t60(0.02f);                /* the closed chokes the open (not the crash) */
    en_glow[EN_ROW_HAT] = 1.0f;
    en_hits[EN_ROW_HAT]++;
    midi(0x99, GM[kind], (uint32_t)(vel * 110.0f));
    midi(0x89, GM[kind], 0);
}

static void render_thump(thump_t *v, uint32_t n, float *out, float g, float drv, float inv)
{
    float ph = v->ph, inc = v->inc, amp = v->amp, ck = v->click;
    const float ie = v->inc_end, pk = v->pk, ad = v->adec;
    uint32_t j;
    for (j = 0; j < n; j++) {
        float x = sine(ph) * amp;
        if (ck > 1e-4f) {
            x += ck * white();
            ck *= 0.975f;
        }
        ph += inc;
        if (ph >= 1.0f)
            ph -= 1.0f;
        inc = ie + (inc - ie) * pk;
        amp *= ad;
        out[j] += sat(x * drv) * inv * g;
    }
    v->ph = ph;
    v->inc = inc;
    v->amp = amp;
    v->click = ck;
    v->on = amp > 0.0003f;
}

static void render_hat(hat_t *v, uint32_t n, float *out, float g)
{
    uint32_t j;
    int k;
    float x1 = v->x1, x2 = v->x2, amp = v->amp, att = v->att, hf = v->hf;
    for (j = 0; j < n; j++) {
        float in = 0.0f, hp;
        for (k = 0; k < 4; k++) {
            v->mph[k] += HAT_INC[k];
            if (v->mph[k] >= 1.0f)
                v->mph[k] -= 1.0f;
            in += v->mph[k] < 0.5f ? 0.25f : -0.25f;
        }
        in = 0.5f * in + 0.6f * white();
        hp = in - x2 - 0.8f * x1;
        x1 += hf * hp;
        x2 += hf * x1;
        out[j] += hp * amp * att * g;
        amp *= v->adec;
        att += (1.0f - att) * 0.3f;
    }
    v->x1 = fm_flush(x1);
    v->x2 = fm_flush(x2);
    v->amp = amp;
    v->att = att;
    v->on = amp > 0.0005f;
}

#define BLK 256
static float mixl[BLK], mixr[BLK], drum[BLK], rsend[BLK], dsend[BLK];

static void render_drums(uint32_t n)
{
    uint32_t j;
    const float g = drum_vol * mg[LY_DRUMS];
    float lvl = 0.0f;
    if (kick.on) {
        lvl = kick.amp;
        render_thump(&kick, n, drum, g * 0.62f, kick_g, kick_inv);
    }
    if (tom.on) {
        float t0 = tom.amp;
        render_thump(&tom, n, drum, g * 0.7f, 1.0f, 1.0f);
        for (j = 0; j < n; j++)                  /* the toms ring in the room a little */
            rsend[j] += drum[j] * 0.0f;
        lvl = fm_maxf(lvl, t0 * 0.7f);
    }
    if (snare.on) {
        snare_t *v = &snare;
        float ph1 = v->ph1, ph2 = v->ph2, inc1 = v->inc1, tamp = v->tamp, namp = v->namp, nlp = v->nlp, nlp2 = v->nlp2;
        const float e1 = v->end1, rs = g * 0.55f;
        int32_t ct = v->clap_t;
        lvl = fm_maxf(lvl, namp);
        for (j = 0; j < n; j++) {
            float t = sine(ph1) * 0.7f + sine(ph2) * 0.35f, nzs = white(), e = 1.0f, x;
            ph1 += inc1;
            if (ph1 >= 1.0f)
                ph1 -= 1.0f;
            ph2 += v->inc2;
            if (ph2 >= 1.0f)
                ph2 -= 1.0f;
            inc1 = e1 + (inc1 - e1) * 0.998f;
            nlp += 0.11f * (nzs - nlp);          /* the noise high-passed at ~800 Hz, and a top cut */
            nlp2 += 0.75f * ((nzs - nlp) - nlp2);
            if (ct >= 0) {                        /* a clap: three bursts, then the tail */
                if (ct < 3 * CLAP_GAP)
                    e = 1.0f - 0.8f * (float)(ct % CLAP_GAP) * (1.0f / CLAP_GAP);
                else
                    ct = -2;
                if (ct >= 0)
                    ct++;
            }
            x = t * tamp + nlp2 * namp * e;
            tamp *= v->tdec;
            if (ct < 0)
                namp *= v->ndec;
            drum[j] += x * g * 1.5f;
            rsend[j] += x * rs;
        }
        v->ph1 = ph1;
        v->ph2 = ph2;
        v->inc1 = inc1;
        v->tamp = tamp;
        v->namp = namp;
        v->nlp = fm_flush(nlp);
        v->nlp2 = fm_flush(nlp2);
        v->clap_t = ct;
        v->on = namp > 0.0004f || tamp > 0.0004f;
    }
    if (hat_c.on)
        render_hat(&hat_c, n, drum, g * 0.36f);
    if (hat_o.on) {
        render_hat(&hat_o, n, drum, g * (hat_o.hf < 0.8f ? 0.42f : 0.28f));
        lvl = fm_maxf(lvl, hat_o.amp * 0.5f);
    }
    if (crush_amt > 0.0f) {                     /* the EMAX / SP-12 crunch */
        int h = crush_n;
        float y = crush_y;
        for (j = 0; j < n; j++) {
            if (++h >= crush_hold) {
                h = 0;
                y = fm_floorf(drum[j] * crush_q + 0.5f) * crush_iq;
            }
            drum[j] = y;
        }
        crush_n = h;
        crush_y = y;
    }
    en_level[LY_DRUMS] = fm_maxf(lvl * (float)par[P_DRUMS] * 0.01f, en_level[LY_DRUMS] * 0.9f);
}

/* ----------------------------------------------------------------- bass --- */
/* a Pro-1 / Minimoog-ish mono synth: a saw and a square a few cents apart, a sine an octave down, a 12 dB TPT low-pass and a
 * one-pole after it (18 dB), an envelope on the cutoff (Squelch: its depth and the resonance), then the
 * drive. Accents open it further; a slide glides into the next note without a new envelope. */
static struct {
    float ph, ph2, ph3, note, target, amp, fenv, acc, s[2], lp, c[4], g1, glide;
    int32_t gate;                               /* samples the note holds */
    uint8_t on, midi_note;
} bass;
static float bass_cut = 400.0f, bass_env = 3.0f, bass_k = 1.0f, bass_fdec = 0.99f, bass_drv = 1.0f, bass_dinv = 1.0f;

static void bass_play(int n, uint8_t fl, int32_t len)
{
    if ((fl & 2u) && bass.gate > 0) {           /* slide: on to the note, the envelope carries on */
        bass.target = (float)n;
        bass.glide = 0.08f;
    } else {
        bass.note = bass.target = (float)n;
        bass.glide = 1.0f;
        bass.fenv = 1.0f;
        bass.acc = (fl & 1u) ? 1.0f : 0.0f;
    }
    bass.gate = len;
    bass.on = 1;
    if (bass.midi_note && bass.midi_note != n)
        midi(0x81, bass.midi_note, 0);
    if (bass.midi_note != n || !(fl & 2u))
        midi(0x91, (uint32_t)n, (fl & 1u) ? 110u : 85u);
    bass.midi_note = (uint8_t)n;
    en_glow[EN_ROW_BASS] = 1.0f;
    en_hits[EN_ROW_BASS]++;
}

static void render_bass(uint32_t n)
{
    uint32_t j;
    const float g = vol[LY_BASS] * mg[LY_BASS] * 0.36f;
    float ph = bass.ph, ph2 = bass.ph2, ph3 = bass.ph3, amp = bass.amp, lp = bass.lp, inc = 0.0f, tgt = 0.0f, ar = 0.0f;
    if (!bass.on && amp < 1e-5f) {
        bass.amp = 0.0f;
        en_level[LY_BASS] = 0.0f;
        return;
    }
    for (j = 0; j < n; j++) {
        float x, bp, y;
        if (!(j & 15u)) {                        /* every 16 samples: pitch, envelopes, the filter */
            float fc;
            bass.note += (bass.target - bass.note) * bass.glide;
            inc = mtof(bass.note) * (1.0f / EN_SR);
            fc = bass_cut * fm_exp2f(bass_env * bass.fenv + 1.5f * bass.acc * bass.fenv + sweep);
            svf_coefs(bass.c, fc, bass_k - 0.3f * bass.acc);
            bass.g1 = bass.c[1] / (bass.c[0] + 1e-9f);   /* g: the one-pole's TPT gain is g / (1 + g) */
            bass.g1 = bass.g1 / (1.0f + bass.g1);
            bass.fenv *= bass_fdec;
            if (bass.gate > 0)
                bass.gate -= 16;
            tgt = bass.gate > 0 ? 1.0f : 0.0f;
            ar = bass.gate > 0 ? 0.06f : 0.004f;
        }
        x = blep_saw(ph, inc) + 0.55f * blep_sq(ph2, inc * 1.003f);
        ph += inc;
        if (ph >= 1.0f)
            ph -= 1.0f;
        ph2 += inc * 1.003f;
        if (ph2 >= 1.0f)
            ph2 -= 1.0f;
        ph3 += inc * 0.5f;
        if (ph3 >= 1.0f)
            ph3 -= 1.0f;
        y = svf(bass.s, bass.c, x, &bp);
        {
            float v = (y - lp) * bass.g1;       /* the one-pole after it (TPT) */
            y = v + lp;
            lp = y + v;
        }
        y = sat(y * bass_drv) * bass_dinv + 0.3f * sine(ph3);
        amp += (tgt - amp) * ar;
        y *= amp * g;
        mixl[j] += y;
        mixr[j] += y;
        rsend[j] += y * 0.04f;
    }
    bass.ph = ph;
    bass.ph2 = ph2;
    bass.ph3 = ph3;
    bass.amp = amp;
    bass.lp = fm_flush(lp);
    bass.s[0] = fm_flush(bass.s[0]);
    bass.s[1] = fm_flush(bass.s[1]);
    bass.on = bass.gate > 0 || amp > 1e-4f;
    en_level[LY_BASS] = amp * (float)par[P_BASS] * 0.01f;
}

/* --------------------------------------------------------------- guitar --- */
/* two guitars, left and right, as a double-tracked rhythm part: each a power chord (the root, the fifth
 * and the octave, three band-limited saws, a few cents apart from the other side) and a pick's click,
 * through a low-pass where the palm mutes (Chug), a high-pass that tightens it (the amp's boost), a
 * high-gain clipper (Gain; run at twice the rate, its input interpolated, against aliasing) and a
 * cabinet (a resonant low-pass at ~4.3 kHz and a pole above it). The right one plays a few milliseconds
 * late: its new chord waits, and the old one rings until it comes (no gap). */
typedef struct {
    float inc[3], env, edec, pre_c, pick;
} gnote_t;
typedef struct {
    float ph[3], pre, hp, cs[2], lp2, dc, xp;
    gnote_t now, next;                          /* the chord sounding; the one waiting */
    int32_t wait;                               /* samples until next takes over (0: none waits) */
    uint8_t on, midi_note;
} gtr_t;
static gtr_t gtr[2];
static float gtr_cab[4], gtr_gain = 20.0f, gtr_chug = 0.7f;
static const float GTR_DET[2] = {0.9983f, 1.0023f};   /* -3 and +4 cents */

static void gtr_take(gtr_t *v)                  /* the waiting chord sounds */
{
    int k;
    if (!v->on)
        for (k = 0; k < 3; k++)
            v->ph[k] = frand();
    v->now = v->next;
    v->wait = 0;
    v->on = 1;
}

static void gtr_play(int n, uint8_t fl, float vel)
{
    int i;
    const int mute = !(fl & 8u);                 /* F_OPEN: let it ring */
    if (gtr[0].midi_note)
        midi(0x82, gtr[0].midi_note, 0);
    for (i = 0; i < 2; i++) {
        gtr_t *v = &gtr[i];
        gnote_t *x = &v->next;
        float f = mtof((float)n) * GTR_DET[i] * (1.0f / EN_SR);
        if (v->wait > 0)
            gtr_take(v);                         /* one still waiting: it goes now */
        x->inc[0] = f;
        x->inc[1] = f * 1.4983f;                 /* the fifth, a hair flat of pure: the 12-tone one */
        x->inc[2] = f * 2.0f;
        x->env = vel;
        x->edec = mute ? t60(0.08f + 0.3f * (1.0f - gtr_chug)) : t60(3.0f);
        x->pre_c = mute ? onepole(450.0f + 1700.0f * (1.0f - gtr_chug)) : onepole(3400.0f);
        x->pick = vel * (mute ? 0.5f : 0.8f);
        v->wait = i ? 150 + irand(400) : 1 + irand(120);   /* the right hand's a little behind */
        v->midi_note = (uint8_t)n;
    }
    midi(0x92, (uint32_t)n, (uint32_t)(vel * 120.0f));
    en_glow[EN_ROW_GUITAR] = 1.0f;
    en_hits[EN_ROW_GUITAR]++;
}

/* samples j0..j1 of one guitar */
static void gtr_run(gtr_t *v, uint32_t j0, uint32_t j1, float *out, float *other, float g)
{
    float p0 = v->ph[0], p1 = v->ph[1], p2 = v->ph[2], env = v->now.env, pick = v->now.pick, pre = v->pre,
          hp = v->hp, dc = v->dc, lp2 = v->lp2, xp = v->xp;
    const float i0 = v->now.inc[0], i1 = v->now.inc[1], i2 = v->now.inc[2], ed = v->now.edec, pc = v->now.pre_c,
                gain = gtr_gain;
    uint32_t j;
    for (j = j0; j < j1; j++) {
        float s = blep_saw(p0, i0) + blep_saw(p1, i1) + 0.5f * blep_saw(p2, i2), bp, y, xin;
        if (pick > 1e-3f) {
            s += pick * white() * 3.0f;
            pick *= 0.96f;
        }
        p0 += i0;
        if (p0 >= 1.0f)
            p0 -= 1.0f;
        p1 += i1;
        if (p1 >= 1.0f)
            p1 -= 1.0f;
        p2 += i2;
        if (p2 >= 1.0f)
            p2 -= 1.0f;
        pre += pc * (s * env - pre);             /* the palm: a low-pass on the string */
        hp += 0.045f * (pre - hp);               /* ~320 Hz high-pass before the clip: tight, mid-forward */
        xin = (pre - hp) * gain;
        y = 0.5f * (sat(0.5f * (xin + xp)) + sat(xin));   /* the clipper at twice the rate */
        xp = xin;
        y = svf(v->cs, gtr_cab, y, &bp);         /* the cabinet */
        lp2 += 0.55f * (y - lp2);                /* and its roll-off above (~5.5 kHz) */
        dc += 0.003f * (lp2 - dc);               /* ~20 Hz: the offset a clipped lopsided wave leaves */
        y = (lp2 - dc) * fm_minf(env * 5.0f, 1.0f) * g;
        env *= ed;
        out[j] += y;
        other[j] += y * 0.18f;
        rsend[j] += y * 0.12f;
        dsend[j] += y * 0.05f;
    }
    v->ph[0] = p0;
    v->ph[1] = p1;
    v->ph[2] = p2;
    v->now.env = env;
    v->now.pick = pick;
    v->pre = fm_flush(pre);
    v->hp = fm_flush(hp);
    v->dc = fm_flush(dc);
    v->lp2 = fm_flush(lp2);
    v->xp = xp;
    v->cs[0] = fm_flush(v->cs[0]);
    v->cs[1] = fm_flush(v->cs[1]);
}

static void render_guitar(uint32_t n)
{
    int i;
    const float g = vol[LY_GUITAR] * mg[LY_GUITAR] * 0.5f;
    float lvl = 0.0f;
    for (i = 0; i < 2; i++) {
        gtr_t *v = &gtr[i];
        float *out = i ? mixr : mixl, *other = i ? mixl : mixr;
        uint32_t j = 0;
        if (v->wait > 0 && (uint32_t)v->wait <= n) {     /* the old chord up to the new one, then it */
            if (v->on)
                gtr_run(v, 0, (uint32_t)v->wait, out, other, g);
            j = (uint32_t)v->wait;
            gtr_take(v);
        } else if (v->wait > 0) {
            v->wait -= (int32_t)n;
        }
        if (!v->on)
            continue;
        gtr_run(v, j, n, out, other, g);
        if (v->now.env < 0.0006f && !v->wait) {
            v->on = 0;
            if (i == 0 && v->midi_note) {
                midi(0x82, v->midi_note, 0);
                v->midi_note = 0;
            }
        }
        lvl = fm_maxf(lvl, fm_minf(v->now.env * 5.0f, 1.0f));
    }
    en_level[LY_GUITAR] = lvl * (float)par[P_GUITAR] * 0.01f;
}

/* ---------------------------------------------------------------- synth --- */
/* stabs and lines, as the Xpander's and the Prophet VS's on Pretty Hate Machine: each voice a saw and a
 * two-operator FM pair, Tone crossfading from the saw (0) to gritty FM at the carrier's ratio (~50) and
 * a clanging 3.5 (100). The FM index falls through the note (the vector's sweep: a bright attack, then
 * the body), then a low-pass with its own envelope. A stab's three voices sit left, centre and right,
 * a few milliseconds apart (a strum). Four voices: a new note takes a free one or the quietest, whose
 * old note fades out in a millisecond and a half before it comes in (a cut would click). */
#define NSYN 4
typedef struct {
    float cinc, minc, idx, idec, amp, adec, gl, gr;
    uint8_t note;
} snote_t;
typedef struct {
    float cph, mph, att, fenv, fade, fade_k, s[2], c[4];
    snote_t now, next;                          /* the note sounding; the one waiting */
    int32_t wait;                               /* samples until next takes over (0: none waits) */
    uint8_t on;
} syn_t;
static syn_t syn[NSYN];
static float syn_tone, syn_decay;

static void syn_take(syn_t *v)
{
    if (v->on && v->now.note)
        midi(0x80, v->now.note, 0);
    v->now = v->next;
    v->att = 0.0f;
    v->fenv = 1.0f;
    v->fade = 1.0f;
    v->fade_k = 1.0f;
    v->wait = 0;
    v->on = 1;
}

static void syn_voice(int note, float vel, float pan, int32_t wait, int longer)
{
    syn_t *v;
    snote_t *x;
    float f = mtof((float)note) * (1.0f / EN_SR), p, t = syn_tone, lo = 9.0f;
    int i, best = 0;
    for (i = 0; i < NSYN; i++) {                 /* a free voice, or the quietest */
        float a = syn[i].wait > 0 ? 8.0f : syn[i].on ? syn[i].now.amp : -1.0f;
        if (a < lo) {
            lo = a;
            best = i;
        }
    }
    v = &syn[best];
    if (v->wait > 0)
        syn_take(v);                             /* every voice waiting (never, at four): it goes now */
    x = &v->next;
    x->note = (uint8_t)note;
    x->cinc = f;
    x->minc = f * (t > 0.7f ? 3.5f : 1.0f);
    x->idx = (0.15f + 1.1f * t * t) * (0.7f + 0.3f * vel);
    x->idec = t60(0.25f + 0.6f * syn_decay);
    x->amp = vel;
    x->adec = t60((0.1f + 1.4f * syn_decay * syn_decay) * (longer ? 4.0f : 1.0f));
    p = (fm_clampf(pan, -1.0f, 1.0f) + 1.0f) * (FM_PI / 4.0f);
    x->gl = fm_cosf(p);
    x->gr = fm_sinf(p);
    if (v->on && v->now.amp > 0.003f) {          /* sounding: it fades while the new one waits */
        v->wait = wait > 64 ? wait : 64;
        v->fade_k = 0.92f;
    } else if (wait > 0) {
        v->on = 0;
        v->wait = wait;
    } else {
        syn_take(v);
    }
    midi(0x90, (uint32_t)note, (uint32_t)(vel * 110.0f));
}

/* samples j0..j1 of one voice */
static void syn_run(syn_t *v, uint32_t j0, uint32_t j1, float g)
{
    const float t = syn_tone, wf = fm_minf(t * 1.6f, 1.0f), ws = 1.0f - wf, ci = v->now.cinc, mi = v->now.minc,
                id = v->now.idec, ad = v->now.adec, fk = v->fade_k, gl = v->now.gl, gr = v->now.gr;
    float cph = v->cph, mph = v->mph, idx = v->now.idx, amp = v->now.amp, att = v->att, fade = v->fade;
    uint32_t j;
    for (j = j0; j < j1; j++) {
        float x = 0.0f, bp;
        if (!((j - j0) & 31u)) {
            svf_coefs(v->c, 600.0f * fm_exp2f(3.5f * v->fenv + 2.2f * t + 0.6f * sweep), 0.8f);
            v->fenv *= 0.985f;
        }
        if (ws > 0.0f)
            x = ws * 0.8f * blep_saw(cph, ci);
        if (wf > 0.0f)
            x += wf * sine(cph + idx * sine(mph) + 4.0f);
        cph += ci;
        if (cph >= 1.0f)
            cph -= 1.0f;
        mph += mi;
        if (mph >= 1.0f)
            mph -= 1.0f;
        idx *= id;
        x = svf(v->s, v->c, x, &bp) * amp * att * fade * g;
        amp *= ad;
        att += (1.0f - att) * 0.05f;
        fade *= fk;
        mixl[j] += x * gl;
        mixr[j] += x * gr;
        rsend[j] += x * 0.35f;
        dsend[j] += x * 0.55f;
    }
    v->cph = cph;
    v->mph = mph;
    v->now.idx = idx;
    v->now.amp = amp;
    v->att = att;
    v->fade = fade;
    v->s[0] = fm_flush(v->s[0]);
    v->s[1] = fm_flush(v->s[1]);
}

static void render_synth(uint32_t n)
{
    int i;
    const float g = vol[LY_SYNTH] * mg[LY_SYNTH] * 1.2f;
    float lvl = 0.0f;
    for (i = 0; i < NSYN; i++) {
        syn_t *v = &syn[i];
        uint32_t j = 0;
        if (v->wait > 0 && (uint32_t)v->wait <= n) {     /* the old note out, then the new one */
            if (v->on)
                syn_run(v, 0, (uint32_t)v->wait, g);
            j = (uint32_t)v->wait;
            syn_take(v);
        } else if (v->wait > 0) {
            if (v->on)
                syn_run(v, 0, n, g);
            v->wait -= (int32_t)n;
            continue;
        }
        if (!v->on)
            continue;
        syn_run(v, j, n, g);
        if (v->now.amp < 0.002f) {
            v->on = 0;
            midi(0x80, v->now.note, 0);
        }
        lvl = fm_maxf(lvl, v->now.amp);
    }
    en_level[LY_SYNTH] = lvl * (float)par[P_SYNTH] * 0.01f;
}

/* ---------------------------------------------------------------- metal --- */
/* clangs: an inharmonic FM pair (an anvil at 1:1.41, scrap at 1:2.76) with a falling index, and noise
 * through a narrow band-pass (a struck sheet) */
#define NMETAL 3
typedef struct {
    float cph, mph, cinc, minc, idx, idec, amp, adec, namp, ndec, s[2], c[4], gl, gr;
    uint8_t on;
} metal_t;
static metal_t metal[NMETAL];
static int metal_next;
static float metal_f[3] = {620.0f, 910.0f, 1380.0f};

static void metal_hit(int kind, float vel)
{
    metal_t *v = &metal[metal_next];
    float f = metal_f[irand(3)] * (kind ? 1.7f : 1.0f), p = (frand() * 1.2f - 0.6f + 1.0f) * (FM_PI / 4.0f);
    metal_next = (metal_next + 1) % NMETAL;
    v->on = 1;
    v->cph = v->mph = 0.0f;
    v->cinc = f * (1.0f / EN_SR);
    v->minc = v->cinc * (kind ? 2.76f : 1.41f);
    v->idx = kind ? 0.6f : 1.1f;
    v->idec = t60(kind ? 0.05f : 0.12f);
    v->amp = vel * (kind ? 0.5f : 0.8f);
    v->adec = t60(kind ? 0.22f : 0.9f + 0.4f * frand());
    v->namp = vel * (kind ? 0.9f : 0.25f);
    v->ndec = t60(kind ? 0.12f : 0.06f);
    svf_coefs(v->c, kind ? 4200.0f : 2600.0f, 0.15f);
    v->gl = fm_cosf(p);
    v->gr = fm_sinf(p);
    en_glow[EN_ROW_METAL] = 1.0f;
    en_hits[EN_ROW_METAL]++;
    midi(0x99, kind ? 54u : 56u, (uint32_t)(vel * 110.0f));
    midi(0x89, kind ? 54u : 56u, 0);
}

static void render_metal(uint32_t n)
{
    int i;
    const float g = vol[LY_METAL] * mg[LY_METAL] * 0.3f;
    float lvl = 0.0f;
    for (i = 0; i < NMETAL; i++) {
        metal_t *v = &metal[i];
        float cph = v->cph, mph = v->mph, idx = v->idx, amp = v->amp, namp = v->namp;
        uint32_t j;
        if (!v->on)
            continue;
        for (j = 0; j < n; j++) {
            float bp, x = sine(cph + idx * sine(mph) + 4.0f) * amp;
            svf(v->s, v->c, white(), &bp);
            x = (x + bp * namp) * g;
            cph += v->cinc;
            if (cph >= 1.0f)
                cph -= 1.0f;
            mph += v->minc;
            if (mph >= 1.0f)
                mph -= 1.0f;
            amp *= v->adec;
            idx *= v->idec;
            namp *= v->ndec;
            mixl[j] += x * v->gl;
            mixr[j] += x * v->gr;
            rsend[j] += x * 0.6f;
            dsend[j] += x * 0.45f;
        }
        v->cph = cph;
        v->mph = mph;
        v->idx = idx;
        v->amp = amp;
        v->namp = namp;
        v->s[0] = fm_flush(v->s[0]);
        v->s[1] = fm_flush(v->s[1]);
        v->on = amp > 0.0005f || namp > 0.0005f;
        lvl = fm_maxf(lvl, amp + namp);
    }
    en_level[LY_METAL] = fm_minf(lvl, 1.0f) * (float)par[P_METAL] * 0.01f;
}

/* ---------------------------------------------------------------- noise --- */
/* the bed: a drone (two saws on the root, beating, and the tritone above, under a wandering low-pass) or
 * static (noise through a wandering band-pass, and crackle); Texture crossfades them. The level follows
 * the section: up in the intro and the breakdown, down under a chorus. A build adds a riser (noise
 * through a band-pass sweeping up). */
static struct {
    float ph[3], ds[2], dc[4], ss[2], sc[4], rs[2], rc[4], lvl, tgt, crack, rise, rise_t;
} tex;
static float tex_t, tex_goal;                    /* the texture: 0 the drone .. 1 static; where it heads */

static void render_noise(uint32_t n)
{
    uint32_t j;
    const float g = vol[LY_NOISE] * mg[LY_NOISE] * 0.5f, t = (tex_t += (tex_goal - tex_t) * fm_minf((float)n * (1.0f / (0.4f * EN_SR)), 1.0f));
    const float wd = (1.0f - t) * 0.5f, wsn = t * 1.6f;
    const float f0 = mtof((float)(36 + root)) * (1.0f / EN_SR);
    const float inc[3] = {f0, f0 * 1.0035f, f0 * 1.41421f};
    float lvl = tex.lvl, rise = tex.rise, p0 = tex.ph[0], p1 = tex.ph[1], p2 = tex.ph[2], ck = tex.crack;
    tex.lvl += (tex.tgt - tex.lvl) * fm_minf((float)n / (1.5f * EN_SR), 1.0f);
    tex.rise += (tex.rise_t - tex.rise) * fm_minf((float)n / (0.05f * EN_SR), 1.0f);
    if (lvl < 1e-4f && tex.lvl < 1e-4f && rise < 1e-4f && tex.rise < 1e-4f) {
        en_level[LY_NOISE] = 0.0f;
        return;
    }
    svf_coefs(tex.dc, fm_clampf(260.0f * fm_exp2f(1.4f * mod[0].v + 0.8f * sweep + 1.0f), 60.0f, 3000.0f), 0.5f);
    svf_coefs(tex.sc, 1400.0f * fm_exp2f(1.8f * mod[1].v), 0.35f);
    svf_coefs(tex.rc, 250.0f * fm_exp2f(5.6f * tex.rise_t), 0.25f);
    for (j = 0; j < n; j++) {
        float x = 0.0f, bp, y;
        lvl += (tex.lvl - lvl) * 0.002f;
        if (wd > 0.0f) {
            float d = blep_saw(p0, inc[0]) + blep_saw(p1, inc[1]) + 0.45f * blep_saw(p2, inc[2]);
            x = svf(tex.ds, tex.dc, d, &bp) * wd;
        }
        p0 += inc[0];
        if (p0 >= 1.0f)
            p0 -= 1.0f;
        p1 += inc[1];
        if (p1 >= 1.0f)
            p1 -= 1.0f;
        p2 += inc[2];
        if (p2 >= 1.0f)
            p2 -= 1.0f;
        if (wsn > 0.0f) {
            float w = white();
            svf(tex.ss, tex.sc, w, &bp);
            if ((nz >> 20) < 3u)                 /* crackle: a few pops a second */
                ck = w;
            x += (bp * 1.3f + ck) * wsn;
            ck *= 0.7f;
        }
        y = x * lvl * g;
        if (rise > 1e-4f) {
            svf(tex.rs, tex.rc, white(), &bp);
            y += bp * rise * rise * g * 1.6f;
            rise += (tex.rise - rise) * 0.001f;
        }
        mixl[j] += y;
        mixr[j] += y;
        rsend[j] += y * 0.5f;
        dsend[j] += y * 0.1f;
    }
    tex.ph[0] = p0;
    tex.ph[1] = p1;
    tex.ph[2] = p2;
    tex.crack = ck;
    tex.ds[0] = fm_flush(tex.ds[0]);
    tex.ds[1] = fm_flush(tex.ds[1]);
    tex.ss[0] = fm_flush(tex.ss[0]);
    tex.ss[1] = fm_flush(tex.ss[1]);
    tex.rs[0] = fm_flush(tex.rs[0]);
    tex.rs[1] = fm_flush(tex.rs[1]);
    en_level[LY_NOISE] = fm_minf(fm_maxf(lvl, rise), 1.0f) * (float)par[P_NOISE] * 0.01f;
}

/* ============================================================== material === */
/* The song (ind.h, en_song): three parts, each a bar of every layer and four chords; the plan; the metal's
 * two rings. On auto the generators below write it, and Evolve changes it a little as it plays; locked,
 * only the player does. Inside this file a part's notes go by short names. */
#define REST EN_REST
enum { F_ACC = EN_F_ACC, F_SLIDE = EN_F_SLIDE, F_LONG = EN_F_LONG, F_OPEN = EN_F_OPEN, F_OCT = EN_F_OCT,
       F_LEAD = EN_F_LEAD };
typedef en_note_t nstep_t;
en_song_t en_song;
const char *const EN_PART_NAME[EN_NPARTS] = {"A", "B", "C"};
int en_part_of(int s) { return s == S_CHORUS || s == S_DROP ? 1 : s == S_BREAK ? 2 : 0; }

/* the moves away from the root: the b2 first (in Phrygian), the minor third, the fifth, the b6, the b7 */
static const int8_t MOVES[8] = {1, 1, 2, 4, 5, -1, 3, 6};
/* rhythms: all sixteenths, gallops (x.xx), eighths, xx.x, dotted eighths, half notes, a syncopation, both */
static const uint16_t CELL[8] = {0xFFFF, 0xDDDD, 0x5555, 0xBBBB, 0x9249, 0x0101, 0x2929, 0x5D5D};
static const uint8_t BASS_CELLS[EN_NGROOVE][4] = {
    {0, 1, 0, 7}, {2, 0, 7, 3}, {6, 2, 4, 3}, {5, 4, 2, 6}, {0, 2, 1, 0}, {2, 3, 7, 1}};
/* the riff's chugs, from sparse to busy (Riff picks) */
static const uint16_t CHUG[6] = {0x1111, 0x5555, 0x9249, 0xDDDD, 0xBBBB, 0xFFFF};

/* the drum grooves: kick, snare, hats, for the verse and the chorus. X accent, x hit, g ghost, o open */
static const char *const GROOVE[EN_NGROOVE][2][3] = {
    {{"X...X...X...X...", "....X.......X...", "..x...x...x...x."},      /* EBM: four on the floor */
     {"X...X...X...X...", "....X.......X...", "x.o.x.o.x.o.x.o."}},
    {{"X.....x...X.....", "....X.......X...", "x.x.x.x.x.x.x.x."},      /* rock */
     {"X.....x.X.x.....", "....X.......X...", "o.x.o.x.o.x.o.x."}},
    {{"X.........X..x..", "....X..g.g..X...", "xxxxxxxxxxxxxxxx"},      /* a sampled break */
     {"X.x.......X..x..", "....X..g.g..X..g", "x.xxx.xxx.xxx.xo"}},
    {{"X.........x.....", "........X.......", "x...x...x...x..."},      /* half time */
     {"X.x.......X.x...", "........X.......", "x.x.x.x.x.x.x.x."}},
    {{"X.x.X.x.X.x.X.x.", "....X.......X...", "x...x...x...x..."},      /* thrash */
     {"XxxxXxxxXxxxXxxx", "....X.......X...", "o...o...o...o..."}},
    {{"X...X...X...X...", "....X..g....X...", "x.x.x.x.x.x.x.x."},      /* a stomp */
     {"X...X...X.x.X...", "....X.......X.xg", "x.xxx.xxx.xxx.xx"}},
};
static int vel_of(char c)
{
    return c == 'X' ? EN_VEL_ACC : c == 'x' || c == 'o' ? EN_VEL_HIT : c == 'g' ? EN_VEL_GHOST : 0;
}

static void gen_drums(en_part_t *pt, int part)
{
    int s;
    for (s = 0; s < EN_NSTEP; s++) {
        pt->tom[s] = 0;
        pt->tom_kind[s] = EN_TOM_HIGH;
        pt->hat_kind[s] = EN_HAT_CLOSED;
    }
    if (part == 2) {                             /* the breakdown: tribal toms over a lone kick */
        static const char TRIBE[] = "X..x..X...x..x..";
        for (s = 0; s < EN_NSTEP; s++) {
            pt->kick[s] = s == 0 ? 110 : 0;
            pt->snare[s] = pt->hat[s] = 0;
            pt->tom[s] = (uint8_t)vel_of(TRIBE[s]);
            pt->tom_kind[s] = (uint8_t)(s < 6 ? EN_TOM_LOW : s < 11 ? EN_TOM_MID : EN_TOM_LOW);
        }
        return;
    }
    {
        const char *const *pat = GROOVE[par[P_GROOVE]][part];
        for (s = 0; s < EN_NSTEP; s++) {
            pt->kick[s] = (uint8_t)vel_of(pat[0][s]);
            pt->snare[s] = (uint8_t)vel_of(pat[1][s]);
            pt->hat[s] = (uint8_t)vel_of(pat[2][s]);
            pt->hat_kind[s] = pat[2][s] == 'o' ? EN_HAT_OPEN : EN_HAT_CLOSED;
        }
    }
}

static void gen_bass(nstep_t *p, int part)
{
    int s, c = BASS_CELLS[par[P_GROOVE]][irand(4)], last = 0;
    const uint16_t cell = CELL[c];
    const float oct = (par[P_GROOVE] == G_EBM || par[P_GROOVE] == G_STOMP) ? 0.35f : 0.12f, move = part ? 0.3f : 0.18f;
    for (s = 0; s < EN_NSTEP; s++) {
        p[s].n = REST;
        p[s].f = 0;
    }
    if (part == 2) {                             /* the breakdown: a long note, perhaps another halfway */
        p[0].n = 0;
        p[0].f = F_LONG;
        if (frand() < 0.5f) {
            p[8].n = en_song.part[0].bass[8].n == REST ? 0 : en_song.part[0].bass[8].n;
            p[8].f = F_LONG;
        }
        return;
    }
    for (s = 0; s < EN_NSTEP; s++) {
        if (!(cell >> s & 1u))
            continue;
        p[s].n = 0;
        if (c == 5)
            p[s].f |= F_LONG;
        if ((s & 3) == 0)
            p[s].f |= F_ACC;
        if ((s & 1) && frand() < oct)
            p[s].f |= F_OCT;                     /* EBM's octaves */
        else if (s >= 6 && frand() < move)
            p[s].n = MOVES[irand(8)];
        if (p[s].n != last && s && p[s - 1].n != REST && frand() < 0.15f)
            p[s].f |= F_SLIDE;
        last = p[s].n;
    }
}

static void gen_riff(nstep_t *p, int part)
{
    int s, k = (int)((float)par[P_RIFF] * 0.0599f);    /* 0..5 */
    uint16_t cell;
    if (par[P_GROOVE] == G_THRASH && k < 3)
        k = 3 + irand(3);
    cell = CHUG[k];
    for (s = 0; s < EN_NSTEP; s++) {
        p[s].n = REST;
        p[s].f = 0;
    }
    if (part == 2)                               /* the breakdown: the guitars wait */
        return;
    if (part == 1) {                             /* open chords on the strong beats, chugs between */
        int every = par[P_GROOVE] == G_HALF || frand() < 0.5f ? 8 : 4;
        for (s = 0; s < EN_NSTEP; s++) {
            if (s % every == 0) {
                p[s].n = (s && frand() < 0.35f) ? MOVES[irand(8)] : 0;
                p[s].f = F_OPEN | F_ACC;
            } else if (s % every >= 2 && (cell >> s & 1u)) {
                p[s].n = 0;
            }
        }
        return;
    }
    for (s = 0; s < EN_NSTEP; s++)               /* the verse: palm-muted chugs on the root */
        if (cell >> s & 1u)
            p[s].n = 0;
    for (s = 0; s < 3; s++) {                    /* and a few accents on the darker degrees */
        static const uint8_t AT[6] = {3, 6, 7, 10, 11, 14};
        int at = AT[irand(6)];
        p[at].n = MOVES[irand(8)];
        p[at].f = F_ACC | (frand() < 0.5f ? F_OPEN : 0);
    }
}

/* the synth: stabs on a rhythm (sparse to busy), a chord on each; with Busy a line in the chorus */
static const uint16_t STAB[8] = {0x0001, 0x0101, 0x0841, 0x0441, 0x1111, 0x4444, 0x2222, 0x4949};
static void gen_synth(nstep_t *p, int part)
{
    static const int8_t LINE[8] = {0, 2, 4, 2, 7, 4, 1, 2};
    float busy = (float)par[P_SBUSY] * 0.01f;
    int s, k = (int)(busy * 5.0f) + irand(3);
    uint16_t cell = STAB[k > 7 ? 7 : k];
    for (s = 0; s < EN_NSTEP; s++) {
        p[s].n = REST;
        p[s].f = 0;
    }
    if (part == 2) {                             /* the breakdown: a long dark chord or two */
        p[0].n = 0;
        p[0].f = F_LONG;
        if (frand() < 0.6f) {
            p[8].n = MOVES[irand(8)];
            p[8].f = F_LONG;
        }
        return;
    }
    if (part == 1 && frand() < busy * 0.8f) {    /* the chorus: a line */
        int r = irand(8), every = busy > 0.7f ? 1 : 2;
        for (s = 0; s < EN_NSTEP; s += every) {
            p[s].n = LINE[(s / every + r) & 7];
            p[s].f = F_LEAD | (s % 4 == 0 ? F_ACC : 0);
        }
        return;
    }
    for (s = 0; s < EN_NSTEP; s++)
        if (cell >> s & 1u) {
            p[s].n = (s && frand() < 0.25f) ? MOVES[irand(8)] : 0;
            p[s].f = s % 4 == 0 ? F_ACC : 0;
        }
}

/* the clangs' three pitches, from the song's metal tone */
static void metal_tune(void)
{
    uint32_t h = (uint32_t)en_song.metal_tone * 2654435761u + 12345u;
    int i;
    for (i = 0; i < 3; i++) {
        h ^= h >> 15;
        h *= 0x2C1B3C6Du;
        h ^= h >> 12;
        metal_f[i] = 420.0f * fm_exp2f((float)(h >> 24) * (1.7f / 256.0f));
    }
}

static int ring_len(int r)
{
    int n = en_song.ring_len[r & 1];
    return n < 1 ? 1 : n > EN_NSTEP ? EN_NSTEP : n;
}

/* Hits: up to six anvils in sixteen, the scrap up to half its ring */
static void rings_hits(void)
{
    const int h = par[P_HITS], n0 = ring_len(0), n1 = ring_len(1);
    en_song.ring_hits[0] = (uint8_t)((h * n0 * 3 / 8 + 50) / 100);
    en_song.ring_hits[1] = (uint8_t)((h * (n1 / 2) + 50) / 100);
}

static void gen_rings(void)                     /* the anvil on sixteen steps, the scrap on 12, 9, 7 or 5 */
{
    static const uint8_t N2[4] = {12, 7, 5, 9};
    en_song.ring_len[0] = EN_NSTEP;
    en_song.ring_len[1] = N2[irand(4)];
    en_song.ring_turn[0] = (uint8_t)irand(EN_NSTEP);
    en_song.ring_turn[1] = (uint8_t)irand(en_song.ring_len[1]);
    en_song.metal_tone = (uint8_t)irand(256);
    rings_hits();
    metal_tune();
}

/* the progressions: the first five stay home (verses), the rest move (choruses) */
static const int8_t PROG[10][4] = {{0, 0, 0, 0}, {0, 0, 0, 1}, {0, 0, 5, 6}, {0, 0, 0, 5}, {0, 0, 3, 3},
                                   {5, 6, 0, 0}, {0, 2, 0, 1}, {0, 3, 0, 4}, {0, 5, 3, 1}, {0, 1, 0, 6}};
static void gen_progs(void)
{
    int i, a = irand(5), b = frand() < 0.8f ? 5 + irand(5) : irand(5);
    for (i = 0; i < 4; i++) {
        en_song.part[0].prog[i] = PROG[a][i];
        en_song.part[1].prog[i] = PROG[b][i];
        en_song.part[2].prog[i] = (int8_t)(i >= 2 && frand() < 0.4f ? 5 : 0);
    }
}

/* what to write: a layer's bit for its lines (the metal's: the rings' hits), and these */
enum { W_DRUMS = 1 << LY_DRUMS, W_BASS = 1 << LY_BASS, W_GTR = 1 << LY_GUITAR, W_SYN = 1 << LY_SYNTH,
       W_HITS = 1 << LY_METAL, W_LINES = W_BASS | W_GTR | W_SYN, W_PROG = 64, W_RINGS = 128, W_ALL = 255 };
static void gen(int what, int parts)            /* parts: bit p, part p */
{
    int p;
    for (p = 0; p < EN_NPARTS; p++) {
        en_part_t *pt = &en_song.part[p];
        if (!(parts >> p & 1))
            continue;
        if (what & W_DRUMS)
            gen_drums(pt, p);
        if (what & W_BASS)
            gen_bass(pt->bass, p);
        if (what & W_GTR)
            gen_riff(pt->gtr, p);
        if (what & W_SYN)
            gen_synth(pt->syn, p);
    }
    if (what & W_PROG)
        gen_progs();
    if (what & W_RINGS)
        gen_rings();
    else if (what & W_HITS)
        rings_hits();
}

/* Evolve, every four bars on auto: one small change somewhere */
static void evolve_step(void)
{
    en_part_t *pt = &en_song.part[irand(2)];
    int s = irand(EN_NSTEP);
    if (frand() >= evolve * 0.6f)
        return;
    switch (irand(4)) {
    case 0:                                      /* a bass note moves, or comes or goes */
        if (pt->bass[s].n == REST)
            pt->bass[s].n = 0;
        else if (s && frand() < 0.5f)
            pt->bass[s].n = REST;
        else
            pt->bass[s].n = MOVES[irand(8)];
        break;
    case 1:                                      /* a chug becomes an accent, or an accent a chug */
        if (pt->gtr[s].n != REST && !(pt->gtr[s].f & F_OPEN)) {
            pt->gtr[s].n = MOVES[irand(8)];
            pt->gtr[s].f = F_ACC | F_OPEN;
        } else if (pt->gtr[s].f & F_OPEN && s % 4) {
            pt->gtr[s].n = 0;
            pt->gtr[s].f = 0;
        }
        break;
    case 2: {                                    /* the stabs turn a sixteenth */
        nstep_t t = pt->syn[EN_NSTEP - 1];
        int k;
        if (pt->syn[0].f & F_LEAD)
            break;
        for (k = EN_NSTEP - 1; k > 0; k--)
            pt->syn[k] = pt->syn[k - 1];
        pt->syn[0] = t;
        break;
    }
    default: {                                   /* the metal turns */
        int r = irand(2);
        en_song.ring_turn[r] = (uint8_t)((en_song.ring_turn[r] + 1) % ring_len(r));
        break;
    }
    }
}

void en_song_defaults(en_song_t *sg)
{
    uint8_t *b = (uint8_t *)sg;
    uint32_t i;
    int p, s;
    for (i = 0; i < sizeof *sg; i++)
        b[i] = 0;
    for (p = 0; p < EN_NPARTS; p++)
        for (s = 0; s < EN_NSTEP; s++)
            sg->part[p].bass[s].n = sg->part[p].gtr[s].n = sg->part[p].syn[s].n = REST;
    sg->plan_n = 1;
    sg->plan[0].type = S_VERSE;
    sg->plan[0].layers = 1u << LY_DRUMS | 1u << LY_BASS;
    sg->ring_len[0] = EN_NSTEP;
    sg->ring_len[1] = 12;
    sg->ring_hits[0] = sg->ring_hits[1] = 2;
}

static int note_ok(const en_note_t *n) { return (n->n == REST || (n->n >= -7 && n->n <= 14)) && n->f < 64u; }
int en_song_valid(const en_song_t *sg)
{
    int p, s, r;
    if (sg->plan_n < 1 || sg->plan_n > EN_PLAN_MAX || sg->locked > 1u)
        return 0;
    for (s = 0; s < sg->plan_n; s++) {
        const en_entry_t *e = &sg->plan[s];
        if (e->type >= EN_NSECT || e->bars > EN_MAX_BARS || e->layers >= 1u << EN_NLAYER || e->tex >= EN_NTEX)
            return 0;
    }
    for (r = 0; r < 2; r++)
        if (sg->ring_len[r] < 1 || sg->ring_len[r] > EN_NSTEP || sg->ring_hits[r] > sg->ring_len[r] ||
            sg->ring_turn[r] >= sg->ring_len[r])
            return 0;
    for (p = 0; p < EN_NPARTS; p++) {
        const en_part_t *pt = &sg->part[p];
        for (s = 0; s < 4; s++)
            if (pt->prog[s] < -7 || pt->prog[s] > 14)
                return 0;
        for (s = 0; s < EN_NSTEP; s++)
            if (pt->kick[s] > 127 || pt->snare[s] > 127 || pt->hat[s] > 127 || pt->tom[s] > 127 ||
                pt->hat_kind[s] > EN_HAT_CRASH || pt->tom_kind[s] > EN_TOM_LOW || !note_ok(&pt->bass[s]) ||
                !note_ok(&pt->gtr[s]) || !note_ok(&pt->syn[s]))
                return 0;
    }
    return 1;
}

/* ============================================================== the song === */
typedef struct {
    uint8_t kick, snare, hat, tom;              /* velocities, 0 none */
    uint8_t hat_kind, tom_kind, roll, boom;     /* EN_HAT_*, EN_TOM_*; roll: a 32nd after; boom: an impact */
    uint8_t metal;                              /* bit 0 an anvil, bit 1 scrap */
    nstep_t bass, gtr, syn;
} step_t;
static step_t bar[EN_NSTEP];                    /* the bar playing: its part, as its section treats it */

static int sect = S_INTRO, prev_sect = S_INTRO, sect_bar, sect_len = 8, plan_pos, jumped, part;
static int8_t jq[3];
static int jq_n;
static int pend_kind;                           /* at the bar: 1 the next section, 2 a breakdown (or on from it) */
static uint8_t pend_fill, pend_mutate, pend_write, pend_act, pend_top, pend_rebuild, bar_fresh, fill_on;
static uint32_t sixteenth;
static int chord_deg;
static int stop_at = EN_NSTEP;                  /* a stop's silence from this step (EN_NSTEP: none) */
static uint8_t tex_knob = 1;                    /* the section's bed follows Texture */

/* which layers play a section, in percent (Intensity leans every one up or down) */
static const uint8_t PACT[EN_NSECT][EN_NLAYER] = {
    {45, 50, 0, 50, 80, 100},       /* intro */
    {100, 100, 35, 60, 70, 60},     /* verse: the guitars mostly wait for the chorus */
    {100, 100, 30, 70, 50, 100},    /* build */
    {100, 100, 100, 85, 60, 50},    /* chorus */
    {35, 45, 0, 85, 85, 100},       /* breakdown */
    {100, 100, 100, 100, 80, 50},   /* drop */
    {60, 50, 30, 50, 60, 100}};     /* outro */
static const float SECT_NOISE[EN_NSECT] = {1.0f, 0.45f, 0.6f, 0.35f, 1.0f, 0.35f, 0.9f};

/* a section's layers: by chance (dice), or as they mostly fall */
static uint8_t pick_layers(int s, int dice)
{
    int l;
    float bias = (intensity - 0.5f) * 100.0f;
    uint8_t a = 0;
    for (l = 0; l < EN_NLAYER; l++)
        if ((dice ? frand() * 100.0f : 50.0f) < (float)PACT[s][l] + bias)
            a |= (uint8_t)(1u << l);
    if (s == S_VERSE || s == S_CHORUS || s == S_DROP || s == S_BUILD)
        a |= 1u << LY_DRUMS | 1u << LY_BASS;     /* these always drive */
    if (s == S_CHORUS || s == S_DROP)
        a |= (uint8_t)(intensity > 0.2f ? 1u << LY_GUITAR : 0u);
    if (!(a & (1u << LY_NOISE | 1u << LY_METAL | 1u << LY_SYNTH)) && (s == S_INTRO || s == S_BREAK))
        a |= 1u << LY_NOISE;
    return a;
}

uint8_t en_layers_for(int s) { return pick_layers((unsigned)s < EN_NSECT ? s : S_VERSE, 0); }

static void plan_song(void)
{
    uint8_t t[EN_PLAN_MAX];
    int n = 0, i;
#define P(s) (t[n++] = (uint8_t)(s))
    P(S_INTRO);
    P(S_VERSE);
    if (frand() < 0.7f)
        P(S_BUILD);
    P(S_CHORUS);
    P(S_VERSE);
    if (frand() < 0.5f)
        P(S_BUILD);
    P(S_CHORUS);
    P(S_BREAK);
    P(S_BUILD);
    P(S_DROP);
    if (frand() < 0.4f)
        P(S_CHORUS);
    P(S_OUTRO);
#undef P
    for (i = 0; i < n; i++) {
        en_entry_t *e = &en_song.plan[i];
        e->type = t[i];
        e->bars = 0;
        e->layers = pick_layers(t[i], 1);
        e->tex = EN_TEX_KNOB;
    }
    en_song.plan_n = (uint8_t)n;
}

static int plan_count(void)
{
    int n = en_song.plan_n;
    return n < 1 ? 1 : n > EN_PLAN_MAX ? EN_PLAN_MAX : n;
}
static int plan_type(int i)
{
    int t = en_song.plan[(unsigned)i < (unsigned)plan_count() ? i : 0].type;
    return t < EN_NSECT ? t : S_VERSE;
}
static int entry_len(const en_entry_t *e)
{
    if (e->bars)
        return e->bars < EN_MAX_BARS ? e->bars : EN_MAX_BARS;
    return en_sect_bars(e->type < EN_NSECT ? e->type : S_VERSE, par[P_ARRANGE]);
}
/* the plan's entry the section playing follows: its own, or (a jump) the plan's next of its kind */
static const en_entry_t *sect_entry(void)
{
    int i, n = plan_count();
    for (i = 0; i < n; i++) {
        int k = (plan_pos + i) % n;
        if (plan_type(k) == sect && (jumped || !i))
            return &en_song.plan[k];
        if (!jumped)
            break;
    }
    return 0;
}

static int peek_next(void);
static void plan_publish(void)
{
    int i, n = plan_count();
    for (i = 0; i < n; i++) {
        en_plan[i] = (uint8_t)plan_type(i);
        en_plan_len[i] = (uint8_t)entry_len(&en_song.plan[i]);
    }
    en_plan_n = (uint8_t)n;
    en_plan_pos = (uint8_t)plan_pos;
    en_jumped = (uint8_t)jumped;
    en_sect_next = (uint8_t)peek_next();
}

/* the section's length (len_too), its layers and its bed, from its entry */
static void sect_resolve(int len_too)
{
    const en_entry_t *e = sect_entry();
    if (len_too)
        sect_len = e ? entry_len(e) : en_sect_bars(sect, par[P_ARRANGE]);
    act = e ? (uint8_t)(e->layers & ((1u << EN_NLAYER) - 1u)) : pick_layers(sect, 0);
    tex_knob = !e || e->tex == EN_TEX_KNOB || e->tex >= EN_NTEX;
    tex_goal = tex_knob ? (float)par[P_TEXTURE] * 0.01f : e->tex == EN_TEX_DRONE ? 0.0f : e->tex == EN_TEX_STATIC ? 1.0f : 0.5f;
    tex.tgt = en_playing && (act >> LY_NOISE & 1u) ? SECT_NOISE[sect] : 0.0f;
    en_active = act;
    en_sect_len = (uint8_t)sect_len;
}

static void sect_begin(int s)
{
    prev_sect = sect;
    sect = s;
    sect_bar = 0;
    part = en_part_of(s);
    sect_resolve(1);
    en_sect = (uint8_t)s;
    en_part = (uint8_t)part;
    en_sect_bar = 0;
    plan_publish();
}

static void song_top(void)                      /* the plan from its first section */
{
    plan_pos = 0;
    jumped = 0;
    jq_n = 0;
    sect_begin(plan_type(0));
}

static void new_song(void)                      /* the plan played through: on auto a new song, else again */
{
    if (!en_song.locked && evolve > 0.0f) {
        gen(W_ALL, 7);
        plan_song();
    }
    plan_pos = 0;
    jumped = 0;
    en_songs++;
}

static void sect_next(void)                     /* the next section: a jump's, or the plan's */
{
    int s;
    if (jq_n) {
        s = jq[0];
        jq[0] = jq[1];
        jq[1] = jq[2];
        jq_n--;
        jumped = 1;
    } else {
        jumped = 0;
        if (++plan_pos >= plan_count())
            new_song();
        s = plan_type(plan_pos);
    }
    sect_begin(s);
}

static void sect_break(void)                    /* REC: a breakdown; in one, the build; in the build, the drop */
{
    jumped = 1;
    if (sect == S_BREAK) {
        jq[0] = S_DROP;
        jq_n = 1;
        sect_begin(S_BUILD);
    } else if (sect == S_BUILD) {
        jq_n = 0;
        sect_begin(S_DROP);
    } else {
        jq[0] = S_BUILD;
        jq[1] = S_DROP;
        jq_n = 2;
        sect_begin(S_BREAK);
    }
}

static int peek_next(void)                      /* the section after this one */
{
    if (jq_n)
        return jq[0];
    if (plan_pos + 1 < plan_count())
        return plan_type(plan_pos + 1);
    return en_song.locked || evolve <= 0.0f ? plan_type(0) : S_INTRO;
}

/* a fill from step st to the bar's end; kind 0 the snare, 1 the toms, 2 kick and snare together, 3 a stop */
enum { FILL_SNARE, FILL_TOMS, FILL_UNISON, FILL_STOP };
static void fill_from(int st, int kind)
{
    int s;
    if (st >= EN_NSTEP)
        return;
    for (s = st; s < EN_NSTEP; s++) {
        bar[s].snare = bar[s].tom = bar[s].roll = 0;
        bar[s].hat = 0;
        if (kind == FILL_STOP && s > st) {       /* a stop: one hit, then nothing at all */
            bar[s].kick = 0;
            bar[s].metal = 0;
            bar[s].bass.n = bar[s].gtr.n = bar[s].syn.n = REST;
            if (s < stop_at)
                stop_at = s;
        } else if (s % 4) {
            bar[s].kick = 0;
        }
    }
    switch (kind) {
    case FILL_SNARE:
        for (s = st; s < EN_NSTEP; s++)
            bar[s].snare = (uint8_t)(60 + 60 * (s - st) / (EN_NSTEP - st));
        bar[EN_NSTEP - 1].roll = 1;
        break;
    case FILL_TOMS:
        for (s = st; s < EN_NSTEP; s++) {
            if ((s - st) % 2 && s < 14)
                continue;
            bar[s].tom = (uint8_t)(90 + 30 * (s - st) / (EN_NSTEP - st));
            bar[s].tom_kind = (uint8_t)(3 * (s - st) / (EN_NSTEP - st));
        }
        break;
    case FILL_UNISON:
        for (s = st; s < EN_NSTEP; s++)
            if (!(s & 1) || s >= 14) {
                bar[s].kick = 110;
                bar[s].snare = 105;
            }
        break;
    default:
        bar[st].kick = 127;
        bar[st].snare = 120;
        bar[st].hat = 110;
        bar[st].hat_kind = EN_HAT_CRASH;
        break;
    }
    fill_on = 1;
}

static void grid_publish(void)
{
    int s, r;
    uint16_t g[EN_NROW] = {0};
    for (s = 0; s < EN_NSTEP; s++) {
        const step_t *e = &bar[s];
        uint8_t v[EN_NROW];
        v[EN_ROW_KICK] = e->kick;
        v[EN_ROW_SNARE] = e->snare > e->tom ? e->snare : e->tom;
        v[EN_ROW_HAT] = e->hat;
        v[EN_ROW_METAL] = e->metal ? 100 : 0;
        v[EN_ROW_BASS] = e->bass.n != REST ? ((e->bass.f & F_ACC) ? 120 : 80) : 0;
        v[EN_ROW_GUITAR] = e->gtr.n != REST ? ((e->gtr.f & F_OPEN) ? 120 : 70) : 0;
        v[EN_ROW_SYNTH] = e->syn.n != REST ? ((e->syn.f & F_ACC) ? 120 : 80) : 0;
        for (r = 0; r < EN_NROW; r++) {
            en_grid_vel[r][s] = v[r];
            if (v[r])
                g[r] |= (uint16_t)(1u << s);
        }
    }
    for (r = 0; r < EN_NROW; r++)
        en_grid[r] = g[r];
}

/* the bar ahead, every layer, at its downbeat: the section's part, as the section treats it. On auto
 * the fills and the ghosts fall as they may; locked, each bar of the song falls the same way every time */
static void bar_build(void)
{
    const en_part_t *pt = &en_song.part[part];
    const int len = sect_len ? sect_len : 4, last = sect_len && sect_bar == sect_len - 1;
    const uint32_t dice = rng, base = sixteenth & ~15u;
    int s, next = last ? peek_next() : sect;
    if (en_song.locked) {                        /* the bar's own dice */
        rng = ((uint32_t)(plan_pos * 64 + sect_bar) * 2654435761u) ^ ((uint32_t)(sect * 2 + jumped) * 40503u) ^ 0x5EED5u;
        rnd();
        rnd();
    }
    for (s = 0; s < EN_NSTEP; s++) {
        bar[s].kick = bar[s].snare = bar[s].hat = bar[s].tom = 0;
        bar[s].hat_kind = bar[s].tom_kind = bar[s].roll = bar[s].boom = bar[s].metal = 0;
        bar[s].bass.n = bar[s].gtr.n = bar[s].syn.n = REST;
        bar[s].bass.f = bar[s].gtr.f = bar[s].syn.f = 0;
    }
    fill_on = 0;
    stop_at = EN_NSTEP;
    {   /* the progression: a quarter of the section a chord (four bars a chord while looping) */
        int slot = sect_len >= 4 ? sect_len / 4 : sect_len ? 1 : 4;
        chord_deg = sect == S_BUILD ? 0 : pt->prog[(sect_bar / slot) & 3];
        en_chord_root = (uint8_t)((root + en_degree(pool, chord_deg) + 12 * 3) % 12);
    }
    if (act >> LY_DRUMS & 1u) {
        int k;
        for (s = 0; s < EN_NSTEP; s++) {
            bar[s].kick = pt->kick[s];
            bar[s].snare = pt->snare[s];
            bar[s].hat = pt->hat[s];
            bar[s].hat_kind = pt->hat_kind[s];
            bar[s].tom = pt->tom[s];
            bar[s].tom_kind = pt->tom_kind[s];
        }
        switch (sect) {
        case S_INTRO:                            /* hats, then the kick joins halfway */
            for (s = 0; s < EN_NSTEP; s++) {
                bar[s].snare = bar[s].tom = 0;
                if (sect_bar < len / 2)
                    bar[s].kick = 0;
                else
                    bar[s].kick = s % 4 ? 0 : 110;
            }
            break;
        case S_BUILD: {                          /* the roll: quarters, eighths, sixteenths, then 32nds */
            int qq = sect_bar * 4 / len, every = qq == 0 ? 4 : qq == 1 ? 2 : 1;
            for (s = 0; s < EN_NSTEP; s++) {
                bar[s].kick = s % 4 ? (last && !(s & 1) ? 100 : 0) : 115;
                bar[s].hat = bar[s].tom = 0;
                bar[s].snare = s % every ? 0 : (uint8_t)(45 + 75 * (sect_bar * 16 + s) / (len * 16));
                bar[s].roll = last && s >= 8;
            }
            break;
        }
        case S_OUTRO:                            /* thinning out */
            if (sect_bar >= len / 2)
                for (s = 0; s < EN_NSTEP; s++)
                    bar[s].snare = bar[s].hat = bar[s].tom = 0;
            break;
        default:
            break;
        }
        if (!en_song.locked && sect != S_BUILD && sect != S_BREAK) {   /* Evolve: a ghost, an open hat */
            if (frand() < evolve * 0.35f) {
                static const uint8_t AT[5] = {7, 10, 11, 14, 15};
                k = AT[irand(5)];
                if (!bar[k].kick)
                    bar[k].kick = 70;
            }
            if (frand() < evolve * 0.3f && !bar[k = 3 + 4 * irand(4)].snare)
                bar[k].snare = 35;
            if (frand() < evolve * 0.25f && bar[14].hat)
                bar[14].hat_kind = EN_HAT_OPEN;
        }
        if (sect_bar == 0 && (sect == S_DROP || sect == S_CHORUS)) {
            bar[0].hat = 120;                    /* the downbeat: a crash; into a drop, an impact */
            bar[0].hat_kind = EN_HAT_CRASH;
            bar[0].boom = sect == S_DROP || prev_sect == S_BUILD;
        }
        if (last && next != sect && sect != S_BUILD) {   /* the section's last bar fills */
            int kind = irand(3), st = 12 - 4 * irand(2);
            if ((next == S_CHORUS || next == S_DROP || next == S_BREAK) && frand() < 0.3f)
                kind = FILL_STOP;
            if (kind == FILL_STOP)
                st = 12;
            fill_from(st, kind);
        } else if (!last && (sect_bar & 7) == 7 && frand() < 0.25f + 0.4f * evolve && sect != S_BUILD && sect != S_BREAK) {
            fill_from(12, irand(3));             /* a phrase's end: a short one */
        }
        if (pend_fill) {
            fill_from(8, irand(3));
            pend_fill = 0;
        }
    }
    {   /* the metal: two Euclidean rings, each turning on its own length across the bars */
        const int n0 = ring_len(0), n1 = ring_len(1);
        const uint32_t g0 = en_euclid(en_song.ring_hits[0], n0, en_song.ring_turn[0] % n0);
        const uint32_t g1 = en_euclid(en_song.ring_hits[1], n1, en_song.ring_turn[1] % n1);
        for (s = 0; s < EN_NSTEP; s++)
            if (act >> LY_METAL & 1u)
                bar[s].metal = (uint8_t)((g0 >> ((base + (uint32_t)s) % (uint32_t)n0) & 1u) |
                                         (g1 >> ((base + (uint32_t)s) % (uint32_t)n1) & 1u) << 1);
    }
    for (s = 0; s < EN_NSTEP; s++) {
        step_t *e = &bar[s];
        if (act >> LY_BASS & 1u) {
            if (sect == S_BUILD) {               /* pulses: eighths, then sixteenths */
                if (!(s & 1) || sect_bar * 2 >= len) {
                    e->bass.n = 0;
                    e->bass.f = (s & 3) ? 0 : F_ACC;
                }
            } else if (sect == S_INTRO) {        /* long notes: the downbeat's, and halfway's */
                if (s == 0 || (s == 8 && pt->bass[8].n != REST)) {
                    e->bass.n = pt->bass[s].n == REST ? 0 : pt->bass[s].n;
                    e->bass.f = F_LONG;
                }
            } else {
                e->bass = pt->bass[s];
            }
        }
        if (act >> LY_GUITAR & 1u) {
            if (sect == S_BUILD) {
                if (!(s & 1) || sect_bar * 2 >= len) {
                    e->gtr.n = 0;
                    e->gtr.f = 0;
                }
            } else if (sect != S_INTRO) {
                e->gtr = pt->gtr[s];
            }
        }
        if (act >> LY_SYNTH & 1u) {
            if (sect != S_INTRO) {
                e->syn = pt->syn[s];
            } else if ((s == 0 || s == 8) && pt->syn[s].n != REST) {   /* the intro: the half bars, held */
                e->syn = pt->syn[s];
                e->syn.f |= F_LONG;
            }
        }
    }
    for (s = stop_at; s < EN_NSTEP; s++) {       /* a stop takes the others with it */
        bar[s].bass.n = bar[s].gtr.n = bar[s].syn.n = REST;
        bar[s].metal = 0;
    }
    if (en_song.locked)
        rng = dice;
    grid_publish();
}

/* the rest of this bar, from the next beat (or halfway, if that is later): a fill. Too late in the bar:
 * the next bar's second half; but before a jump (EDIT, REC) the jump just comes */
static void fill_now(int jump)
{
    int nx = (int)(sixteenth & 15u), st = ((nx + 3) / 4) * 4;
    if (!en_playing || !(act >> LY_DRUMS & 1u))
        return;
    if (st < 8)
        st = 8;
    if (nx != 0 && st < EN_NSTEP) {
        fill_from(st, irand(3));
        grid_publish();
    } else if (!jump) {
        pend_fill = 1;
    }
}

/* ============================================================ the space === */
/* ------------------------------------------------- reverb: Dattorro's plate --- */
/* J. Dattorro, "Effect Design, Part 1", JAES 1997 (as in FoMni, as in AMB-1): a predelay, four input
 * diffusers, then a figure-eight tank of two halves, each a modulated allpass, a delay, a damping lowpass,
 * an allpass and a delay, feeding the other half. Stereo out from seven taps a side. The paper's lengths
 * are for 29761 Hz; DS() scales them to 44.1 kHz. */
#define DS(n) (((n) * 14818 + 5000) / 10000)          /* 44100 / 29761 */
#define PRE_N 882                                    /* 20 ms */
#define EXC DS(16)
enum { RV_PRE, RV_IN1, RV_IN2, RV_IN3, RV_IN4, RV_APL, RV_D1L, RV_AP2L, RV_D2L, RV_APR, RV_D1R, RV_AP2R, RV_D2R, RV_N };
static const int DL_LEN[RV_N] = {PRE_N, DS(142), DS(107), DS(379), DS(277), DS(672) + EXC + 2, DS(4453), DS(1800),
                                 DS(3720), DS(908) + EXC + 2, DS(4217), DS(2656), DS(3163)};
#define DL_TOTAL (PRE_N + DS(142) + DS(107) + DS(379) + DS(277) + DS(672) + DS(4453) + DS(1800) + DS(3720) + DS(908) + \
                  DS(4217) + DS(2656) + DS(3163) + 2 * (EXC + 2))
static float pl_buf[DL_TOTAL] EN_POOL;
typedef struct {
    float *b;
    int n, i;
} dl_t;
static dl_t dl[RV_N];
static float pl_bw, pl_damp_l, pl_damp_r, pl_decay = 0.8f, pl_decay_t = 0.8f, pl_dd2 = 0.5f, lfo_s, lfo_c = 1.0f;
static float rev_send;

static inline float tap(const dl_t *d, int k)
{
    int j = d->i - k;
    return d->b[j < 0 ? j + d->n : j];
}
static inline void push(dl_t *d, float x)
{
    d->b[d->i] = x;
    if (++d->i >= d->n)
        d->i = 0;
}
static inline float allpass(dl_t *d, int len, float x, float g)
{
    float z = tap(d, len), v = x - g * z;
    push(d, fm_flush(v));
    return z + g * v;
}
static inline float allpass_mod(dl_t *d, float len, float x, float g)
{
    int k = (int)len;
    float f = len - (float)k, z = tap(d, k) + (tap(d, k + 1) - tap(d, k)) * f, v = x - g * z;
    push(d, fm_flush(v));
    return z + g * v;
}

static inline void reverb(float in, float *ol, float *or_)
{
    float x, a, b, fb_l = tap(&dl[RV_D2R], DS(3163)), fb_r = tap(&dl[RV_D2L], DS(3720));
    push(&dl[RV_PRE], in);
    x = tap(&dl[RV_PRE], PRE_N);
    pl_bw += 0.55f * (x - pl_bw);
    x = allpass(&dl[RV_IN1], DS(142), pl_bw, 0.75f);
    x = allpass(&dl[RV_IN2], DS(107), x, 0.75f);
    x = allpass(&dl[RV_IN3], DS(379), x, 0.625f);
    x = allpass(&dl[RV_IN4], DS(277), x, 0.625f);
    {
        float s = lfo_s + 0.0001f * lfo_c, c = lfo_c - 0.0001f * s;
        lfo_s = s;
        lfo_c = c;
    }
    a = allpass_mod(&dl[RV_APL], (float)DS(672) + (float)EXC * lfo_s, x + pl_decay * fb_l, -0.7f);
    push(&dl[RV_D1L], a);
    b = tap(&dl[RV_D1L], DS(4453));
    pl_damp_l += 0.4f * (b - pl_damp_l);
    b = allpass(&dl[RV_AP2L], DS(1800), pl_damp_l * pl_decay, pl_dd2);
    push(&dl[RV_D2L], b);
    a = allpass_mod(&dl[RV_APR], (float)DS(908) + (float)EXC * lfo_c, x + pl_decay * fb_r, -0.7f);
    push(&dl[RV_D1R], a);
    b = tap(&dl[RV_D1R], DS(4217));
    pl_damp_r += 0.4f * (b - pl_damp_r);
    b = allpass(&dl[RV_AP2R], DS(2656), pl_damp_r * pl_decay, pl_dd2);
    push(&dl[RV_D2R], b);
    *ol = 0.6f * (tap(&dl[RV_D1R], DS(266)) + tap(&dl[RV_D1R], DS(2974)) - tap(&dl[RV_AP2R], DS(1913)) +
                  tap(&dl[RV_D2R], DS(1996)) - tap(&dl[RV_D1L], DS(1990)) - tap(&dl[RV_AP2L], DS(187)) -
                  tap(&dl[RV_D2L], DS(1066)));
    *or_ = 0.6f * (tap(&dl[RV_D1L], DS(353)) + tap(&dl[RV_D1L], DS(3627)) - tap(&dl[RV_AP2L], DS(1228)) +
                   tap(&dl[RV_D2L], DS(2673)) - tap(&dl[RV_D1R], DS(2111)) - tap(&dl[RV_AP2R], DS(335)) -
                   tap(&dl[RV_D2R], DS(121)));
}

static void plate_init(void)
{
    float *p = pl_buf;
    int k, i;
    for (k = 0; k < RV_N; k++) {
        dl[k].b = p;
        dl[k].n = DL_LEN[k];
        dl[k].i = 0;
        p += DL_LEN[k];
    }
    for (i = 0; i < DL_TOTAL; i++)
        pl_buf[i] = 0.0f;
    pl_bw = pl_damp_l = pl_damp_r = 0.0f;
    lfo_s = 0.0f;
    lfo_c = 1.0f;
}

/* ------------------------------------------------------ the ping-pong delay --- */
/* a dotted eighth, left then right then left, each repeat darker; at half rate (22 kHz, 16-bit), which
 * the repeats don't miss */
#define DLY_N 16560                             /* 0.75 s at 22.05 kHz: a dotted eighth at 60 */
#define DLY_SR (EN_SR * 0.5f)
static int16_t dly_l[DLY_N] EN_POOL, dly_r[DLY_N] EN_POOL;
static int dly_w, dly_ph;
static float dly_len = 8000.0f, dly_len_t = 8000.0f, dly_acc, dly_lpl, dly_lpr, dly_pl, dly_pr, dly_cl, dly_cr, dly_send;

static inline float dread(const int16_t *b, float d)
{
    int k = (int)d, i0 = dly_w - k, i1;
    float f = d - (float)k, a, c;
    if (i0 < 0)
        i0 += DLY_N;
    i1 = i0 - 1;
    if (i1 < 0)
        i1 += DLY_N;
    a = (float)b[i0];
    c = (float)b[i1];
    return (a + (c - a) * f) * (1.0f / 12000.0f);
}
static inline int16_t s16(float x)
{
    x *= 12000.0f;
    return (int16_t)(x > 32767.0f ? 32767.0f : x < -32767.0f ? -32767.0f : x);
}

static void delay_time(void)
{
    float t = 0.75f * 60.0f / (float)(par[P_TEMPO] > 0 ? par[P_TEMPO] : 120);
    dly_len_t = fm_minf(t * DLY_SR, (float)(DLY_N - 8));
    if (dly_len <= 0.0f)
        dly_len = dly_len_t;
}

static inline void delays(float in, float *ol, float *or_)
{
    float f;
    dly_acc += in;
    if (++dly_ph >= 2) {
        float yl, yr;
        dly_ph = 0;
        dly_len += (dly_len_t - dly_len) * 0.0005f;
        yl = dread(dly_l, dly_len);
        yr = dread(dly_r, dly_len);
        dly_lpl += 0.5f * (yl - dly_lpl);
        dly_lpr += 0.5f * (yr - dly_lpr);
        if (++dly_w >= DLY_N)
            dly_w = 0;
        dly_l[dly_w] = s16(dly_acc * 0.5f + 0.62f * dly_lpr);   /* the left hears the send and the right's repeats */
        dly_r[dly_w] = s16(0.62f * dly_lpl);
        dly_acc = 0.0f;
        dly_pl = dly_cl;
        dly_pr = dly_cr;
        dly_cl = yl;
        dly_cr = yr;
    }
    f = (float)dly_ph * 0.5f;
    *ol = dly_pl + (dly_cl - dly_pl) * f;
    *or_ = dly_pr + (dly_cr - dly_pr) * f;
}

/* ============================================================ the grid === */
/* a part's notes: scale degree deg above the bar's chord, flags F_* */
static void bass_note(int deg, uint8_t f, int slide_next)
{
    int n = 36 + root + en_degree(pool, chord_deg + deg);
    int32_t len = (int32_t)(step_len * ((f & F_LONG) ? 7.0f : slide_next ? 1.15f : 0.7f));
    while (n > 50)
        n -= 12;
    while (n < 31)
        n += 12;
    if (f & F_OCT)
        n += 12;
    bass_play(n, f, len);
}

static void gtr_note(int deg, uint8_t f)
{
    int n = 36 + root + en_degree(pool, chord_deg + deg);
    while (n >= 48)
        n -= 12;
    while (n < 28)
        n += 12;
    gtr_play(n, f, (f & F_ACC) ? 1.0f : 0.8f);
}

static void syn_note(int deg, uint8_t f, int s)
{
    const int d = chord_deg + deg, base = 48 + root;
    const float vel = (f & F_ACC) ? 0.95f : 0.7f;
    if (f & F_LEAD) {                            /* a line: one voice, an octave up */
        int n = base + 12 + en_degree(pool, d);
        while (n > 79)
            n -= 12;
        syn_voice(n, vel, 0.25f * (float)(s & 2) - 0.25f, 0, 0);
    } else {                                     /* a stab: the chord on the degree (stacked thirds) */
        int n0 = base + en_degree(pool, d), k;
        while (n0 >= 60 + root % 12 && n0 > 57)
            n0 -= 12;
        for (k = 0; k < 3; k++)
            syn_voice(n0 + en_degree(pool, d + 2 * k) - en_degree(pool, d), vel, (float)(k - 1) * 0.55f, k * 40,
                      (f & F_LONG) != 0);
    }
    en_glow[EN_ROW_SYNTH] = 1.0f;
    en_hits[EN_ROW_SYNTH]++;
}

static void play_step(int s)
{
    const step_t *e = &bar[s];
    if (on(LY_DRUMS)) {
        if (e->kick)
            kick_fire((float)e->kick * (1.0f / 127.0f), e->boom, s < EN_NSTEP - 1 && bar[s + 1].kick);
        if (e->snare) {
            snare_fire((float)e->snare * (1.0f / 127.0f));
            if (e->roll)
                bar[s].roll = 2;                 /* the 32nd: half a step on (render) */
        }
        if (e->tom)
            tom_fire((float)e->tom * (1.0f / 127.0f), e->tom_kind > 2 ? 2 : e->tom_kind);
        if (e->hat)
            hat_fire(e->hat_kind > 2 ? 2 : e->hat_kind, (float)e->hat * (1.0f / 127.0f));
    }
    if (e->bass.n != REST && on(LY_BASS))
        bass_note(e->bass.n, e->bass.f, s < EN_NSTEP - 1 && bar[s + 1].bass.n != REST && (bar[s + 1].bass.f & F_SLIDE));
    if (e->gtr.n != REST && on(LY_GUITAR))
        gtr_note(e->gtr.n, e->gtr.f);
    if (e->syn.n != REST && on(LY_SYNTH))
        syn_note(e->syn.n, e->syn.f, s);
    if (e->metal && on(LY_METAL)) {
        if (e->metal & 1u)
            metal_hit(0, 0.6f + 0.3f * frand());
        if (e->metal & 2u)
            metal_hit(1, 0.5f + 0.4f * frand());
    }
}

/* the editor's sounds, now (the bar's chord while playing, else the root's) */
static void audition(int l, int what, uint8_t f)
{
    const int keep = chord_deg;
    if (!en_playing)
        chord_deg = 0;
    switch (l) {
    case LY_DRUMS:
        if (what == EN_HIT_KICK)
            kick_fire(0.85f, 0, 0);
        else if (what == EN_HIT_SNARE)
            snare_fire(0.8f);
        else if (what >= EN_HIT_HAT && what <= EN_HIT_CRASH)
            hat_fire(what - EN_HIT_HAT, 0.8f);
        else if (what >= EN_HIT_TOM_HIGH && what <= EN_HIT_TOM_LOW)
            tom_fire(0.85f, what - EN_HIT_TOM_HIGH);
        break;
    case LY_BASS: bass_note(what, f, 0); break;
    case LY_GUITAR: gtr_note(what, f); break;
    case LY_SYNTH: syn_note(what, f, 0); break;
    case LY_METAL: metal_hit(what & 1, 0.75f); break;
    default: break;
    }
    chord_deg = keep;
}

static void pending_publish(void)
{
    en_pending = (uint8_t)((pend_kind == 1 ? EN_PEND_NEXT : 0) | (pend_kind == 2 ? EN_PEND_BREAK : 0) |
                           (pend_fill || fill_on ? EN_PEND_FILL : 0) | (pend_mutate || pend_top ? EN_PEND_MUTATE : 0) |
                           (root_pend >= 0 ? EN_PEND_ROOT : 0));
}

/* OCT+: on auto, new everything; locked, new lines for the part playing */
static void mutate(void)
{
    if (en_song.locked)
        gen(W_LINES, 1 << part);
    else
        gen(W_ALL, 7);
}

/* a knob that writes the song was turned (en_turned): what it writes */
static int turned_writes(int p)
{
    switch (p) {
    case P_GROOVE: return W_DRUMS | (en_song.locked ? 0 : W_BASS | W_GTR);
    case P_RIFF: return W_GTR;
    case P_SBUSY: return W_SYN;
    case P_HITS: return W_HITS;
    default: return 0;
    }
}
static void write_turned(int w)                 /* the verse's and the chorus's; the synth's breakdown too */
{
    gen(w & ~W_SYN, 3);
    gen(w & W_SYN, 7);
}

/* Intensity: on auto the plan's layers are dealt again; locked, they stand */
static void repick(void)
{
    int i, n = plan_count();
    if (!en_song.locked)
        for (i = 0; i < n; i++)
            en_song.plan[i].layers = pick_layers(plan_type(i), 1);
    sect_resolve(0);
}

/* en_song changed under the render (the player's edits): the section playing follows its entry, and the
 * bar is built again (its fill too, but a fill waiting for the next bar keeps waiting) */
static void rebuild(void)
{
    const uint8_t pf = pend_fill;
    const int n = plan_count();
    if (plan_pos >= n)
        plan_pos = n - 1;
    if (!jumped && plan_type(plan_pos) != sect) {
        sect = plan_type(plan_pos);
        en_sect = (uint8_t)sect;
    }
    part = en_part_of(sect);
    en_part = (uint8_t)part;
    sect_resolve(1);
    if (sect_len && sect_bar >= sect_len)
        sect_bar = sect_len - 1;
    en_sect_bar = (uint8_t)sect_bar;
    metal_tune();
    plan_publish();
    pend_fill = 0;
    bar_build();
    pend_fill = pf;
}

/* a new song in en_song (another piece, or auto again): on auto the machine writes it first; from the
 * top, at the bar while playing */
static void loaded(void)
{
    if (!en_song.locked) {
        gen(W_ALL, 7);
        plan_song();
    }
    metal_tune();
    if (en_playing) {
        pend_top = 1;
        plan_publish();
    } else {
        song_top();
        bar_build();
    }
}

/* the grid: one call a sixteenth (MIDI clock: 6 ticks) */
static void bar_start(void)
{
    const int top = pend_top;
    if (top) {                                   /* a new song: from its top (a jump asked for since comes after) */
        song_top();
        pend_top = 0;
    }
    if (!bar_fresh) {
        if (!top)
            sect_bar++;
        if (pend_kind) {
            if (pend_kind == 2)
                sect_break();
            else
                sect_next();
            pend_kind = 0;
        } else if (!top && sect_len && sect_bar >= sect_len) {
            sect_next();
        } else if (!top && (sect_bar & 3) == 0 && !en_song.locked) {
            evolve_step();
        }
    }
    bar_fresh = 0;
    if (pend_mutate) {
        mutate();
        pend_mutate = 0;
    }
    if (pend_write) {
        write_turned(pend_write);
        pend_write = 0;
    }
    pend_rebuild = 0;
    if (pend_act) {
        repick();
        pend_act = 0;
    }
    if (root_pend >= 0) {
        root = root_pend;
        root_pend = -1;
        en_root_now = (uint8_t)root;
    }
    en_sect_bar = (uint8_t)sect_bar;
    bar_build();
    pending_publish();
}

static int roll_left;                           /* samples to a roll's 32nd (0: none) */
static void tick(void)
{
    int s = (int)(sixteenth & 15u);
    if (s == 0)
        bar_start();
    play_step(s);
    if (bar[s].roll == 2)
        roll_left = (int)(step_len * 0.5f);
    en_step = (uint8_t)s;
    sixteenth++;
    en_bars = sixteenth;
}

/* ------------------------------------------------------------ parameters --- */
static int pulse;
static uint32_t ext_age;

static void tempo_set(void) { step_len = EN_SR * 60.0f / ((float)par[P_TEMPO] * 4.0f); }
static void join(int l);

static void apply(int p, int v)
{
    const en_param_t *pi = &PARAMS[p];
    if (v < pi->lo)
        v = pi->lo;
    if (v > pi->hi)
        v = pi->hi;
    par[p] = (int16_t)v;
    switch (p) {
    case P_DRUMS:
        if (drum_vol <= 0.0f && v > 0 && en_playing)
            join(LY_DRUMS);
        drum_vol = (float)(v * v) * 0.0001f;
        break;
    case P_BASS: case P_GUITAR: case P_SYNTH: case P_METAL: case P_NOISE: {
        int l;
        for (l = 0; l < EN_NLAYER; l++)
            if (EN_LAYER_PARAM[l] == p) {
                if (vol[l] <= 0.0f && v > 0 && en_playing)
                    join(l);                     /* turned up from nothing: it joins the section */
                vol[l] = (float)(v * v) * 0.0001f;
            }
        break;
    }
    case P_TEMPO:
        tempo_set();
        delay_time();
        break;
    case P_ARRANGE: {
        const en_entry_t *e = sect_entry();
        sect_len = e ? entry_len(e) : en_sect_bars(sect, v);
        en_sect_len = (uint8_t)sect_len;
        plan_publish();
        break;
    }
    case P_EVOLVE: evolve = (float)v * 0.01f; break;
    case P_INTENSITY:
        intensity = (float)v * 0.01f;
        pend_act = 1;
        break;
    case P_CRUSH:
        crush_amt = (float)v * 0.01f;
        crush_hold = 1 + (int)(crush_amt * crush_amt * 11.0f);
        crush_q = fm_exp2f(15.0f - 9.0f * crush_amt);
        crush_iq = 1.0f / crush_q;
        break;
    case P_KICK:
        kick_g = 1.0f + 6.0f * (float)v * 0.01f;
        kick_inv = 1.0f / sat(kick_g);
        kick_t60 = 0.28f + 0.4f * (float)v * 0.01f;
        break;
    case P_CUTOFF: bass_cut = 55.0f * fm_exp2f((float)v * 0.055f); break;      /* 55 Hz .. 2.5 kHz */
    case P_SQUELCH:
        bass_env = 1.0f + 4.5f * (float)v * 0.01f;                         /* octaves the envelope opens */
        bass_k = 1.7f - 1.45f * (float)v * 0.01f;                          /* and the resonance */
        bass_fdec = fm_expf(-16.0f * 6.9f / ((0.06f + 0.35f * (float)v * 0.01f) * EN_SR));
        break;
    case P_BDRIVE:
        bass_drv = 1.0f + 9.0f * (float)v * 0.01f;
        bass_dinv = 1.0f / sat(bass_drv);
        break;
    case P_GAIN: gtr_gain = 3.0f + 60.0f * (float)(v * v) * 0.0001f; break;
    case P_CHUG: gtr_chug = (float)v * 0.01f; break;
    case P_STONE: syn_tone = (float)v * 0.01f; break;
    case P_SDECAY: syn_decay = (float)v * 0.01f; break;
    case P_TEXTURE:
        if (tex_knob)
            tex_goal = (float)v * 0.01f;
        break;
    case P_REVERB: rev_send = (float)(v * v) * 0.0001f; break;
    case P_GATE: gate_amt = (float)v * 0.01f; break;
    case P_DELAY: dly_send = (float)(v * v) * 0.0001f; break;
    case P_PUMP: pump_amt = (float)v * 0.007f; break;
    case P_TUNE: tunefac = fm_exp2f((float)v * (1.0f / 1200.0f)); break;
    case P_ROOT:
        if (en_playing) {
            root_pend = v;                       /* on the next bar */
        } else {
            root = v;
            root_pend = -1;
            en_root_now = (uint8_t)v;
            en_chord_root = (uint8_t)((root + en_degree(pool, chord_deg) + 12) % 12);
        }
        break;
    case P_MODE: pool = EN_MODE_POOL[v]; break;
    case P_MIDI:
        if (!v)
            en_midi_out(0xB0, 123, 0);
        break;
    default: break;
    }
}

static void join(int l)
{
    act |= (uint8_t)(1u << l);
    en_active = act;
    if (l == LY_NOISE)
        tex.tgt = SECT_NOISE[sect];
    pend_rebuild = 1;
}

static void notes_off(void)
{
    int i;
    for (i = 0; i < 3; i++)
        en_midi_out(0xB0 | (uint32_t)i, 123, 0);
    en_midi_out(0xB9, 123, 0);
}

static void quiet(void)
{
    int i;
    kick.on = tom.on = snare.on = hat_c.on = hat_o.on = 0;
    bass.on = 0;
    bass.amp = 0.0f;
    bass.gate = 0;
    for (i = 0; i < 2; i++)
        gtr[i].on = 0, gtr[i].wait = 0;
    for (i = 0; i < NSYN; i++)
        syn[i].on = 0, syn[i].wait = 0;
    for (i = 0; i < NMETAL; i++)
        metal[i].on = 0;
    tex.lvl = tex.tgt = tex.rise = tex.rise_t = 0.0f;
    roll_left = 0;
    notes_off();
}

void en_init(void)
{
    int i;
    for (i = 0; i <= SN; i++)
        sint[i] = fm_sinf((float)i * (FM_TWO_PI / (float)SN));
    plate_init();
    for (i = 0; i < DLY_N; i++)
        dly_l[i] = dly_r[i] = 0;
    dly_w = dly_ph = 0;
    dly_len = 0.0f;
    q_r = q_w = 0;
    for (i = 0; i < NMOD; i++) {
        mod[i].v = mod[i].tgt = 0.0f;
        mod[i].left = 0;
    }
    svf_coefs(gtr_cab, 4300.0f, 0.75f);
    for (i = 0; i < EN_NLAYER; i++)
        mg[i] = 1.0f;
    muted = 0;
    en_muted = 0;
    root_pend = -1;
    for (i = 0; i < P_NPARAMS; i++)
        apply(i, PARAMS[i].def);
    en_song_defaults(&en_song);
    gen(W_ALL, 7);
    plan_song();
    pend_kind = 0;
    pend_fill = pend_mutate = pend_write = pend_act = pend_top = 0;
    sect = S_INTRO;
    song_top();
    tex_t = tex_goal;
    sixteenth = 0;
    bar_fresh = 1;
    bar_build();
    pl_decay = pl_decay_t;
    en_root_now = (uint8_t)root;
}

/* ------------------------------------------------------------ transport --- */
static void clock_pulse(void)
{
    if (pulse == 0)
        tick();
    if (!en_ext)
        midi(0xF8, 0, 0);
    if (++pulse >= 6)
        pulse = 0;
}

static void play_start(int from_top)
{
    pulse = 0;
    sixteenth = 0;
    bar_fresh = 1;
    if (from_top) {
        pend_top = 0;
        song_top();
    } else {
        sect_bar = 0;
        en_sect_bar = 0;
    }
    if (root_pend >= 0) {
        root = root_pend;
        root_pend = -1;
        en_root_now = (uint8_t)root;
    }
    tex.tgt = (act >> LY_NOISE & 1u) ? SECT_NOISE[sect] : 0.0f;
    pulse_left = 0.0f;
    en_playing = 1;
    if (!en_ext)
        midi(from_top ? 0xFA : 0xFB, 0, 0);
}

static void play_stop(void)
{
    en_playing = 0;
    roll_left = 0;
    pend_kind = 0;
    pend_fill = 0;
    bass.gate = 0;
    tex.tgt = 0.0f;
    tex.rise_t = 0.0f;
    if (!en_ext)
        midi(0xFC, 0, 0);
    notes_off();
}

static void drain(void)
{
    while (q_r != q_w) {
        uint32_t c = q[q_r % QN];
        int a = (int)(c >> 16 & 0xFFu), b = (int)(int16_t)(c & 0xFFFFu);
        BARRIER();
        q_r++;
        switch (c >> 24) {
        case C_SET:
            if (a < P_NPARAMS)
                apply(a, b);
            break;
        case C_PLAY:
            if (a && !en_playing)
                play_start(0);
            else if (!a && en_playing)
                play_stop();
            break;
        case C_NEXT:
        case C_BREAK:
            if (en_playing) {
                pend_kind = (c >> 24) == C_NEXT ? 1 : 2;
                fill_now(1);
            } else {
                if ((c >> 24) == C_NEXT)
                    sect_next();
                else
                    sect_break();
                bar_build();
            }
            break;
        case C_FILL: fill_now(0); break;
        case C_MUTATE:
            if (en_playing) {
                pend_mutate = 1;
            } else {
                mutate();
                bar_build();
            }
            break;
        case C_MUTE:
            if (a < EN_NLAYER) {
                if (b)
                    muted |= (uint8_t)(1u << a);
                else
                    muted &= (uint8_t)~(1u << a);
                en_muted = muted;
            }
            break;
        case C_SEED:
            rng ^= (uint32_t)(b & 0xFFFF) * 2654435761u;
            if (!rng)
                rng = 1u;
            break;
        case C_CLOCK:
            en_ext = 1;
            ext_age = 0;
            if (a == EN_CLK_TICK) {
                if (en_playing)
                    clock_pulse();
            } else if (a == EN_CLK_START) {
                play_start(1);
            } else if (a == EN_CLK_CONTINUE) {
                if (!en_playing)
                    play_start(0);
            } else if (en_playing) {
                play_stop();
            }
            break;
        case C_PANIC: quiet(); break;
        case C_LOADED: loaded(); break;
        case C_REBUILD: rebuild(); break;
        case C_TURNED:
            if (en_playing) {
                pend_write |= (uint8_t)turned_writes(a);
            } else {
                write_turned(turned_writes(a));
                bar_build();
            }
            break;
        case C_AUDITION: audition(a, (int8_t)(b & 0xFF), (uint8_t)(b >> 8 & 0xFF)); break;
        }
    }
    pending_publish();
}

/* --------------------------------------------------------------- render --- */
static void render_chunk(int32_t *out, uint32_t n, float gain)
{
    uint32_t j;
    const float rg = rev_send, dg = dly_send, ga = gate_amt, pa = pump_amt;
    for (j = 0; j < n; j++)
        mixl[j] = mixr[j] = drum[j] = rsend[j] = dsend[j] = 0.0f;
    prof(PR_OTHER);
    render_drums(n);
    prof(PR_DRUMS);
    render_bass(n);
    prof(PR_BASS);
    render_guitar(n);
    prof(PR_GUITAR);
    render_synth(n);
    prof(PR_SYNTH);
    render_metal(n);
    prof(PR_METAL);
    render_noise(n);
    prof(PR_NOISE);
    for (j = 0; j < n; j++) {
        float wl, wr, l, r, el, er, duck, rgate;
        if (gate_hold > 0)
            gate_hold--;
        else
            gate_tgt = since_snare > (int32_t)(16.0f * step_len) ? 1.0f : 0.0f;
        gate_env += (gate_tgt - gate_env) * 0.004f;
        rgate = 1.0f - ga + ga * gate_env;
        pump_tgt *= pump_rel;
        pump_env += (pump_tgt - pump_env) * 0.05f;
        duck = 1.0f - pa * pump_env;
        delays(dsend[j] * dg, &el, &er);
        reverb((rsend[j] + (el + er) * 0.25f) * rg, &wl, &wr);
        l = drum[j] + (mixl[j] + 0.8f * el + 1.1f * wl * rgate) * duck;
        r = drum[j] + (mixr[j] + 0.8f * er + 1.1f * wr * rgate) * duck;
        out[2 * j] = (int32_t)(fm_tanhf(l * 0.5f * gain) * 8300000.0f);
        out[2 * j + 1] = (int32_t)(fm_tanhf(r * 0.5f * gain) * 8300000.0f);
    }
    since_snare += (int32_t)n;
    prof(PR_SPACE);
}

static void control(uint32_t n)                 /* once a chunk: the slow things */
{
    int i;
    float pos;
    mods_step(n);
    for (i = 0; i < EN_NLAYER; i++)              /* the mutes fade in ~20 ms */
        mg[i] += ((muted >> i & 1u ? 0.0f : 1.0f) - mg[i]) * fm_minf((float)n * (1.0f / (0.02f * EN_SR)), 1.0f);
    /* where in the section: the intro opens, the build rises (and its riser), the breakdown darkens */
    pos = ((float)sect_bar * 16.0f + (float)(sixteenth & 15u)) / ((float)(sect_len ? sect_len : 4) * 16.0f);
    pos = fm_clampf(pos, 0.0f, 1.0f);
    {
        float sw = 0.0f;
        if (sect == S_INTRO)
            sw = -2.5f * (1.0f - pos);
        else if (sect == S_BUILD)
            sw = -2.0f * (1.0f - pos);
        else if (sect == S_BREAK)
            sw = -1.2f;
        if (!en_playing)
            sw = 0.0f;
        sweep += (sw - sweep) * fm_minf((float)n * (1.0f / (0.3f * EN_SR)), 1.0f);
    }
    tex.rise_t = (en_playing && sect == S_BUILD && (act >> LY_NOISE & 1u)) ? pos : 0.0f;
    pl_decay += ((sect == S_BREAK ? 0.9f : pl_decay_t) - pl_decay) * 0.002f;
    pl_dd2 = fm_clampf(pl_decay + 0.15f, 0.25f, 0.5f);
    pump_rel = fm_expf(-6.9f / (fm_maxf(step_len * 2.5f, 1000.0f)));
    for (i = 0; i < EN_NROW; i++)
        en_glow[i] *= 0.95f;
}

void en_render(int32_t *out, uint32_t n, uint32_t gain_q12)
{
    const float gain = (float)gain_q12 * (1.0f / 4096.0f) * 2.0f;
    if (en_time)
        prof_t = en_time();
    drain();
    prof(PR_SEQ);
    if (en_ext && ++ext_age > 100u)
        en_ext = 0;
    while (n) {
        uint32_t k = n > BLK ? BLK : n;
        int timed = en_playing && !en_ext;
        if (timed) {
            if (pulse_left <= 0.0f) {
                clock_pulse();
                pulse_left += step_len * (1.0f / 6.0f);
                prof(PR_SEQ);
            }
            if ((float)k > pulse_left)
                k = (uint32_t)pulse_left + 1u;
        }
        if (k > n)
            k = n;
        if (roll_left > 0 && (uint32_t)roll_left < k)
            k = (uint32_t)roll_left;             /* a roll's 32nd lands on its sample */
        if (timed)
            pulse_left -= (float)k;
        control(k);
        render_chunk(out, k, gain);
        if (roll_left > 0 && (roll_left -= (int)k) <= 0) {
            roll_left = 0;
            if (en_playing && on(LY_DRUMS))
                snare_fire(0.75f);
        }
        out += 2 * k;
        n -= k;
    }
}
