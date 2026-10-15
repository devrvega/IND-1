/* SPDX-License-Identifier: GPL-3.0-only */
/* IND-1 UI: the panel and the screen. Part of the unity build (after gfx.c and project.c); the host
 * simulator includes it too.
 *
 *   keys           the root: the machine moves to the key's note on the next bar (MIDI channel 1 too)
 *   PLAY           run / stop                      REC    a breakdown; in one, the build; in the build, the drop
 *   EDIT           the next section (the rest of the bar fills)
 *   EDIT + a layer's page button: mute it          OCT-   a fill        OCT+   mutate
 *   HOME SEQ LFO ENV ARP SEL FX GLO   the pages: Machine, Drums, Bass, Guitar, Synth, Metal, Space, Setup;
 *                  pressed again, the page's editor (the Song, the Drums, the Bass, the Guitar, the Synth,
 *                  the Metal); again, back
 *   SELECT the pages (with HOME held, the tempo); in an editor, the part (A B C), the view or the ring
 *   ALGORITHM the mode     PRESETS the piece      KNOB 1-4  the page's four values
 *   SAVE           saves (it also saves by itself a few seconds after a change, while stopped);
 *                  held 1.5 s: the piece back to its recipe, and the song to the machine
 *
 * In the editors the keys write the song (en_song): the white keys are the sixteen steps (the song
 * editor's: its sixteen sections), the black keys the drum, the note or the layer. A step held: the black
 * keys change it, the knobs are its own. ENV + a step: an accent; EDIT + a step: cleared. An edit locks the
 * song: from then on the machine changes nothing in it by itself. */
#include "../dsp/fastmath.h"

/* the pages; SELECT steps through them in this order */
enum { V_MACHINE, V_DRUMS, V_BASS, V_GUITAR, V_SYNTH, V_METAL, V_SPACE, V_SETUP, NVIEWS };
static const char *const VIEW_NAME[NVIEWS] = {"Machine", "Drums", "Bass", "Guitar", "Synth", "Metal", "Space", "Setup"};
static const uint8_t VIEW_BTN[NVIEWS] = {B_HOME, B_SEQ, B_LFO, B_ENV, B_ARP, B_SEL, B_FX, B_GLO};
/* the layers a page's button mutes (with EDIT held): the Metal page holds the metal and the noise */
static const uint8_t VIEW_MUTE[NVIEWS] = {0, 1u << LY_DRUMS, 1u << LY_BASS, 1u << LY_GUITAR, 1u << LY_SYNTH,
                                          1u << LY_METAL | 1u << LY_NOISE, 0, 0};
/* the layer a page shows under the scene */
static const uint8_t VIEW_LAYER[NVIEWS] = {LY_DRUMS, LY_DRUMS, LY_BASS, LY_GUITAR, LY_SYNTH, LY_METAL, LY_DRUMS, LY_DRUMS};
/* the editors: a page's button pressed again */
enum { E_NONE, E_SONG, E_DRUMS, E_BASS, E_GUITAR, E_SYNTH, E_METAL };
static const uint8_t VIEW_EDIT[NVIEWS] = {E_SONG, E_DRUMS, E_BASS, E_GUITAR, E_SYNTH, E_METAL, E_NONE, E_NONE};

/* the four knobs of each page: a parameter, or one of these; and what the knob is called there */
enum {
    K_LEDS = 100, K_CPU, K_NONE,
    K_SEC_TYPE, K_SEC_BARS, K_SEC_BED, K_SONG,         /* the song editor: the section picked; auto or locked */
    K_CHORD0, K_CHORD1, K_CHORD2, K_CHORD3,            /* its chords: the part's four */
    K_RING_HITS, K_RING_LEN, K_RING_TURN, K_TONE,      /* the metal editor: the ring picked; the clangs */
    K_STEP_NOTE, K_STEP_ACC, K_STEP_F1, K_STEP_F2      /* a bass, guitar or synth step held */
};
static const uint8_t VIEW_KNOB[NVIEWS][4] = {
    {P_TEMPO, P_ARRANGE, P_EVOLVE, P_INTENSITY},
    {P_DRUMS, P_GROOVE, P_CRUSH, P_KICK},
    {P_BASS, P_CUTOFF, P_SQUELCH, P_BDRIVE},
    {P_GUITAR, P_GAIN, P_CHUG, P_RIFF},
    {P_SYNTH, P_STONE, P_SDECAY, P_SBUSY},
    {P_METAL, P_HITS, P_NOISE, P_TEXTURE},
    {P_REVERB, P_GATE, P_DELAY, P_PUMP},
    {P_ROOT, P_TUNE, P_MIDI, K_LEDS},
};
static const char *const KNOB_LABEL[NVIEWS][4] = {
    {"Tempo", "Arrange", "Evolve", "Intensity"},
    {"Level", "Groove", "Crush", "Kick"},
    {"Level", "Cutoff", "Squelch", "Drive"},
    {"Level", "Gain", "Chug", "Riff"},
    {"Level", "Tone", "Decay", "Busy"},
    {"Metal", "Hits", "Noise", "Texture"},
    {"Reverb", "Gate", "Delay", "Pump"},
    {"Root", "Tune", "MIDI out", "Lights"},
};

/* the screen: a dark LCD (near-black glass, ember segments and their ghosts) over the case, which holds
 * the layers and the knobs. Each layer its own colour on the case (bone drums, rust bass, red guitar, cold
 * synth, steel metal, a toxic green noise) */
#define K_BODY RGB(17, 16, 18)
#define K_PANEL RGB(38, 35, 38)
#define K_LINE RGB(60, 56, 60)
#define K_DIM RGB(122, 116, 110)
#define K_TEXT RGB(234, 228, 216)
#define K_ACC RGB(255, 132, 36)          /* ember: the page, the knob being turned */
#define K_RED RGB(228, 44, 36)           /* muted */
#define L_BG RGB(12, 9, 8)               /* the glass */
#define L_EDGE RGB(60, 45, 36)
#define L_GHOST RGB(40, 24, 15)          /* a segment, unlit */
#define L_PRINT RGB(84, 47, 27)          /* the scenery, printed on the glass */
#define L_MID RGB(168, 80, 28)
#define L_INK RGB(255, 134, 40)          /* a segment, lit */
#define L_HOT RGB(255, 224, 156)         /* white-hot: an accent, the furnace */
static const uint16_t LAYER_C[EN_NLAYER] = {RGB(226, 220, 204), RGB(255, 112, 28), RGB(228, 44, 36),
                                            RGB(70, 190, 232), RGB(160, 166, 178), RGB(146, 178, 64)};
static const char *const LAYER_SHORT[EN_NLAYER] = {"DRM", "BAS", "GTR", "SYN", "MTL", "NOI"};

/* the keys: the white ones are steps, the black ones drums, notes or layers (in the editors) */
static const uint8_t WHITE_KEY[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t BLACK_KEY[11] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};
static const int8_t KEY_DEG[11] = {0, 1, 2, 3, 4, 5, 6, 7, -1, -2, -3};   /* the black keys' notes: scale degrees */
#define DEG_LO (-3)
#define DEG_HI 7

/* the drums editor's rows (the black keys), in the engine's audition order (EN_HIT_*) */
enum { R_KICK, R_SNARE, R_HAT, R_OPEN, R_CRASH, R_TOMH, R_TOMM, R_TOML, NDROW };
static const char *const DROW_NAME[NDROW] = {"KICK", "SNARE", "HAT", "OPEN", "CRASH", "TOM H", "TOM M", "TOM L"};

static struct {
    uint8_t view, edit;                /* the page; its editor open */
    uint8_t part;                      /* the editors' part: 0 A, 1 B, 2 C */
    uint8_t row;                       /* the drums editor's row */
    int8_t note;                       /* the bass, guitar and synth editors' note: a scale degree */
    uint8_t sec;                       /* the song editor's section */
    uint8_t chords;                    /* the song editor: 0 its sections, 1..3 the chords of part A..C */
    uint8_t ring;                      /* the metal editor's ring: 0 the anvil, 1 the scrap */
    int8_t held;                       /* the step held in an editor (its white key), -1 none */
    uint32_t btn, keys, btn_used, keys_used;
    uint32_t enc_t[NE];
    uint8_t enc_run[NE];               /* detents in the current quick run, per knob (accel) */
    uint32_t down_t[NB];               /* when each button went down */
    uint32_t turn_t;                   /* the last knob turn */
    uint8_t help;                      /* a button's card is up: the button + 1 */
    uint8_t reset_done;                /* SAVE held: the piece went back to its recipe this hold */
    uint8_t dirty;                     /* the project changed since the last save */
    uint32_t act_t, saved_t;
    char msg[2][24];
    uint32_t msg_until;
    int8_t touched;                    /* the knob turned last (0..3), shown bright for a moment */
    uint32_t touch_until;
    uint32_t frame;
    uint8_t seen[EN_NROW];             /* the rows' hits as last seen; when each changed, how often */
    uint32_t hit_t[EN_NROW];
    uint8_t cnt[EN_NROW];
    uint32_t sig[4];                   /* what each band shows: redrawn when it changes */
} ui;

static void say(const char *a, const char *b)
{
    uint32_t i;
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
}
void ui_say(const char *a, const char *b) { say(a, b); }

static void itoa_u(uint32_t v, char *b)
{
    char t[12];
    int n = 0;
    do {
        t[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    while (n)
        *b++ = t[--n];
    *b = 0;
}

static void caps(char *d, const char *s, int n)  /* s in capitals, at most n - 1 characters */
{
    int i;
    for (i = 0; i < n - 1 && s[i]; i++)
        d[i] = (char)(s[i] >= 'a' && s[i] <= 'z' ? s[i] - 32 : s[i]);
    d[i] = 0;
}

static void scopy(char *d, const char *s, int n)
{
    int i;
    for (i = 0; i < n - 1 && s[i]; i++)
        d[i] = s[i];
    d[i] = 0;
}

static void mark_dirty(void) { ui.dirty = 1; }
int ui_dirty(void) { return ui.dirty != 0; }

/* ------------------------------------------------------------ the song --- */
static int editor(void) { return ui.edit ? VIEW_EDIT[ui.view] : E_NONE; }
static int line_editor(void)
{
    int e = editor();
    return e == E_BASS || e == E_GUITAR || e == E_SYNTH;
}
static int step_editor(void) { return editor() == E_DRUMS || line_editor(); }
static int ed_layer(void)
{
    static const uint8_t L[7] = {0, 0, LY_DRUMS, LY_BASS, LY_GUITAR, LY_SYNTH, LY_METAL};
    return L[editor()];
}
static en_part_t *epart(void) { return &en_song.part[ui.part % EN_NPARTS]; }
static en_note_t *eline(void)
{
    en_part_t *p = epart();
    return editor() == E_BASS ? p->bass : editor() == E_GUITAR ? p->gtr : p->syn;
}
static int plan_n(void) { return en_song.plan_n < 1 ? 1 : en_song.plan_n > EN_PLAN_MAX ? EN_PLAN_MAX : en_song.plan_n; }
static en_entry_t *esec(void)
{
    if (ui.sec >= plan_n())
        ui.sec = (uint8_t)(plan_n() - 1);
    return &en_song.plan[ui.sec];
}

/* about to change the song: from now on it is the player's (locked) */
static void song_w(void)
{
    if (!en_song.locked) {
        en_lock(1);
        say("LOCKED", "THE SONG IS YOURS");
    }
}
static void song_done(void)
{
    en_rebuild();
    mark_dirty();
}

/* the parts a part plays in, as words */
static const char *part_use(int p)
{
    static const char *const U[EN_NPARTS] = {"VERSE INTRO BUILD OUTRO", "CHORUS DROP", "BREAKDOWN"};
    return U[p % EN_NPARTS];
}

/* a scale degree's note, from the root in the mode: "E", "F#'" (an octave up), "B," (below) */
static void deg_name(int d, char *b)
{
    int st = en_degree(EN_MODE_POOL[proj.par[P_MODE] % EN_NMODES], d), pc = ((proj.par[P_ROOT] + st) % 12 + 12) % 12, i = 0;
    const char *n = EN_NOTE_NAME[pc];
    while (*n)
        b[i++] = *n++;
    if (st < 0)
        b[i++] = ',';
    for (; st >= 12 && i < 5; st -= 12)
        b[i++] = '\'';
    b[i] = 0;
}

static int drum_get(const en_part_t *p, int r, int s)
{
    switch (r) {
    case R_KICK: return p->kick[s];
    case R_SNARE: return p->snare[s];
    case R_HAT: case R_OPEN: case R_CRASH: return p->hat_kind[s] == r - R_HAT ? p->hat[s] : 0;
    default: return p->tom_kind[s] == r - R_TOMH ? p->tom[s] : 0;
    }
}
static void drum_set(en_part_t *p, int r, int s, int v)
{
    switch (r) {
    case R_KICK: p->kick[s] = (uint8_t)v; break;
    case R_SNARE: p->snare[s] = (uint8_t)v; break;
    case R_HAT: case R_OPEN: case R_CRASH:
        if (v) {
            p->hat[s] = (uint8_t)v;
            p->hat_kind[s] = (uint8_t)(r - R_HAT);
        } else if (p->hat_kind[s] == r - R_HAT) {
            p->hat[s] = 0;
        }
        break;
    default:
        if (v) {
            p->tom[s] = (uint8_t)v;
            p->tom_kind[s] = (uint8_t)(r - R_TOMH);
        } else if (p->tom_kind[s] == r - R_TOMH) {
            p->tom[s] = 0;
        }
        break;
    }
}

/* ------------------------------------------------------------ values --- */
static const uint8_t SONG_KNOB[4] = {K_SEC_TYPE, K_SEC_BARS, K_SEC_BED, K_SONG};
static const uint8_t METAL_KNOB[4] = {K_RING_HITS, K_RING_LEN, K_RING_TURN, K_TONE};
static const char *const BED_NAME[EN_NTEX] = {"Texture", "Drone", "Static", "Both"};

/* what knob i is now: the page's, an editor's, a step's */
static int knob_id(int i)
{
    switch (editor()) {
    case E_SONG: return ui.chords ? K_CHORD0 + i : SONG_KNOB[i];
    case E_METAL: return METAL_KNOB[i];
    case E_BASS: case E_GUITAR: case E_SYNTH:
        if (ui.held >= 0)
            return (editor() == E_GUITAR && i == 3) ? K_NONE : K_STEP_NOTE + i;
        break;
    default: break;
    }
    return VIEW_KNOB[ui.view][i];
}
static const char *knob_label(int i)
{
    static const char *const SONG_L[4] = {"Section", "Bars", "Bed", "Song"};
    static const char *const CHORD_L[4] = {"Chord 1", "Chord 2", "Chord 3", "Chord 4"};
    static const char *const METAL_L[4] = {"Hits", "Steps", "Turn", "Tone"};
    int k = knob_id(i);
    if (k >= K_SEC_TYPE && k <= K_SONG)
        return SONG_L[i];
    if (k >= K_CHORD0 && k <= K_CHORD3)
        return CHORD_L[i];
    if (k >= K_RING_HITS && k <= K_TONE)
        return METAL_L[i];
    switch (k) {
    case K_STEP_NOTE: return "Note";
    case K_STEP_ACC: return "Accent";
    case K_STEP_F1: return editor() == E_BASS ? "Tie" : editor() == E_GUITAR ? "Chord" : "Held";
    case K_STEP_F2: return editor() == E_BASS ? "Octave" : "Line";
    default: return KNOB_LABEL[ui.view][i];
    }
}

static int step_f1(const en_note_t *n)
{
    if (editor() == E_BASS)
        return (n->f & EN_F_SLIDE) ? 1 : (n->f & EN_F_LONG) ? 2 : 0;
    return editor() == E_GUITAR ? (n->f & EN_F_OPEN) != 0 : (n->f & EN_F_LONG) != 0;
}

static int knob_get(int k)
{
    if (k < P_NPARAMS)
        return proj.par[k];
    switch (k) {
    case K_LEDS: return proj.leds;
    case K_CPU: return (int)plat_cpu_pct();
    case K_SEC_TYPE: return esec()->type;
    case K_SEC_BARS: return esec()->bars;
    case K_SEC_BED: return esec()->tex;
    case K_SONG: return en_song.locked;
    case K_CHORD0: case K_CHORD1: case K_CHORD2: case K_CHORD3:
        return en_song.part[(ui.chords + 2) % 3].prog[k - K_CHORD0];
    case K_RING_HITS: return en_song.ring_hits[ui.ring & 1];
    case K_RING_LEN: return en_song.ring_len[ui.ring & 1];
    case K_RING_TURN: return en_song.ring_turn[ui.ring & 1];
    case K_TONE: return en_song.metal_tone;
    default: break;
    }
    if (k >= K_STEP_NOTE && k <= K_STEP_F2 && ui.held >= 0) {
        const en_note_t *n = &eline()[ui.held];
        if (k == K_STEP_NOTE)
            return n->n == EN_REST ? -8 : n->n;
        if (n->n == EN_REST)
            return 0;
        if (k == K_STEP_ACC)
            return (n->f & EN_F_ACC) != 0;
        if (k == K_STEP_F1)
            return step_f1(n);
        return (n->f & (editor() == E_BASS ? EN_F_OCT : EN_F_LEAD)) != 0;
    }
    return 0;
}
static void knob_range(int k, int *lo, int *hi)
{
    *lo = 0;
    if (k < P_NPARAMS) {
        *lo = en_param_info(k)->lo;
        *hi = en_param_info(k)->hi;
        return;
    }
    switch (k) {
    case K_LEDS: *hi = 2; break;
    case K_CPU: *hi = 100; break;
    case K_SEC_TYPE: *hi = EN_NSECT - 1; break;
    case K_SEC_BARS: *hi = EN_MAX_BARS; break;
    case K_SEC_BED: *hi = EN_NTEX - 1; break;
    case K_SONG: *hi = 1; break;
    case K_CHORD0: case K_CHORD1: case K_CHORD2: case K_CHORD3:
        *lo = -7;
        *hi = 7;
        break;
    case K_RING_HITS: *hi = en_song.ring_len[ui.ring & 1]; break;
    case K_RING_LEN:
        *lo = 1;
        *hi = EN_NSTEP;
        break;
    case K_RING_TURN: *hi = en_song.ring_len[ui.ring & 1] - 1; break;
    case K_TONE: *hi = 255; break;
    case K_STEP_NOTE:
        *lo = -8;
        *hi = 14;
        break;
    case K_STEP_F1: *hi = editor() == E_BASS ? 2 : 1; break;
    case K_STEP_ACC: case K_STEP_F2: *hi = 1; break;
    default: *hi = 0; break;
    }
}
static void knob_text(int k, char *b)
{
    int v = knob_get(k);
    const char *s = 0;
    if (k < P_NPARAMS) {
        en_param_text(k, v, b);
        return;
    }
    switch (k) {
    case K_LEDS: s = v == 2 ? "Keys" : v ? "On" : "Off"; break;   /* On: the keys, and the unlit buttons glow */
    case K_CPU:
        itoa_u((uint32_t)v, b);
        while (*b)
            b++;
        *b++ = '%';
        *b = 0;
        return;
    case K_SEC_TYPE: s = EN_SECT_NAME[v % EN_NSECT]; break;
    case K_SEC_BARS:
        if (!v)
            s = "Arrange";
        else
            itoa_u((uint32_t)v, b);
        break;
    case K_SEC_BED: s = BED_NAME[v % EN_NTEX]; break;
    case K_SONG: s = v ? "Locked" : "Auto"; break;
    case K_CHORD0: case K_CHORD1: case K_CHORD2: case K_CHORD3: case K_STEP_NOTE:
        if (k == K_STEP_NOTE && v == -8)
            s = "Rest";
        else
            deg_name(v, b);
        break;
    case K_STEP_ACC: s = v ? "On" : "Off"; break;
    case K_STEP_F1:
        if (editor() == E_BASS)
            s = v == 1 ? "Slide" : v == 2 ? "Held" : "Off";
        else if (editor() == E_GUITAR)
            s = v ? "Open" : "Chug";
        else
            s = v ? "Held" : "Stab";
        break;
    case K_STEP_F2: s = v ? (editor() == E_BASS ? "Up" : "Line") : (editor() == E_BASS ? "Off" : "Chord"); break;
    case K_RING_HITS: case K_RING_LEN: case K_RING_TURN: case K_TONE: itoa_u((uint32_t)v, b); break;
    default: b[0] = 0; break;
    }
    if (s)
        scopy(b, s, 10);
}

static void step_write(int k, int v)            /* a step's knob, a step held */
{
    en_note_t *n = &eline()[ui.held];
    uint8_t f;
    song_w();
    if (k == K_STEP_NOTE) {
        if (v == -8) {
            n->n = EN_REST;
        } else {
            if (n->n == EN_REST)
                n->f = 0;
            n->n = (int8_t)v;
        }
    } else {
        if (n->n == EN_REST) {                   /* a rest takes the note picked first */
            n->n = ui.note;
            n->f = 0;
        }
        f = n->f;
        if (k == K_STEP_ACC) {
            f = (uint8_t)(v ? f | EN_F_ACC : f & ~EN_F_ACC);
        } else if (k == K_STEP_F1) {
            if (editor() == E_BASS)
                f = (uint8_t)((f & ~(EN_F_SLIDE | EN_F_LONG)) | (v == 1 ? EN_F_SLIDE : v == 2 ? EN_F_LONG : 0));
            else if (editor() == E_GUITAR)
                f = (uint8_t)(v ? f | EN_F_OPEN : f & ~EN_F_OPEN);
            else
                f = (uint8_t)(v ? f | EN_F_LONG : f & ~EN_F_LONG);
        } else {
            uint8_t m = editor() == E_BASS ? EN_F_OCT : EN_F_LEAD;
            f = (uint8_t)(v ? f | m : f & ~m);
        }
        n->f = f;
    }
    song_done();
    ui.keys_used |= 1u << WHITE_KEY[ui.held];
}

static void knob_set(int k, int v)
{
    int lo, hi;
    if (k == K_CPU || k == K_NONE)
        return;
    knob_range(k, &lo, &hi);
    v = v < lo ? lo : v > hi ? hi : v;
    if (v == knob_get(k))
        return;
    if (k < P_NPARAMS) {
        proj.par[k] = (int16_t)v;
        en_set(k, v);
        if (k == P_GROOVE || k == P_RIFF || k == P_SBUSY || k == P_HITS)
            en_turned(k);                        /* these write the song */
        mark_dirty();
        return;
    }
    if (k == K_LEDS) {
        proj.leds = (uint8_t)v;
        mark_dirty();
        return;
    }
    if (k == K_SONG) {
        en_lock(v);
        say(v ? "LOCKED" : "AUTO", v ? "THE SONG IS YOURS" : "THE MACHINE WRITES ON");
        mark_dirty();
        return;
    }
    if (k >= K_STEP_NOTE) {
        step_write(k, v);
        return;
    }
    song_w();
    switch (k) {
    case K_SEC_TYPE:
        esec()->type = (uint8_t)v;
        esec()->layers = en_layers_for(v);       /* a new kind of section: its usual layers */
        break;
    case K_SEC_BARS: esec()->bars = (uint8_t)v; break;
    case K_SEC_BED: esec()->tex = (uint8_t)v; break;
    case K_CHORD0: case K_CHORD1: case K_CHORD2: case K_CHORD3:
        en_song.part[(ui.chords + 2) % 3].prog[k - K_CHORD0] = (int8_t)v;
        break;
    case K_RING_HITS: en_song.ring_hits[ui.ring & 1] = (uint8_t)v; break;
    case K_RING_LEN: {
        int r = ui.ring & 1;
        en_song.ring_len[r] = (uint8_t)v;
        if (en_song.ring_hits[r] > v)
            en_song.ring_hits[r] = (uint8_t)v;
        if (en_song.ring_turn[r] >= v)
            en_song.ring_turn[r] = (uint8_t)(v - 1);
        break;
    }
    case K_RING_TURN: en_song.ring_turn[ui.ring & 1] = (uint8_t)v; break;
    case K_TONE: en_song.metal_tone = (uint8_t)v; break;
    default: break;
    }
    song_done();
}

/* knob acceleration by speed (X0X's): a few clicks are one step each however quick, so small
 * corrections stay exact; a longer quick turn speeds up to 8 a detent, so half a turn sweeps 0-100.
 * Small ranges never accelerate. */
static uint8_t accel_off;              /* the host simulator's "spin": exact steps */
static int32_t accel(int role, int32_t s, int range)
{
    uint32_t now = plat_ms(), dt = now - ui.enc_t[role], a = (uint32_t)(s < 0 ? -s : s), m;
    ui.enc_t[role] = now;
    if (!a)
        return s;
    if (a > 1)
        dt /= a;                                      /* per detent */
    ui.enc_run[role] = (uint8_t)(dt < 60u ? (ui.enc_run[role] + a > 255u ? 255u : ui.enc_run[role] + a) : a);
    if (range <= 24 || accel_off || ui.enc_run[role] < 4u)
        return s;
    m = dt < 12u ? 8u : dt < 25u ? 5u : dt < 45u ? 3u : 1u;
    if (range < 100 && m > 3u)
        m = 3u;
    return s * (int32_t)m;
}

static void turn(int role, int k, int32_t e)
{
    int lo, hi;
    if (!e || k == K_NONE || k == K_CPU)
        return;
    knob_range(k, &lo, &hi);
    knob_set(k, knob_get(k) + accel(role, e, hi - lo));
}

/* ------------------------------------------------------------- the root --- */
static void set_root(int pc)                    /* a key (or MIDI): the machine moves there on the bar */
{
    pc = (pc % 12 + 12) % 12;
    if (pc != proj.par[P_ROOT]) {
        proj.par[P_ROOT] = (int16_t)pc;
        en_set(P_ROOT, pc);
        mark_dirty();
    }
    say("ROOT", EN_NOTE_NAME[pc]);
}

static void set_mode(int m)
{
    m = (m % EN_NMODES + EN_NMODES) % EN_NMODES;
    proj.par[P_MODE] = (int16_t)m;
    en_set(P_MODE, m);
    say("MODE", EN_MODE_NAME[m]);
    mark_dirty();
}

static void set_piece(int i)                    /* the piece playing kept as it is; piece i as it was left */
{
    project_piece(i);
    project_apply();
    en_seed(plat_ms());
    project_song_take();
    ui.held = -1;
    say("PIECE", piece_name(proj.piece));
    mark_dirty();
}

/* ------------------------------------------------------------- the editors --- */
static void editor_open(void)
{
    ui.part = (uint8_t)(en_part % EN_NPARTS);    /* the part playing */
    ui.sec = en_plan_pos < plan_n() ? en_plan_pos : 0;
    ui.held = -1;
}

static int mod_held(int b) { return (ui.btn >> b & 1u) != 0; }
static void mod_used(int b) { ui.btn_used |= 1u << b; }

static void song_key(int w, int b)              /* the song editor: sections on the white keys, layers on the black */
{
    int n = plan_n(), i;
    if (b >= 0) {
        if (b < EN_NLAYER) {
            en_entry_t *e = esec();
            char t[12];
            song_w();
            e->layers ^= (uint8_t)(1u << b);
            song_done();
            caps(t, EN_LAYER_NAME[b], sizeof t);
            say(t, e->layers >> b & 1u ? "PLAYS" : "RESTS");
        }
        return;
    }
    if (mod_held(B_EDIT)) {                      /* EDIT + a section: gone */
        mod_used(B_EDIT);
        if (w < n && n > 1) {
            song_w();
            for (i = w; i < n - 1; i++)
                en_song.plan[i] = en_song.plan[i + 1];
            en_song.plan_n = (uint8_t)(n - 1);
            if (ui.sec >= n - 1)
                ui.sec = (uint8_t)(n - 2);
            song_done();
            say("SECTION", "GONE");
        }
        return;
    }
    if (mod_held(B_ENV)) {                       /* ENV + a section: a copy after it */
        mod_used(B_ENV);
        if (w < n && n < EN_PLAN_MAX) {
            song_w();
            for (i = n; i > w + 1; i--)
                en_song.plan[i] = en_song.plan[i - 1];
            en_song.plan[w + 1] = en_song.plan[w];
            en_song.plan_n = (uint8_t)(n + 1);
            ui.sec = (uint8_t)(w + 1);
            song_done();
            say("SECTION", "COPIED");
        }
        return;
    }
    if (w < n) {
        ui.sec = (uint8_t)w;
    } else if (w == n && n < EN_PLAN_MAX) {      /* the key after the last: another section, as the last */
        song_w();
        en_song.plan[n] = en_song.plan[n - 1];
        en_song.plan_n = (uint8_t)(n + 1);
        ui.sec = (uint8_t)n;
        song_done();
        say("SECTION", "ADDED");
    }
}

static void drums_key(int k, int w, int b)
{
    en_part_t *p = epart();
    int v;
    if (b >= 0) {                                /* a drum: picked, heard; a step held: put there too */
        if (b >= NDROW)
            return;
        ui.row = (uint8_t)b;
        en_audition(LY_DRUMS, b, 0);
        if (ui.held >= 0) {
            song_w();
            drum_set(p, b, ui.held, drum_get(p, b, ui.held) ? 0 : EN_VEL_HIT);
            song_done();
            ui.keys_used |= 1u << WHITE_KEY[ui.held];
        }
        return;
    }
    ui.held = (int8_t)w;
    v = drum_get(p, ui.row, w);
    if (mod_held(B_EDIT)) {                      /* EDIT + a step: every drum off it */
        int r;
        mod_used(B_EDIT);
        song_w();
        for (r = 0; r < NDROW; r++)
            drum_set(p, r, w, 0);
        song_done();
    } else if (mod_held(B_ENV)) {                /* ENV + a step: an accent, a ghost, a hit */
        mod_used(B_ENV);
        song_w();
        drum_set(p, ui.row, w, !v || v == EN_VEL_HIT ? EN_VEL_ACC : v >= EN_VEL_ACC ? EN_VEL_GHOST : EN_VEL_HIT);
        song_done();
    } else if (!v) {
        song_w();
        drum_set(p, ui.row, w, EN_VEL_HIT);
        song_done();
        en_audition(LY_DRUMS, ui.row, 0);
    } else {
        return;                                  /* on: let go, it goes (unless something else happened) */
    }
    ui.keys_used |= 1u << k;
}

static void line_key(int k, int w, int b)
{
    en_note_t *line = eline();
    const int l = ed_layer();
    if (b >= 0) {                                /* a note: picked, heard; a step held: it becomes that */
        ui.note = KEY_DEG[b];
        if (ui.held >= 0) {
            en_note_t *n = &line[ui.held];
            song_w();
            if (n->n == EN_REST)
                n->f = 0;
            n->n = ui.note;
            song_done();
            ui.keys_used |= 1u << WHITE_KEY[ui.held];
            en_audition(l, n->n, n->f);
        } else {
            en_audition(l, ui.note, l == LY_GUITAR ? EN_F_OPEN : 0);
        }
        return;
    }
    ui.held = (int8_t)w;
    if (mod_held(B_EDIT)) {                      /* EDIT + a step: a rest */
        mod_used(B_EDIT);
        song_w();
        line[w].n = EN_REST;
        line[w].f = 0;
        song_done();
    } else if (mod_held(B_ENV)) {                /* ENV + a step: an accent (on, off) */
        mod_used(B_ENV);
        song_w();
        if (line[w].n == EN_REST) {
            line[w].n = ui.note;
            line[w].f = EN_F_ACC;
        } else {
            line[w].f ^= EN_F_ACC;
        }
        song_done();
    } else if (line[w].n == EN_REST) {
        song_w();
        line[w].n = ui.note;
        line[w].f = 0;
        song_done();
        en_audition(l, ui.note, 0);
    } else {
        en_audition(l, line[w].n, line[w].f);
        return;                                  /* let go: it goes (unless held for something) */
    }
    ui.keys_used |= 1u << k;
}

static void key_down(int k)
{
    int w = -1, b = -1, i;
    for (i = 0; i < 16; i++)
        if (WHITE_KEY[i] == k)
            w = i;
    for (i = 0; i < 11; i++)
        if (BLACK_KEY[i] == k)
            b = i;
    switch (editor()) {
    case E_SONG:
        if (!ui.chords) {
            song_key(w, b);
            return;
        }
        break;
    case E_DRUMS: drums_key(k, w, b); return;
    case E_BASS: case E_GUITAR: case E_SYNTH: line_key(k, w, b); return;
    default: break;
    }
    set_root((5 + k) % 12);                      /* key 0 is F3 */
}

static void key_up(int k)
{
    int w = -1, i;
    for (i = 0; i < 16; i++)
        if (WHITE_KEY[i] == k)
            w = i;
    if (w < 0 || !step_editor())
        return;
    if (!(ui.keys_used >> k & 1u)) {             /* a step pressed and let go: off */
        song_w();
        if (editor() == E_DRUMS)
            drum_set(epart(), ui.row, w, 0);
        else
            eline()[w].n = EN_REST, eline()[w].f = 0;
        song_done();
    }
    if (ui.held == w) {
        int j;
        ui.held = -1;
        ui.touched = -1;                         /* the step's knobs are gone */
        for (j = 0; j < 16; j++)                 /* another still held: that one */
            if (j != w && (ui.keys >> WHITE_KEY[j] & 1u))
                ui.held = (int8_t)j;
    }
}

/* ------------------------------------------------------------ buttons --- */
static void set_view(int v)
{
    if (v == ui.view) {                          /* again: the editor, or back from it */
        ui.edit = VIEW_EDIT[v] ? !ui.edit : 0;
    } else {
        ui.edit = ui.edit && VIEW_EDIT[v];       /* from an editor to another page's: its editor */
        ui.view = (uint8_t)v;
    }
    if (ui.edit)
        editor_open();
    ui.held = -1;
    ui.touched = -1;
}

static void toggle_mute(int v)                  /* EDIT + a layer's page button */
{
    uint8_t m = VIEW_MUTE[v];
    int l, on = !(en_muted & m);
    char t[12];
    for (l = 0; l < EN_NLAYER; l++)
        if (m >> l & 1u)
            en_mute(l, on);
    caps(t, VIEW_NAME[v], sizeof t);
    say(on ? "MUTE" : "UNMUTE", t);
}

static void button(int b)
{
    int v;
    for (v = 0; v < NVIEWS; v++)                 /* a page's button: its page (again: its editor) */
        if (VIEW_BTN[v] == b) {
            set_view(v);
            return;
        }
    switch (b) {
    case B_EDIT: {
        char t[12];
        caps(t, EN_SECT_NAME[en_sect_next % EN_NSECT], sizeof t);
        en_next();
        say("NEXT", t);
        break;
    }
    case B_PLAY:
        if (en_playing) {
            en_play(0);
            say("STOPPED", 0);
        } else {
            en_seed(plat_ms());
            en_play(1);
            say("RUNNING", 0);
        }
        break;
    case B_REC:
        en_break();
        say(en_sect == S_BREAK ? "BUILD" : en_sect == S_BUILD ? "DROP" : "BREAKDOWN", en_playing ? "ON THE BAR" : 0);
        break;
    case B_OCTDN:
        en_fill();
        say("FILL", 0);
        break;
    case B_OCTUP:
        if (en_song.locked)
            mark_dirty();                        /* locked, it writes the lines of the part playing */
        en_mutate();
        say("MUTATE", en_playing ? "ON THE BAR" : 0);
        break;
    case B_SAVE:
        if (project_save() == 0) {
            ui.dirty = 0;
            say("SAVED", 0);
        } else {
            say("NOT SAVED", "FLASH ERROR");
        }
        break;
    default: break;
    }
}

/* MIDI in (USB and the TRS jack): channel 1 notes move the root; clock and start / stop drive the grid */
static uint32_t midi_rx_count;                   /* MIDI messages in (the device's SysEx status reports it) */
static void midi_in(void)
{
    uint32_t pkt;
    while (plat_midi_in(&pkt)) {
        uint32_t st = (pkt >> 8) & 0xFFu, n = (pkt >> 16) & 0x7Fu, v = pkt >> 24;
        midi_rx_count++;
        if (st == 0xF8u || st == 0xFAu || st == 0xFBu || st == 0xFCu)
            en_clock(st == 0xF8u ? EN_CLK_TICK : st == 0xFAu ? EN_CLK_START : st == 0xFBu ? EN_CLK_CONTINUE : EN_CLK_STOP);
        else if (st == 0x90u && v)
            set_root((int)(n % 12u));
    }
}

/* HELP CARDS (X0X's): hold one button alone for HELP_HOLD and a card says what it does. A key, a
 * turn or another button puts it away; letting go after its card does nothing else. PLAY acts on
 * its press: it has no card. */
#define HELP_HOLD 900u
#define RESET_HOLD 1500u
static void help_tick(uint32_t btn, uint32_t keys)
{
    uint32_t now = plat_ms();
    int i;
    if (ui.help) {
        uint32_t m = 1u << (ui.help - 1);
        if (btn != m || keys || (int32_t)(ui.turn_t - ui.down_t[ui.help - 1]) > 0)
            ui.help = 0;
        return;
    }
    if (!btn || (btn & (btn - 1u)) || keys)
        return;
    for (i = 0; i < NB; i++)
        if (btn == 1u << i && i != B_PLAY && !(ui.btn_used & btn) && now - ui.down_t[i] >= HELP_HOLD &&
            (int32_t)(ui.turn_t - ui.down_t[i]) <= 0) {
            ui.help = (uint8_t)(i + 1);
            ui.btn_used |= btn;                  /* its release now does nothing */
        }
}

static void select_turn(int32_t e)              /* SELECT: the pages; in an editor, its part, view or ring */
{
    int d = e > 0 ? 1 : -1;
    switch (editor()) {
    case E_SONG: {
        int c = ui.chords + d;
        static const char *const V[4] = {"SECTIONS", "CHORDS A", "CHORDS B", "CHORDS C"};
        if (c >= 0 && c <= 3) {
            ui.chords = (uint8_t)c;
            say("SONG", V[c]);
        }
        return;
    }
    case E_METAL:
        ui.ring = (uint8_t)(d > 0);
        say("RING", ui.ring ? "SCRAP" : "ANVIL");
        return;
    case E_DRUMS: case E_BASS: case E_GUITAR: case E_SYNTH: {
        int p = ui.part + d;
        if (p >= 0 && p < EN_NPARTS) {
            char t[8] = "PART ";
            ui.part = (uint8_t)p;
            ui.held = -1;
            t[5] = EN_PART_NAME[p][0];
            t[6] = 0;
            say(t, part_use(p));
        }
        return;
    }
    default: {
        int v = (int)ui.view + d;
        if (v >= 0 && v < NVIEWS)
            set_view(v);
    }
    }
}

static void input(void)
{
    uint32_t btn = plat_buttons(), keys = plat_keys(), ch, i;
    const uint32_t octs = 1u << B_OCTDN | 1u << B_OCTUP;
    int32_t e;
    ch = btn ^ ui.btn;
    ui.btn = btn;
    if ((btn & octs) == octs)                   /* both held: the update mode's countdown, not a fill */
        ui.btn_used |= octs;
    for (i = 0; i < NB; i++) {
        uint32_t m = 1u << i;
        if (!(ch & m))
            continue;
        if (btn & m) {
            int v;
            ui.btn_used &= ~m;
            ui.down_t[i] = plat_ms();
            if (i == B_SAVE)
                ui.reset_done = 0;
            if (i == B_PLAY)                   /* transport on the press */
                button((int)i);
            if (btn >> B_EDIT & 1u)             /* EDIT held: a layer's page button mutes it */
                for (v = 0; v < NVIEWS; v++)
                    if (VIEW_BTN[v] == i && VIEW_MUTE[v]) {
                        toggle_mute(v);
                        ui.btn_used |= m | 1u << B_EDIT;
                        ui.help = 0;
                    }
        } else if (!(ui.btn_used & m) && i != B_PLAY) {
            button((int)i);
        }
    }
    ch = keys ^ ui.keys;
    ui.keys = keys;
    for (i = 0; i < NKEYS; i++) {
        if (!(ch >> i & 1u))
            continue;
        if (keys >> i & 1u) {
            ui.keys_used &= ~(1u << i);
            key_down((int)i);
        } else {
            key_up((int)i);
        }
    }
    if (btn || keys || ch)
        ui.act_t = plat_ms();
    midi_in();
    if ((e = plat_enc(EN_SELECT)) != 0) {        /* SELECT: the pages; with HOME held, the tempo */
        if (btn >> B_HOME & 1u) {
            turn(EN_SELECT, P_TEMPO, e);
            ui.btn_used |= 1u << B_HOME;
            if (ui.view == V_MACHINE && !ui.edit) {   /* Tempo is the Machine's first knob */
                ui.touched = 0;
                ui.touch_until = plat_ms() + 1200u;
            } else {
                char t[8];
                itoa_u((uint32_t)proj.par[P_TEMPO], t);
                say("TEMPO", t);
            }
        } else {
            select_turn(e);
        }
        ui.turn_t = plat_ms();
    }
    if ((e = plat_enc(EN_ALGO)) != 0) {
        set_mode(proj.par[P_MODE] + (e > 0 ? 1 : -1));
        ui.turn_t = plat_ms();
    }
    if ((e = plat_enc(EN_PRESET)) != 0) {
        set_piece(proj.piece + (e > 0 ? 1 : -1));
        ui.turn_t = plat_ms();
    }
    if ((btn >> B_SAVE & 1u) && !ui.reset_done && plat_ms() - ui.down_t[B_SAVE] > RESET_HOLD) {
        ui.reset_done = 1;                     /* held: the piece back to its recipe (not saved) */
        ui.btn_used |= 1u << B_SAVE;
        ui.help = 0;
        project_piece_reset();
        project_apply();
        en_regrow();
        say("RESET", piece_name(proj.piece));
        mark_dirty();
    }
    for (i = 0; i < 4; i++)
        if ((e = plat_enc(EN_K1 + (int)i)) != 0) {
            turn(EN_K1 + (int)i, knob_id((int)i), e);
            ui.touched = (int8_t)i;
            ui.touch_until = plat_ms() + 1200u;
            ui.act_t = ui.turn_t = plat_ms();
        }
    help_tick(btn, keys);
}

/* ------------------------------------------------------------- screen --- */
/* four bands: on the LCD the status (25 px), the scene (97) and the steps (30); on the case the layers
 * and the knobs (88). Each is drawn again only when what it shows changes. */
#define B0_Y 0
#define B0_H 25
#define B1_Y 25
#define B1_H 97
#define B2_Y 122
#define B2_H 30
#define B3_Y 152
#define B3_H 88
#define K0 (B3_Y + 20)                   /* the knobs' strip */
#define LCD_X 3
#define LCD_Y 3
#define LCD_W 234
#define LCD_H 147

static uint32_t hash(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

static void band(int32_t y, int32_t h, uint16_t bg)   /* a band of the screen, drawn in screen co-ordinates */
{
    cv_begin(240, (uint32_t)h, bg);
    cv_oy = -y;
    cv_y0 = 0;
    cv_y1 = h;
}

/* a rectangle with rounded corners (radius r, up to 6) */
static void cv_round(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c)
{
    static const uint8_t IN[7][6] = {{0}, {1}, {1, 0}, {2, 1, 0}, {2, 1, 0, 0}, {3, 2, 1, 0, 0}, {4, 2, 1, 1, 0, 0}};
    int32_t j;
    if (r > 6)
        r = 6;
    if (r > h / 2)
        r = h / 2;
    cv_rect(x, y + r, w, h - 2 * r, c);
    for (j = 0; j < r; j++) {
        int32_t d = IN[r][j];
        cv_rect(x + d, y + j, w - 2 * d, 1, c);
        cv_rect(x + d, y + h - 1 - j, w - 2 * d, 1, c);
    }
}

static void lcd_glass(void)                      /* the LCD and its frame, clipped to the band */
{
    cv_round(LCD_X - 2, LCD_Y - 2, LCD_W + 4, LCD_H + 4, 6, L_EDGE);
    cv_round(LCD_X, LCD_Y, LCD_W, LCD_H, 5, L_BG);
}

static void text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(cx - text_w(f, s) / 2, y, f, s, c);
}
static void text_r(int32_t rx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(rx - text_w(f, s), y, f, s, c);
}

/* anti-aliased drawing (X0X's): a pixel blended over the canvas, a disc, and a knob's ring */
static void px_blend(int32_t x, int32_t y, uint16_t c, uint32_t a)
{
    uint16_t *q;
    uint32_t bg;
    y += cv_oy;
    if ((uint32_t)x >= cv_w || y < cv_y0 || y >= cv_y1 || !a)
        return;
    q = &cv_px[(uint32_t)y * cv_w + (uint32_t)x];
    if (a >= 16u) {
        *q = swap16(c);
        return;
    }
    bg = swap16(*q);
    *q = swap16((uint32_t)(((((c >> 11) * a + (bg >> 11) * (16u - a)) >> 4) << 11) |
                           (((((c >> 5) & 63u) * a + ((bg >> 5) & 63u) * (16u - a)) >> 4) << 5) |
                           (((c & 31u) * a + (bg & 31u) * (16u - a)) >> 4)));
}

static void disc(float cx, float cy, float r, uint16_t c)
{
    int32_t x0 = (int32_t)fm_floorf(cx - r - 1.0f), x1 = (int32_t)(cx + r + 1.0f);
    int32_t y0 = (int32_t)fm_floorf(cy - r - 1.0f), y1 = (int32_t)(cy + r + 1.0f), x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy, d = fm_sqrtf(dx * dx + dy * dy);
            px_blend(x, y, c, (uint32_t)(fm_clampf(r - d + 0.5f, 0.0f, 1.0f) * 16.0f + 0.5f));
        }
}

/* atan2 in turns, 0..1 from +x going clockwise on screen (y down) */
static float turns(float y, float x)
{
    float ax = fm_fabsf(x), ay = fm_fabsf(y), a = ax < ay ? ax / (ay + 1e-9f) : ay / (ax + 1e-9f), s = a * a, r;
    r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;   /* atan on [0, 1] */
    if (ay > ax)
        r = 1.57079637f - r;
    if (x < 0.0f)
        r = FM_PI - r;
    if (y < 0.0f)
        r = FM_TWO_PI - r;
    return r * (1.0f / FM_TWO_PI);
}

/* a knob: a 270-degree track from 7:30 to 4:30, anti-aliased, round ends; the value filled from
 * the start (or from the top, for a centred value) and a round handle */
static void arc(int32_t cx, int32_t cy, int32_t r, float v, uint16_t track, uint16_t fill, uint16_t bg, int bipolar)
{
    const float th = 3.4f, a0 = 0.375f, sweep = 0.75f;
    float ro = (float)r, ri = ro - th, rm = ro - th * 0.5f, fx = (float)cx + 0.5f, fy = (float)cy + 0.5f, lo, hi;
    int32_t x, y;
    v = fm_clampf(v, 0.0f, 1.0f);
    lo = bipolar ? (v < 0.5f ? v : 0.5f) : 0.0f;
    hi = bipolar ? (v < 0.5f ? 0.5f : v) : v;
    for (y = -r - 1; y <= r + 1; y++)
        for (x = -r - 1; x <= r + 1; x++) {
            float dx = (float)x, dy = (float)y, d = fm_sqrtf(dx * dx + dy * dy), t, a;
            a = fm_clampf(ro + 0.5f - d, 0.0f, 1.0f) * fm_clampf(d - ri + 0.5f, 0.0f, 1.0f);
            if (a <= 0.0f)
                continue;
            t = turns(dy, dx) - a0;
            if (t < 0.0f)
                t += 1.0f;
            t /= sweep;
            if (t > 1.0f)
                continue;
            px_blend(cx + x, cy + y, (t >= lo && t <= hi) ? fill : track, (uint32_t)(a * 16.0f + 0.5f));
        }
    {
        float e0 = a0 * FM_TWO_PI, e1 = (a0 + sweep) * FM_TWO_PI, ev = (a0 + sweep * v) * FM_TWO_PI;
        float el = (a0 + sweep * lo) * FM_TWO_PI;
        disc(fx + fm_cosf(e0) * rm - 0.5f, fy + fm_sinf(e0) * rm - 0.5f, th * 0.5f, lo <= 0.0f && hi > 0.0f ? fill : track);
        disc(fx + fm_cosf(e1) * rm - 0.5f, fy + fm_sinf(e1) * rm - 0.5f, th * 0.5f, hi >= 1.0f ? fill : track);
        if (hi > lo)
            disc(fx + fm_cosf(el) * rm - 0.5f, fy + fm_sinf(el) * rm - 0.5f, th * 0.5f, fill);
        disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, th * 0.5f + 1.3f, bg);
        disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, th * 0.5f + 0.6f, fill);
    }
}

static uint16_t mix(uint16_t a, uint16_t b, int t)   /* t 0..16: a -> b */
{
    int r, g, bl;
    if (t <= 0)
        return a;
    if (t >= 16)
        return b;
    r = ((a >> 11) * (16 - t) + (b >> 11) * t) / 16;
    g = (((a >> 5) & 63) * (16 - t) + ((b >> 5) & 63) * t) / 16;
    bl = ((a & 31) * (16 - t) + (b & 31) * t) / 16;
    return (uint16_t)(r << 11 | g << 5 | bl);
}
static int blink(void) { return (int)((plat_ms() / 160u) & 1u); }

/* a seven-segment digit, its ghost behind it (the LCD's) */
static void seg7(int32_t x, int32_t y, int d, uint16_t on, uint16_t off)
{
    static const uint8_t SEG[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    uint8_t m = d >= 0 && d <= 9 ? SEG[d] : 0;
    const int32_t w = 7, h = 13;
    cv_rect(x + 1, y, w - 2, 2, m & 1u ? on : off);
    cv_rect(x + w - 2, y + 1, 2, h / 2 - 1, m & 2u ? on : off);
    cv_rect(x + w - 2, y + h / 2 + 1, 2, h / 2 - 1, m & 4u ? on : off);
    cv_rect(x + 1, y + h - 2, w - 2, 2, m & 8u ? on : off);
    cv_rect(x, y + h / 2 + 1, 2, h / 2 - 1, m & 16u ? on : off);
    cv_rect(x, y + 1, 2, h / 2 - 1, m & 32u ? on : off);
    cv_rect(x + 1, y + h / 2 - 1, w - 2, 2, m & 64u ? on : off);
}

#include "scenes.c"

/* the status: the piece, auto or locked, the root and the mode; the tempo, running or not, unsaved.
 * A message takes its middle for a moment. */
static void draw_status(void)
{
    char t[24];
    int msg = plat_ms() < ui.msg_until, pend = (en_pending & EN_PEND_ROOT) != 0;
    uint32_t h = hash(hash(hash(2166136261u, proj.piece | (uint32_t)proj.par[P_ROOT] << 8 | (uint32_t)en_root_now << 12 |
                                                 (uint32_t)en_song.locked << 16),
                           en_playing | ui.dirty << 4 | (uint32_t)msg << 5 | (uint32_t)en_ext << 6 | (uint32_t)proj.par[P_MODE] << 8),
                      (uint32_t)proj.par[P_TEMPO] | (uint32_t)pend << 20);
    int32_t x, xr = 232;
    if (msg)
        h = hash(hash(h, (uint32_t)ui.msg[0][0] << 8 | ui.msg[0][1] | (uint32_t)ui.msg[1][0] << 16), ui.msg_until);
    if (en_playing)
        h = hash(h, (en_bars / 4u) & 1u);
    if (pend)
        h = hash(h, (uint32_t)blink());
    if (h == ui.sig[0])
        return;
    ui.sig[0] = h;
    band(B0_Y, B0_H, K_BODY);
    lcd_glass();
    caps(t, piece_name(proj.piece), sizeof t);   /* the piece, and whose song it plays: the machine's, or yours */
    x = cv_text(9, 5, &FONT_S, t, L_INK);
    cv_round(x + 6, 7, 30, 13, 2, en_song.locked ? L_INK : L_GHOST);
    text_c(x + 21, 6, &FONT_XS, en_song.locked ? "LOCK" : "AUTO", en_song.locked ? L_BG : L_MID);
    x += 42;
    {   /* the right: unsaved, running, the tempo in segments */
        if (ui.dirty) {
            cv_rect(xr - 3, 9, 3, 3, L_MID);
            xr -= 7;
        }
        if (en_playing) {                        /* the beat */
            int on = ((en_bars / 4u) & 1u) == 0u, j;
            for (j = 0; j < 5; j++)
                cv_rect(xr - 7 + j, 7 + j, 1, 11 - 2 * j, on ? L_INK : L_MID);
        } else {
            cv_rect(xr - 8, 8, 8, 8, L_MID);
        }
        xr -= 14;
        if (en_ext) {
            text_r(xr, 6, &FONT_XS, "EXT", L_INK);
            xr -= 26;
        } else {
            int d, v = proj.par[P_TEMPO];
            const int lead = v < 100;
            for (d = 2; d >= 0; d--) {
                xr -= 9;
                seg7(xr, 6, d == 0 && lead ? -1 : v % 10, L_INK, L_GHOST);
                v /= 10;
            }
        }
    }
    if (msg) {                                   /* a message (its second part, if there is room) */
        x = cv_text(x, 6, &FONT_XS, ui.msg[0], L_INK);
        if (x + 6 + text_w(&FONT_XS, ui.msg[1]) < xr - 4)
            cv_text(x + 6, 6, &FONT_XS, ui.msg[1], L_MID);
    } else {                                     /* the root (and the one on its way), the mode */
        x = cv_text(x, 6, &FONT_XS, EN_NOTE_NAME[en_root_now % 12u], L_INK);
        if (pend) {
            x = cv_text(x + 2, 6, &FONT_XS, ">", L_MID);
            x = cv_text(x + 2, 6, &FONT_XS, EN_NOTE_NAME[proj.par[P_ROOT] % 12], blink() ? L_INK : L_MID);
        }
        caps(t, EN_MODE_NAME[proj.par[P_MODE] % EN_NMODES], sizeof t);
        if (x + 6 + text_w(&FONT_XS, t) < xr)
            cv_text(x + 5, 6, &FONT_XS, t, L_MID);
    }
    cv_blit(0, B0_Y);
}

/* ------------------------------------------------------- the scene band --- */
/* a button's card (help_tick): its name, then what it does, in the scene's place */
#define HELP_LINES 4
static const char *const BTN_LABEL[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                          "PLAY", "REC", "OCT-", "OCT+"};
static int help_lines(int b, const char *l[HELP_LINES])
{
    int n = 0, e = editor();
#define L(t) (l[n++] = (t))
    if (e && b == VIEW_BTN[ui.view]) {
        L("AGAIN: BACK TO");
        L("THE PAGE");
        return n;
    }
    if (e == E_SONG && !ui.chords) {
        switch (b) {
        case B_EDIT: L("EDIT + A SECTION'S KEY:"); L("IT GOES"); L("ALONE: THE NEXT SECTION"); return n;
        case B_ENV: L("ENV + A SECTION'S KEY:"); L("A COPY AFTER IT"); return n;
        default: break;
        }
    }
    if (e == E_DRUMS || line_editor()) {
        switch (b) {
        case B_EDIT: L("EDIT + A STEP: CLEARED"); L("ALONE: THE NEXT SECTION"); return n;
        case B_ENV:
            if (e == E_DRUMS) {
                L("ENV + A STEP:"); L("AN ACCENT, A GHOST, A HIT");
            } else {
                L("ENV + A STEP:"); L("AN ACCENT ON, OFF");
            }
            return n;
        default: break;
        }
    }
    switch (b) {
    case B_HOME: L("THE MACHINE"); L("TEMPO ARRANGE EVOLVE"); L("INTENSITY"); L("AGAIN: THE SONG"); break;
    case B_SEQ: L("THE DRUMS"); L("LEVEL GROOVE CRUSH KICK"); L("AGAIN: THEIR STEPS"); L("EDIT + SEQ: MUTE"); break;
    case B_LFO: L("THE BASS"); L("LEVEL CUTOFF SQUELCH"); L("DRIVE  AGAIN: ITS NOTES"); L("EDIT + LFO: MUTE"); break;
    case B_ENV: L("THE GUITAR"); L("LEVEL GAIN CHUG RIFF"); L("AGAIN: ITS CHORDS"); L("EDIT + ENV: MUTE"); break;
    case B_ARP: L("THE SYNTH"); L("LEVEL TONE DECAY BUSY"); L("AGAIN: ITS NOTES"); L("EDIT + ARP: MUTE"); break;
    case B_SEL: L("METAL AND NOISE"); L("METAL HITS NOISE TEXTURE"); L("AGAIN: THE RINGS"); L("EDIT + SEL: MUTE"); break;
    case B_FX: L("THE SPACE"); L("REVERB GATE DELAY PUMP"); break;
    case B_GLO: L("SETUP"); L("ROOT TUNE MIDI LIGHTS"); break;
    case B_EDIT: L("THE NEXT SECTION:"); L("THE BAR FILLS, THEN IT"); L("HOLD + A LAYER'S PAGE"); L("BUTTON: MUTE IT"); break;
    case B_REC: L("A BREAKDOWN, ON THE BAR"); L("AGAIN: THE BUILD"); L("AGAIN: THE DROP"); break;
    case B_OCTDN: L("A FILL: THE REST"); L("OF THIS BAR"); L("WITH OCT+, 5 S: UPDATE MODE"); break;
    case B_OCTUP:
        L("MUTATE, FROM THE NEXT BAR");
        L(en_song.locked ? "LOCKED: NEW LINES FOR" : "NEW RIFFS, LINES, CHORDS");
        L(en_song.locked ? "THE PART PLAYING" : "WITH OCT-, 5 S: UPDATE");
        break;
    case B_SAVE: L("SAVES ALL EIGHT PIECES"); L("KEEP HOLDING: THE PIECE"); L("BACK TO ITS RECIPE"); L("AND THE SONG TO AUTO"); break;
    default: break;
    }
#undef L
    return n;
}

static void draw_help(void)
{
    const char *l[HELP_LINES];
    int n = help_lines(ui.help - 1, l), k;
    cv_round(14, 30, 212, 88, 4, L_INK);
    cv_round(16, 32, 208, 84, 3, L_BG);
    cv_round(16, 32, 208, 20, 3, L_INK);
    text_c(120, 34, &FONT_B, BTN_LABEL[ui.help - 1], L_BG);
    for (k = 0; k < n; k++)
        text_c(120, 56 + k * 14, &FONT_XS, l[k], L_INK);
}

/* the caption over the scene: the section and its bar, the place; on the right the chord, or what waits */
static void draw_caption(void)
{
    char t[24], b[8];
    int32_t x;
    caps(t, EN_SECT_NAME[en_sect % EN_NSECT], sizeof t);
    x = cv_text(9, 25, &FONT_XS, t, L_INK);
    if (en_sect_len) {
        itoa_u((uint32_t)en_sect_bar + 1u, b);
        x = cv_text(x + 5, 25, &FONT_XS, b, L_INK);
        x = cv_text(x, 25, &FONT_XS, "/", L_MID);
        itoa_u(en_sect_len, b);
        x = cv_text(x, 25, &FONT_XS, b, L_MID);
    } else {
        x = cv_text(x + 5, 25, &FONT_XS, "LOOP", L_MID);
    }
    cv_text(x + 8, 25, &FONT_XS, PLACE[en_sect % EN_NSECT], L_PRINT);
    {
        const char *w = 0;
        if (en_pending & EN_PEND_BREAK)
            w = en_sect == S_BREAK ? "> BUILD" : en_sect == S_BUILD ? "> DROP" : "> BREAKDOWN";
        else if (en_pending & EN_PEND_NEXT)
            w = "> NEXT";
        else if (en_pending & EN_PEND_MUTATE)
            w = "> MUTATE";
        if (w) {
            text_r(230, 25, &FONT_XS, w, blink() ? L_INK : L_MID);
        } else {
            x = text_w(&FONT_XS, EN_NOTE_NAME[en_chord_root % 12u]);
            cv_text(230 - x, 25, &FONT_XS, EN_NOTE_NAME[en_chord_root % 12u], L_INK);
            text_r(226 - x, 25, &FONT_XS, "ON", L_MID);
        }
    }
    {   /* the quest's map: a block a section, as long as it lasts; this one lit, filling */
        int i, tot = 0, n = en_plan_n;
        int32_t xx = 9, wall = 222;
        for (i = 0; i < n; i++)
            tot += en_plan_len[i] ? en_plan_len[i] : 8;
        for (i = 0; i < n; i++) {
            int len = en_plan_len[i] ? en_plan_len[i] : 8, cur = i == en_plan_pos && !en_jumped;
            int32_t w = (int32_t)(len * (wall - 2 * (n - 1)) / (tot ? tot : 1));
            if (w < 3)
                w = 3;
            cv_rect(xx, 40, w, 3, cur ? L_MID : L_GHOST);
            if (cur) {
                int32_t f = en_sect_len ? (int32_t)((en_sect_bar + 1) * w / en_sect_len) : w;
                cv_rect(xx, 40, f, 3, L_INK);
            } else if (i == en_plan_pos && en_jumped) {
                cv_rect(xx, 43, w, 1, L_MID);    /* left for a jump: comes back after it */
            }
            xx += w + 2;
        }
    }
}

/* -------------------------------------------------------------- the editors --- */
#define EDX 46                                   /* the editors' first step */
static int32_t ed_x(int s) { return EDX + s * 11 + (s / 4) * 2; }

static void ed_title(const char *what, int with_part)
{
    char t[24];
    int32_t x = cv_text(9, 25, &FONT_XS, what, L_INK);
    if (with_part) {
        t[0] = 'P', t[1] = 'A', t[2] = 'R', t[3] = 'T', t[4] = ' ', t[5] = EN_PART_NAME[ui.part % EN_NPARTS][0], t[6] = 0;
        x = cv_text(x + 8, 25, &FONT_XS, t, L_INK);
        cv_text(x + 8, 25, &FONT_XS, part_use(ui.part), L_PRINT);
    }
}

static int part_playing(void) { return en_playing && en_part == ui.part; }

/* the song: its sections, a key each, as the quest's places; the one picked, its layers and its bed */
static const char *const ICON[EN_NSECT][7] = {
    {"########", "#.####.#", "#.#..#.#", "#.#..#.#", "#.#..#.#", "#.#..#.#", 0},     /* the gate */
    {"#######", "#######", "#######", "###.###", "##...##", "#.....#", 0},          /* a banner in the halls */
    {"......##", "......##", "....####", "....####", "..######", "########", 0},   /* the stair */
    {"#.....#", ".#...#.", "..#.#..", "...#...", "..#.#..", ".#...#.", 0},          /* crossed swords */
    {"...#...", "..##...", "..###..", ".#####.", ".#####.", "#.#.#.#", 0},          /* the fire */
    {"#.....#", "#######", "#.#.#.#", "#######", ".#####.", ".#.#.#.", 0},          /* the Warden */
    {"...###...", ".#######.", "#########", ".........", "#########", 0}};         /* the dawn */
static void icon(int32_t x, int32_t y, int s, uint16_t c)
{
    int r, i;
    for (r = 0; ICON[s][r]; r++)
        for (i = 0; ICON[s][r][i]; i++)
            if (ICON[s][r][i] == '#')
                cv_rect(x + 2 * i, y + 2 * r, 2, 2, c);
}

static void draw_song_ed(void)
{
    int i, n = plan_n();
    char t[24], b[8];
    const en_entry_t *e = esec();
    ed_title(en_song.locked ? "SONG  LOCKED" : "SONG  AUTO", 0);
    itoa_u((uint32_t)n, t);
    scopy(t + (n > 9 ? 2 : 1), n == 1 ? " SECTION" : " SECTIONS", 12);
    text_r(230, 25, &FONT_XS, t, L_MID);
    for (i = 0; i < EN_PLAN_MAX; i++) {
        int32_t x = 9 + (i % 8) * 28, y = 42 + (i / 8) * 32;
        int sel = i == ui.sec, play = en_playing && i == en_plan_pos && !en_jumped;
        if (i < n) {
            int s = en_song.plan[i].type % EN_NSECT;
            cv_round(x, y, 26, 30, 3, sel ? L_INK : L_GHOST);
            cv_round(x + 1, y + 1, 24, 28, 2, L_BG);
            icon(x + 13 - (int32_t)(s == S_VERSE || s == S_CHORUS || s == S_BREAK || s == S_DROP ? 7 : s == S_OUTRO ? 9 : 8),
                 y + 3, s, play ? (blink() ? L_HOT : L_INK) : sel ? L_INK : L_MID);
            if (en_plan_len[i])                  /* its length (as Arrange makes it: dimmer); looping: a ring */
                itoa_u(en_plan_len[i], b);
            else
                scopy(b, "o", 2);
            text_c(x + 13, y + 15, &FONT_XS, b, !en_song.plan[i].bars ? L_PRINT : sel ? L_INK : L_MID);
        } else {
            cv_round(x + 4, y + 4, 18, 22, 2, L_GHOST);
            if (i == n)
                text_c(x + 13, y + 7, &FONT_XS, "+", L_MID);
        }
    }
    /* the section picked: its name and length, its layers, its bed */
    caps(t, EN_SECT_NAME[e->type % EN_NSECT], sizeof t);
    {
        int32_t x = cv_text(9, 106, &FONT_XS, t, L_INK), l;
        if (e->bars) {
            itoa_u(e->bars, b);
            x = cv_text(x + 5, 106, &FONT_XS, b, L_INK);
            x = cv_text(x + 3, 106, &FONT_XS, e->bars == 1 ? "BAR" : "BARS", L_MID);
        } else {
            x = cv_text(x + 5, 106, &FONT_XS, "ARRANGE", L_MID);
        }
        x += 6;
        for (l = 0; l < EN_NLAYER; l++) {       /* its layers: the black keys turn them on and off */
            char c1[2];
            c1[0] = LAYER_SHORT[l][0];
            c1[1] = 0;
            cv_rect(x + 2, 107, 11, 11, e->layers >> l & 1u ? L_INK : L_GHOST);
            text_c(x + 8, 105, &FONT_XS, c1, e->layers >> l & 1u ? L_BG : L_MID);
            x += 14;
        }
        if (x + 8 < 230)
            text_r(230, 106, &FONT_XS, e->tex == EN_TEX_KNOB ? "" : BED_NAME[e->tex % EN_NTEX], L_MID);
    }
}

static void draw_chords_ed(void)
{
    const int p = (ui.chords + 2) % 3, now = en_playing && en_part == p;
    const en_part_t *pt = &en_song.part[p];
    int i;
    char t[16];
    ui.part = (uint8_t)p;
    ed_title("CHORDS", 1);
    for (i = 0; i < 4; i++) {
        int32_t x = 9 + i * 57;
        int lit = now && en_chord_root == (uint8_t)((proj.par[P_ROOT] + en_degree(EN_MODE_POOL[proj.par[P_MODE] % EN_NMODES],
                                                                                      pt->prog[i]) + 36) % 12) &&
                  (en_sect_len >= 4 ? en_sect_bar / (en_sect_len / 4) : en_sect_len ? en_sect_bar : en_sect_bar / 4) % 4 == i;
        cv_round(x, 44, 51, 58, 4, lit ? L_INK : L_GHOST);
        cv_round(x + 2, 46, 47, 54, 3, lit ? L_INK : L_BG);
        deg_name(pt->prog[i], t);
        text_c(x + 26, 54, &FONT_M, t, lit ? L_BG : L_INK);
        itoa_u((uint32_t)i + 1u, t);
        text_c(x + 26, 84, &FONT_XS, t, lit ? L_BG : L_MID);
    }
    text_c(120, 106, &FONT_XS, "A QUARTER OF THE SECTION EACH", L_PRINT);
}

static void draw_drums_ed(void)
{
    const en_part_t *p = epart();
    int r, s, play = part_playing() ? en_step : -1;
    ed_title("DRUMS", 1);
    if (play >= 0)
        cv_rect(ed_x(play) - 1, 40, 12, NDROW * 9 + 1, L_GHOST);
    for (r = 0; r < NDROW; r++) {
        int32_t y = 41 + r * 9;
        if (r == ui.row)                         /* the drum the black keys picked, by name; the others a tick */
            text_r(EDX - 6, y - 4, &FONT_XS, DROW_NAME[r], L_INK);
        else
            cv_rect(EDX - 5, y + 2, 2, 2, L_PRINT);
        for (s = 0; s < EN_NSTEP; s++) {
            int v = drum_get(p, r, s);
            uint16_t c = !v ? L_GHOST : v <= EN_VEL_GHOST ? L_PRINT : v >= EN_VEL_ACC ? L_HOT : r == ui.row ? L_INK : L_MID;
            cv_rect(ed_x(s), y, 10, 7, c);
            if (!v && s == ui.held && r == ui.row)
                cv_rect(ed_x(s), y, 10, 7, L_PRINT);
        }
    }
}

static void draw_line_ed(void)
{
    static const char *const NAME[3] = {"BASS", "GUITAR", "SYNTH"};
    const en_note_t *line = eline();
    const int e = editor(), play = part_playing() ? en_step : -1;
    int d, s;
    char t[8];
    ed_title(NAME[e - E_BASS], 1);
    if (play >= 0)
        cv_rect(ed_x(play) - 1, 40, 12, 11 * 7 + 1, L_GHOST);
    for (d = DEG_HI; d >= DEG_LO; d--) {         /* the rows: the black keys' notes, high to low */
        int32_t y = 41 + (DEG_HI - d) * 7;
        cv_rect(EDX - 4, y + 2, 2, 2, d == ui.note ? L_INK : (d == 0 || d == 7) ? L_MID : L_GHOST);
    }
    deg_name(ui.note, t);                        /* the note picked */
    cv_text(9, 50, &FONT_B, t, L_INK);
    cv_text(9, 72, &FONT_XS, "NOTE", L_PRINT);
    for (s = 0; s < EN_NSTEP; s++) {
        const en_note_t *n = &line[s];
        int dd, f = n->f;
        int32_t x = ed_x(s), y;
        uint16_t c;
        if (n->n == EN_REST) {
            cv_rect(x + 3, 41 + (DEG_HI - 0) * 7 + 2, 4, 2, s == ui.held ? L_MID : L_GHOST);
            continue;
        }
        dd = n->n < DEG_LO ? DEG_LO : n->n > DEG_HI ? DEG_HI : n->n;
        y = 41 + (DEG_HI - dd) * 7;
        c = (f & EN_F_ACC) ? L_HOT : L_INK;
        if (s == ui.held)
            c = L_MID;
        if (e == E_GUITAR && !(f & EN_F_OPEN))
            cv_rect(x, y + 2, 10, 4, c);         /* a chug: palm-muted, short */
        else
            cv_rect(x, y, 10, 6, c);
        if (f & EN_F_LONG)
            cv_rect(x + 10, y + 2, 3, 2, c);     /* held on */
        if (e == E_BASS && (f & EN_F_SLIDE))
            cv_rect(x - 3, y + 2, 3, 2, c);      /* slid into */
        if ((e == E_BASS && (f & EN_F_OCT)) || (e == E_SYNTH && (f & EN_F_LEAD)))
            cv_rect(x + 4, y - 2, 2, 2, c);      /* an octave up; a line */
        if (n->n != dd)
            cv_rect(x + 3, n->n > dd ? y - 1 : y + 6, 4, 1, L_MID);   /* beyond the rows */
    }
}

static void draw_metal_ed(void)
{
    char t[24], b[8];
    int r;
    ed_title("METAL  THE RINGS", 0);
    itoa_u(en_song.metal_tone, b);
    scopy(t, "TONE ", 8);
    scopy(t + 5, b, 8);
    text_r(230, 25, &FONT_XS, t, L_MID);
    for (r = 0; r < 2; r++) {
        const int n = en_song.ring_len[r] < 1 ? 1 : en_song.ring_len[r] > 16 ? 16 : en_song.ring_len[r];
        const uint32_t g = en_euclid(en_song.ring_hits[r], n, en_song.ring_turn[r] % n);
        const int now = en_playing ? (int)((en_bars ? en_bars - 1u : 0u) % (uint32_t)n) : -1, sel = r == (ui.ring & 1);
        const float cx = 62.0f + 116.0f * (float)r, cy = 72.0f;
        int i;
        for (i = 0; i < n; i++) {
            float a = ((float)i / (float)n - 0.25f) * FM_TWO_PI;
            int32_t x = (int32_t)(cx + fm_cosf(a) * 25.0f), y = (int32_t)(cy + fm_sinf(a) * 25.0f);
            int hit = (g >> i & 1u) != 0;
            uint16_t c = hit ? (i == now ? L_HOT : sel ? L_INK : L_MID) : (i == now ? L_MID : L_GHOST);
            cv_rect(x - (hit ? 3 : 2), y - (hit ? 3 : 2), hit ? 6 : 4, hit ? 6 : 4, c);
        }
        caps(t, r ? "SCRAP" : "ANVIL", sizeof t);
        text_c((int32_t)cx, (int32_t)cy - 6, &FONT_XS, t, sel ? L_INK : L_PRINT);
        itoa_u(en_song.ring_hits[r], t);
        scopy(t + (en_song.ring_hits[r] > 9 ? 2 : 1), "/", 4);
        itoa_u((uint32_t)n, b);
        scopy(t + (en_song.ring_hits[r] > 9 ? 3 : 2), b, 4);
        text_c((int32_t)cx, (int32_t)cy + 4, &FONT_XS, t, sel ? L_INK : L_PRINT);
    }
}

static void draw_scene(void)
{
    scn_t s;
    uint32_t h;
    int i;
    const uint32_t now = plat_ms();
    for (i = 0; i < EN_NROW; i++)                /* what played: counted by the engine, timed here */
        if (en_hits[i] != ui.seen[i]) {
            ui.seen[i] = en_hits[i];
            ui.hit_t[i] = now;
            ui.cnt[i]++;
        }
    if (ui.frame % 2u)                           /* 30 frames a second */
        return;
    memset(&s, 0, sizeof s);
    s.ms = now;
    s.frame = ui.frame / 2u;
    s.playing = en_playing;
    s.step = en_step;
    s.sect = en_sect % EN_NSECT;
    s.part = en_part;
    s.sect_bar = en_sect_bar;
    s.sect_len = en_sect_len;
    {
        int len = en_sect_len ? en_sect_len : 4, bar = en_sect_len ? en_sect_bar : en_sect_bar % 4;
        s.prog = fm_clampf(((float)bar * 16.0f + (float)(en_playing ? en_step : 0)) / ((float)len * 16.0f), 0.0f, 0.999f);
    }
    for (i = 0; i < EN_NROW; i++) {
        s.ago[i] = now - ui.hit_t[i] > 60000u ? 60000u : now - ui.hit_t[i];
        s.cnt[i] = ui.cnt[i];
    }
    s.noise = en_level[LY_NOISE];
    h = scene_hash(&s);
    h = hash(h, (uint32_t)en_pending | (uint32_t)en_chord_root << 8 | (uint32_t)en_plan_n << 12 | (uint32_t)en_plan_pos << 20 |
                    (uint32_t)en_jumped << 28);
    if (en_pending)
        h = hash(h, (uint32_t)blink());
    if (editor()) {                              /* an editor: what it shows */
        const uint8_t *b = (const uint8_t *)&en_song;
        uint32_t k;
        h = hash(0x45444954u, (uint32_t)editor() | (uint32_t)ui.part << 4 | (uint32_t)ui.row << 8 | (uint32_t)(ui.note & 15) << 12 |
                                  (uint32_t)ui.sec << 16 | (uint32_t)ui.chords << 21 | (uint32_t)ui.ring << 23 |
                                  (uint32_t)(ui.held + 1) << 24);
        for (k = 0; k < sizeof en_song; k++)
            h = hash(h, b[k]);
        h = hash(h, (uint32_t)(en_playing ? en_step : 0xFF) | (uint32_t)en_part << 8 | (uint32_t)en_plan_pos << 12 |
                        (uint32_t)en_chord_root << 20 | (uint32_t)en_sect_bar << 24);
        h = hash(h, (uint32_t)blink() | (uint32_t)proj.par[P_ROOT] << 4 | (uint32_t)proj.par[P_MODE] << 8 | en_bars << 12);
    }
    if (ui.help)
        h = hash(0x48454C50u, ui.help | (uint32_t)editor() << 8);
    if (h == ui.sig[1])
        return;
    ui.sig[1] = h;
    band(B1_Y, B1_H, K_BODY);
    lcd_glass();
    if (ui.help) {
        draw_help();
    } else {
        switch (editor()) {
        case E_SONG:
            if (ui.chords)
                draw_chords_ed();
            else
                draw_song_ed();
            break;
        case E_DRUMS: draw_drums_ed(); break;
        case E_BASS: case E_GUITAR: case E_SYNTH: draw_line_ed(); break;
        case E_METAL: draw_metal_ed(); break;
        default:
            draw_caption();
            scene_draw(&s);
            break;
        }
    }
    cv_blit(0, B1_Y);
}

/* ------------------------------------------------------- the steps band --- */
/* the sixteen steps under the white keys: the bar playing (the page's layer), or what the editor writes */
#define STEP_X 12
static int32_t step_x(int s) { return STEP_X + s * 13 + (s / 4) * 3; }

static void draw_steps(void)
{
    const int e = editor(), l = VIEW_LAYER[ui.view];
    int s, play = en_playing ? en_step : -1;
    uint32_t h = hash(2166136261u, (uint32_t)e | (uint32_t)ui.view << 4 | (uint32_t)(play & 0xFF) << 8 | (uint32_t)ui.part << 16 |
                                       (uint32_t)ui.row << 20 | (uint32_t)(ui.held + 1) << 24);
    if (e == E_DRUMS || line_editor()) {
        const en_part_t *p = epart();
        const uint8_t *b = (const uint8_t *)p;
        uint32_t k;
        for (k = 0; k < sizeof *p; k++)
            h = hash(h, b[k]);
        h = hash(h, (uint32_t)en_part);
    } else if (e == E_SONG) {
        h = hash(h, (uint32_t)plan_n() | (uint32_t)ui.sec << 8 | (uint32_t)en_plan_pos << 16 | (uint32_t)en_jumped << 24);
        h = hash(h, (uint32_t)blink());
    } else if (e == E_METAL) {
        h = hash(h, (uint32_t)en_song.ring_len[ui.ring & 1] | (uint32_t)en_song.ring_hits[ui.ring & 1] << 8 |
                        (uint32_t)en_song.ring_turn[ui.ring & 1] << 16 | (uint32_t)ui.ring << 24);
        h = hash(h, en_bars);
    } else {
        int r;
        for (r = 0; r < EN_NROW; r++)
            h = hash(h, en_grid[r]);
        h = hash(h, (uint32_t)en_grid_vel[EN_ROW_KICK][play & 15] | (uint32_t)en_active << 8 | (uint32_t)en_muted << 16);
    }
    if (h == ui.sig[2])
        return;
    ui.sig[2] = h;
    band(B2_Y, B2_H, K_BODY);
    lcd_glass();
    if (e == E_SONG) {                           /* the sections */
        int n = plan_n();
        for (s = 0; s < EN_NSTEP; s++) {
            int32_t x = step_x(s), y = 126;
            int cur = en_playing && s == en_plan_pos && !en_jumped;
            if (s < n)
                cv_rect(x, s == ui.sec ? y : y + 3, 11, s == ui.sec ? 12 : 9, cur && blink() ? L_HOT : s == ui.sec ? L_INK : L_MID);
            else
                cv_rect(x, y + 3, 11, 9, s == n ? L_PRINT : L_GHOST);
            cv_rect(x, y + 18, 11, 3, cur ? L_INK : L_GHOST);
        }
        cv_blit(0, B2_Y);
        return;
    }
    if (e == E_METAL) {                          /* the ring picked, its hits as they fall */
        const int r = ui.ring & 1, n = en_song.ring_len[r] < 1 ? 1 : en_song.ring_len[r] > 16 ? 16 : en_song.ring_len[r];
        const uint32_t g = en_euclid(en_song.ring_hits[r], n, en_song.ring_turn[r] % n);
        const int now = en_playing ? (int)((en_bars ? en_bars - 1u : 0u) % (uint32_t)n) : -1;
        for (s = 0; s < EN_NSTEP; s++) {
            int32_t x = step_x(s), y = 126;
            if (s >= n) {
                cv_rect(x + 2, y + 6, 7, 1, L_GHOST);
                continue;
            }
            cv_rect(x, y + 3, 11, 9, g >> s & 1u ? L_INK : L_GHOST);
            cv_rect(x, y + 18, 11, 3, s == now ? L_INK : L_GHOST);
        }
        cv_blit(0, B2_Y);
        return;
    }
    if (step_editor() && !part_playing())
        play = -1;
    for (s = 0; s < EN_NSTEP; s++) {
        int32_t x = step_x(s), y = 126;
        int v = 0, acc = 0, marks = 0, dy = 0, held = s == ui.held;
        if (e == E_DRUMS) {
            const en_part_t *p = epart();
            int r;
            v = drum_get(p, ui.row, s);
            acc = v >= EN_VEL_ACC;
            for (r = 0; r < NDROW; r++)
                if (r != ui.row && drum_get(p, r, s))
                    marks++;
        } else if (line_editor()) {
            const en_note_t *n = &eline()[s];
            if (n->n != EN_REST) {
                int dd = n->n < DEG_LO ? DEG_LO : n->n > DEG_HI ? DEG_HI : n->n;
                v = 1;
                acc = (n->f & EN_F_ACC) != 0;
                dy = (DEG_HI - dd);              /* a little roll: high notes high */
            }
        } else {
            static const uint8_t ROW[EN_NLAYER] = {EN_ROW_KICK, EN_ROW_BASS, EN_ROW_GUITAR, EN_ROW_SYNTH, EN_ROW_METAL, EN_ROW_METAL};
            v = en_grid_vel[ROW[l]][s];
            acc = v >= 110;
            if (l == LY_DRUMS)
                marks = (en_grid_vel[EN_ROW_SNARE][s] != 0) + (en_grid_vel[EN_ROW_HAT][s] != 0);
        }
        if (line_editor()) {
            cv_rect(x, y + 1, 11, 16, L_GHOST);
            if (v)
                cv_rect(x, y + 1 + dy * 3 / 2, 11, 3, held ? L_MID : acc ? L_HOT : L_INK);
        } else if (v) {
            cv_rect(x, acc ? y : y + 3, 11, acc ? 12 : 9, held ? L_MID : v <= EN_VEL_GHOST ? L_PRINT : acc ? L_HOT : L_INK);
        } else {
            cv_rect(x, y + 3, 11, 9, held ? L_MID : L_GHOST);
        }
        if (marks)                               /* the other drums on this step */
            cv_rect(x + 4 - (marks > 1 ? 2 : 0), y + 14, marks > 1 ? 7 : 3, 2, L_MID);
        cv_rect(x, y + 18, 11, 3, s == play ? L_INK : L_GHOST);
    }
    cv_blit(0, B2_Y);
}

/* ----------------------------------------------------------- the case --- */
/* the layers (playing, resting, muted; the page's lit), then the rule (where you are) and the knobs */
static void draw_body(void)
{
    uint32_t h = hash(2166136261u, ui.view | (uint32_t)ui.touched << 8 | (uint32_t)en_muted << 16 | (uint32_t)en_active << 24), i;
    int now_touch = plat_ms() < ui.touch_until, l;
    const uint16_t c = LAYER_C[VIEW_LAYER[ui.view]];
    for (i = 0; i < 4; i++)
        h = hash(h, (uint32_t)knob_id((int)i) | (uint32_t)knob_get(knob_id((int)i)) << 8);
    for (l = 0; l < EN_NLAYER; l++)
        h = hash(h, (uint32_t)(en_level[l] * 16.0f));
    h = hash(h, (uint32_t)now_touch | (uint32_t)editor() << 1 | (uint32_t)(ui.held + 1) << 4 | (uint32_t)ui.part << 10 |
                    (uint32_t)ui.chords << 12 | (uint32_t)ui.ring << 14 | (uint32_t)proj.par[P_ROOT] << 16 |
                    (uint32_t)proj.par[P_MODE] << 20);
    if (h == ui.sig[3])
        return;
    ui.sig[3] = h;
    band(B3_Y, B3_H, K_BODY);
    for (l = 0; l < EN_NLAYER; l++) {            /* the layers */
        int32_t x = 3 + l * 39, w = 36;
        int sel = l == VIEW_LAYER[ui.view] && ui.view >= V_DRUMS && ui.view <= V_METAL;
        int mute = (en_muted >> l & 1u) != 0, act = (en_active >> l & 1u) != 0;
        int lv = (int)(fm_clampf(en_level[l], 0.0f, 1.0f) * (float)(w - 8));
        if (ui.view == V_METAL && l == LY_NOISE)
            sel = 1;
        cv_round(x, B3_Y + 1, w, 17, 3, sel ? LAYER_C[l] : K_PANEL);
        cv_text(x + 5, B3_Y + 2, &FONT_XS, LAYER_SHORT[l], mute ? K_RED : sel ? K_BODY : act ? K_TEXT : K_DIM);
        if (mute)
            cv_rect(x + 4, B3_Y + 9, w - 8, 1, sel ? K_BODY : K_RED);
        else if (lv > 0)
            cv_rect(x + 4, B3_Y + 15, lv, 2, sel ? K_BODY : LAYER_C[l]);
    }
    {   /* the rule; at its right end, where you are: the page (or the editor, the step), then the dots */
        char tt[24];
        int k;
        int32_t xd = 237 - NVIEWS * 6, xt, x0 = 4;
        const uint16_t pc = ui.view >= V_DRUMS && ui.view <= V_METAL ? c : K_ACC;
        if (ui.held >= 0 && step_editor()) {
            scopy(tt, "STEP ", 8);
            itoa_u((uint32_t)ui.held + 1u, tt + 5);
        } else if (editor()) {
            static const char *const EN[7] = {"", "SONG", "DRUMS", "BASS", "GUITAR", "SYNTH", "RINGS"};
            scopy(tt, EN[editor()], 10);
            if (editor() == E_SONG && ui.chords)
                scopy(tt, "CHORDS", 10);
        } else {
            caps(tt, VIEW_NAME[ui.view], sizeof tt);
        }
        xt = xd - 5 - text_w(&FONT_XS, tt);
        if (editor() == E_DRUMS) {               /* the drum the black keys picked */
            x0 = cv_text(5, K0 - 1, &FONT_XS, DROW_NAME[ui.row], K_TEXT) + 6;
        } else if (ui.view == V_SETUP) {         /* Setup: which IND-1 this is */
            x0 = cv_text(5, K0 - 1, &FONT_XS, "IND-1 v" EN_VERSION, K_DIM) + 6;
        } else if (VIEW_MUTE[ui.view] && (en_muted & VIEW_MUTE[ui.view]) == VIEW_MUTE[ui.view]) {
            x0 = cv_text(5, K0 - 1, &FONT_XS, "MUTED", K_RED) + 6;
        }
        if (xt - 9 > x0)
            cv_rect(x0, K0 + 5, xt - 9 - x0, 1, K_LINE);
        cv_text(xt, K0 - 1, &FONT_XS, tt, pc);
        for (k = 0; k < NVIEWS; k++)
            cv_round(xd + k * 6, K0 + 3, 4, 4, 1, k == ui.view ? (ui.edit ? K_TEXT : pc) : K_LINE);
    }
    for (i = 0; i < 4; i++) {
        int k = knob_id((int)i), lo, hi, v, hot = now_touch && ui.touched == (int)i;
        int32_t x = (int32_t)i * 60, cx = x + 30;
        char t[16];
        const uint16_t fc = ui.view >= V_DRUMS && ui.view <= V_METAL ? c : K_ACC;
        uint16_t bg = hot ? K_PANEL : K_BODY, fill = hot ? fc : K_TEXT;
        if (k == K_NONE)
            continue;
        if (i)
            cv_rect(x, K0 + 16, 1, 46, K_LINE);
        if (hot)
            cv_round(x + 3, K0 + 11, 54, 57, 4, K_PANEL);
        knob_range(k, &lo, &hi);
        v = knob_get(k);
        caps(t, knob_label((int)i), sizeof t);
        text_c(cx, K0 + 12, &FONT_XS, t, hot ? K_TEXT : K_DIM);
        if (k != K_CPU && hi - lo < 6) {         /* a switch: its positions as pips */
            int n = hi - lo + 1, j, pw = n > 4 ? 6 : 9, gap = 3, w0 = n * (pw + gap) - gap;
            for (j = 0; j < n; j++)
                cv_round(cx - w0 / 2 + j * (pw + gap), K0 + 33, pw, 8, 1, j == v - lo ? fill : K_LINE);
        } else if (k != K_CPU) {
            arc(cx, K0 + 37, 12, hi > lo ? (float)(v - lo) / (float)(hi - lo) : 0.0f, K_LINE, fill, bg, lo < 0);
        }
        knob_text(k, t);
        text_c(cx, K0 + 50, &FONT_B, t, hot ? fc : K_TEXT);
    }
    cv_blit(0, B3_Y);
}

static void leds(void)
{
    uint32_t b = 0, k = 0, i;
    const int bl = blink(), e = editor();
    b |= 1u << VIEW_BTN[ui.view];
    if (en_playing)
        b |= 1u << B_PLAY;
    if (en_sect == S_BREAK || en_sect == S_BUILD || ((en_pending & EN_PEND_BREAK) && bl))
        b |= 1u << B_REC;
    if ((en_pending & EN_PEND_NEXT) && bl)
        b |= 1u << B_EDIT;
    if (en_pending & EN_PEND_FILL)
        b |= 1u << B_OCTDN;
    if ((en_pending & EN_PEND_MUTATE) && bl)
        b |= 1u << B_OCTUP;
    if (proj.leds) {
        if (e == E_SONG && !ui.chords) {         /* the sections (the one playing blinks), the picked one's layers */
            int n = plan_n();
            for (i = 0; i < 16u; i++)
                if ((int)i < n && !(en_playing && (int)i == en_plan_pos && !en_jumped && bl))
                    k |= 1u << WHITE_KEY[i];
            for (i = 0; i < EN_NLAYER; i++)
                if (esec()->layers >> i & 1u)
                    k |= 1u << BLACK_KEY[i];
        } else if (step_editor()) {              /* the steps that play (the playhead blinks), the drum or the note */
            int play = part_playing() ? en_step : -1;
            for (i = 0; i < 16u; i++) {
                int on = e == E_DRUMS ? drum_get(epart(), ui.row, (int)i) != 0 : eline()[i].n != EN_REST;
                if ((int)i == play)
                    on = !on || bl;
                if (on)
                    k |= 1u << WHITE_KEY[i];
            }
            if (e == E_DRUMS)
                k |= 1u << BLACK_KEY[ui.row % NDROW];
            else
                for (i = 0; i < 11u; i++)
                    if (KEY_DEG[i] == ui.note)
                        k |= 1u << BLACK_KEY[i];
        } else {                                 /* the root's keys; the new one blinks on its way */
            for (i = 0; i < NKEYS; i++) {
                uint32_t pc = (5u + i) % 12u;
                if (pc == en_root_now || (pc == (uint32_t)proj.par[P_ROOT] && bl))
                    k |= 1u << i;
            }
        }
    }
    plat_glow(proj.leds == 1);
    plat_leds(b, k);
}

/* AUTOSAVE: a few seconds after a change, while stopped and quiet, nothing touched (a flash erase
 * silences the audio for a moment: never while the machine runs; SAVE saves then) */
#define AUTOSAVE_QUIET 4000u
static void autosave(void)
{
    uint32_t now = plat_ms();
    int l;
    float lvl = 0.0f;
    for (l = 0; l < EN_NLAYER; l++)
        lvl = fm_maxf(lvl, en_level[l]);
    if (!ui.dirty || en_playing || ui.btn || ui.keys || now - ui.act_t < AUTOSAVE_QUIET || lvl > 0.01f)
        return;
    if (project_save() == 0) {
        ui.dirty = 0;
        ui.saved_t = now;
    }
}

void ui_init(void)
{
    memset(&ui, 0, sizeof ui);
    ui.touched = -1;
    ui.held = -1;
    lcd_fill(0, 0, 240, 240, K_BODY);
    if (!en_playing)
        en_play(1);                              /* it runs from the start */
}

void ui_frame(void)
{
    input();
    autosave();
    draw_status();
    draw_scene();
    draw_steps();
    draw_body();
    leds();
    ui.frame++;
}

void ui_input_only(void) { input(); }
