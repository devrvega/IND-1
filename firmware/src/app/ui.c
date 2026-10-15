/* SPDX-License-Identifier: GPL-3.0-only */
/* IND-1 UI: the panel and the screen. Part of the unity build (after gfx.c and project.c); the host
 * simulator includes it too.
 *
 *   keys           the root: the machine moves to the key's note on the next bar (MIDI channel 1 too)
 *   PLAY           run / stop                      REC    a breakdown; in one, the build; in the build, the drop
 *   EDIT           the next section (the rest of the bar fills)
 *   EDIT + a layer's page button: mute it          OCT-   a fill        OCT+   mutate: new riffs and lines
 *   HOME SEQ LFO ENV ARP SEL FX GLO   the pages: Machine, Drums, Bass, Guitar, Synth, Metal, Space, Setup
 *   SELECT the pages (with HOME held, the tempo)   ALGORITHM the mode     PRESETS the piece
 *   KNOB 1-4       the page's four values
 *   SAVE           saves (it also saves by itself a few seconds after a change, while stopped);
 *                  held 1.5 s: the piece back to its recipe */
#include "../dsp/fastmath.h"

/* the pages; SELECT steps through them in this order */
enum { V_MACHINE, V_DRUMS, V_BASS, V_GUITAR, V_SYNTH, V_METAL, V_SPACE, V_SETUP, NVIEWS };
static const char *const VIEW_NAME[NVIEWS] = {"Machine", "Drums", "Bass", "Guitar", "Synth", "Metal", "Space", "Setup"};
static const uint8_t VIEW_BTN[NVIEWS] = {B_HOME, B_SEQ, B_LFO, B_ENV, B_ARP, B_SEL, B_FX, B_GLO};
/* the layers a page's button mutes (with EDIT held): the Metal page holds the metal and the noise */
static const uint8_t VIEW_MUTE[NVIEWS] = {0, 1u << LY_DRUMS, 1u << LY_BASS, 1u << LY_GUITAR, 1u << LY_SYNTH,
                                          1u << LY_METAL | 1u << LY_NOISE, 0, 0};
/* the four knobs of each page: a parameter, or one of these; and what the knob is called there */
enum { K_LEDS = 100, K_CPU, K_NONE };
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

/* the screen's colours: a factory at night. Near-black, steel greys for the structure, hazard orange for
 * where you are and what you touch; each layer its own colour (bone drums, rust bass, red guitar, cold
 * synth, steel metal, a toxic green noise) */
#define K_BG RGB(14, 14, 16)
#define K_PANEL RGB(34, 34, 38)
#define K_LINE RGB(56, 56, 62)
#define K_DIM RGB(118, 116, 110)
#define K_TEXT RGB(232, 228, 216)
#define K_ACC RGB(255, 138, 24)          /* hazard orange: the page, the knob being turned, a message's word */
#define K_WARN RGB(250, 204, 21)         /* hazard yellow: what waits for the bar */
#define K_RED RGB(228, 44, 36)           /* muted */
static const uint16_t LAYER_C[EN_NLAYER] = {RGB(226, 220, 204), RGB(255, 112, 28), RGB(228, 44, 36),
                                            RGB(70, 190, 232), RGB(160, 166, 178), RGB(146, 178, 64)};
static const uint8_t ROW_LAYER[EN_NROW] = {LY_DRUMS, LY_DRUMS, LY_DRUMS, LY_METAL, LY_BASS, LY_GUITAR, LY_SYNTH};
static const char *const ROW_NAME[EN_NROW] = {"KCK", "SNR", "HAT", "MTL", "BAS", "GTR", "SYN"};
static const uint16_t SECT_C[EN_NSECT] = {RGB(96, 100, 110), RGB(170, 92, 30), RGB(250, 204, 21), RGB(255, 138, 24),
                                          RGB(60, 130, 170), RGB(228, 44, 36), RGB(96, 100, 110)};

static struct {
    uint8_t view;
    uint32_t btn, keys, btn_used;
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
    uint32_t sig[3];                   /* what each band shows: redrawn when it changes */
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

static void mark_dirty(void) { ui.dirty = 1; }
int ui_dirty(void) { return ui.dirty != 0; }

/* ------------------------------------------------------------ values --- */
static int knob_get(int k)
{
    if (k < P_NPARAMS)
        return proj.par[k];
    if (k == K_LEDS)
        return proj.leds;
    if (k == K_CPU)
        return (int)plat_cpu_pct();
    return 0;
}
static void knob_range(int k, int *lo, int *hi)
{
    if (k < P_NPARAMS) {
        *lo = en_param_info(k)->lo;
        *hi = en_param_info(k)->hi;
    } else {
        *lo = 0;
        *hi = k == K_LEDS ? 2 : k == K_CPU ? 100 : 0;
    }
}
static void knob_text(int k, char *b)
{
    int v = knob_get(k);
    if (k < P_NPARAMS) {
        en_param_text(k, v, b);
    } else if (k == K_LEDS) {                    /* On: the keys, and the unlit buttons glow; Keys: no glow */
        const char *n = v == 2 ? "Keys" : v ? "On" : "Off";
        while ((*b++ = *n++))
            ;
    } else if (k == K_CPU) {
        itoa_u((uint32_t)v, b);
        while (*b)
            b++;
        *b++ = '%';
        *b = 0;
    } else {
        b[0] = 0;
    }
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
    } else if (k == K_LEDS) {
        proj.leds = (uint8_t)v;
    }
    mark_dirty();
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
    en_regrow();
    say("PIECE", piece_name(proj.piece));
    mark_dirty();
}

/* ------------------------------------------------------------ buttons --- */
static void set_view(int v)
{
    ui.view = (uint8_t)v;
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
    for (v = 0; v < NVIEWS; v++)                 /* a page's button: its page */
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
static void midi_in(void)
{
    uint32_t pkt;
    while (plat_midi_in(&pkt)) {
        uint32_t st = (pkt >> 8) & 0xFFu, n = (pkt >> 16) & 0x7Fu, v = pkt >> 24;
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
    for (i = 0; i < NKEYS; i++)
        if ((ch & keys) >> i & 1u)
            set_root((int)((5u + i) % 12u));    /* key 0 is F3 */
    if (btn || keys || ch)
        ui.act_t = plat_ms();
    midi_in();
    if ((e = plat_enc(EN_SELECT)) != 0) {        /* SELECT: the pages; with HOME held, the tempo */
        if (btn >> B_HOME & 1u) {
            turn(EN_SELECT, P_TEMPO, e);
            ui.btn_used |= 1u << B_HOME;
            if (ui.view == V_MACHINE) {          /* Tempo is the Machine's first knob */
                ui.touched = 0;
                ui.touch_until = plat_ms() + 1200u;
            } else {
                char t[8];
                itoa_u((uint32_t)proj.par[P_TEMPO], t);
                say("TEMPO", t);
            }
        } else {
            int v = (int)ui.view + (e > 0 ? 1 : -1);
            if (v >= 0 && v < NVIEWS)
                set_view(v);
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
            turn(EN_K1 + (int)i, VIEW_KNOB[ui.view][i], e);
            ui.touched = (int8_t)i;
            ui.touch_until = plat_ms() + 1200u;
            ui.act_t = ui.turn_t = plat_ms();
        }
    help_tick(btn, keys);
}

/* ------------------------------------------------------------- screen --- */
/* three bands: the header (28 px), the machine (144), the knobs (68) */
#define HDR_H 28
#define MAIN_Y 28
#define MAIN_H 144
#define KNB_Y 172
#define KNB_H 68

static uint32_t hash(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

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

static void text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(cx - text_w(f, s) / 2, y, f, s, c);
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
static int q16(float x) { return x <= 0.0f ? 0 : x >= 1.0f ? 16 : (int)(x * 16.0f + 0.5f); }
static int blink(void) { return (int)((plat_ms() / 160u) & 1u); }

/* the header: the piece on a hazard-orange plate, the root and the mode (and the root on its way, while
 * it waits for the bar); on the right, the tempo with a beat light (a play triangle while stopped) and a
 * dot while there is something unsaved. Messages take its place for a moment. */
static void draw_header(void)
{
    char t[24];
    int msg = plat_ms() < ui.msg_until, pend = (en_pending & EN_PEND_ROOT) != 0;
    uint32_t h = hash(hash(hash(2166136261u, proj.piece | (uint32_t)proj.par[P_ROOT] << 8 | (uint32_t)en_root_now << 12),
                           en_playing | ui.dirty << 4 | (uint32_t)msg << 5 | (uint32_t)en_ext << 6 | (uint32_t)proj.par[P_MODE] << 8),
                      (uint32_t)proj.par[P_TEMPO] | (ui.view == V_SETUP ? plat_cpu_pct() << 8 | 1u << 30 : 0u) |
                          (uint32_t)pend << 20);
    if (msg)
        h = hash(hash(h, (uint32_t)ui.msg[0][0] << 8 | ui.msg[0][1]), ui.msg_until);
    if (en_playing)
        h = hash(h, (en_bars / 4u) & 1u);
    if (pend)
        h = hash(h, (uint32_t)blink());
    if (h == ui.sig[0])
        return;
    ui.sig[0] = h;
    cv_begin(240, HDR_H, K_BG);
    if (msg) {
        int32_t x = cv_text(10, 6, &FONT_B, ui.msg[0], K_TEXT);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], K_ACC);
    } else {
        int32_t x = 232, w;
        if (ui.dirty) {                            /* unsaved */
            cv_round(x - 5, 12, 5, 5, 2, K_DIM);
            x -= 11;
        }
        if (en_ext) {
            t[0] = 'E';
            t[1] = 'X';
            t[2] = 'T';
            t[3] = 0;
        } else {
            itoa_u((uint32_t)proj.par[P_TEMPO], t);
        }
        if (ui.view == V_SETUP) {                 /* Setup: the audio load too */
            char c[12] = "CPU ";
            itoa_u(plat_cpu_pct(), c + 4);
            x -= text_w(&FONT_XS, c);
            cv_text(x, 8, &FONT_XS, c, K_DIM);
            x -= 10;
        }
        x -= text_w(&FONT_S, t);
        cv_text(x, 6, &FONT_S, t, en_playing ? K_TEXT : K_DIM);
        x -= 16;
        if (en_playing) {                         /* the beat light */
            int on = ((en_bars / 4u) & 1u) == 0u;
            cv_round(x, 9, 10, 10, 2, on ? K_ACC : K_PANEL);
        } else {
            int32_t j;                            /* stopped: a play triangle */
            for (j = 0; j < 5; j++)
                cv_rect(x + 2 + j, 9 + j, 1, 11 - 2 * j, K_DIM);
        }
        caps(t, piece_name(proj.piece), sizeof t);
        w = text_w(&FONT_B, t) + 12;              /* the piece, on its plate */
        cv_round(4, 4, w, 20, 3, K_ACC);
        cv_text(10, 6, &FONT_B, t, K_BG);
        w = cv_text(w + 10, 8, &FONT_XS, EN_NOTE_NAME[en_root_now % 12u], K_TEXT);
        if (pend) {                               /* on its way: the new root, blinking */
            w = cv_text(w + 2, 8, &FONT_XS, ">", K_WARN);
            w = cv_text(w + 2, 8, &FONT_XS, EN_NOTE_NAME[proj.par[P_ROOT]], blink() ? K_WARN : K_DIM);
        }
        cv_text(w + 4, 8, &FONT_XS, EN_MODE_NAME[proj.par[P_MODE]], K_DIM);
    }
    cv_blit(0, 0);
}

/* the machine: the section and where the song is; the bar ahead, every row, its playhead; the layers */
#define GRID_X 34
#define GRID_Y 32
#define ROW_H 13
static int32_t cell_x(int s) { return GRID_X + s * 11 + (s / 4) * 2; }

static void draw_section(void)
{
    char t[16], b[12];
    int32_t x;
    caps(t, EN_SECT_NAME[en_sect], sizeof t);
    x = cv_text(6, 1, &FONT_B, t, SECT_C[en_sect]);
    if (en_sect_len) {                           /* bar 5 of 16 (or looping) */
        itoa_u((uint32_t)en_sect_bar + 1u, b);
        x = cv_text(x + 6, 4, &FONT_XS, b, K_TEXT);
        x = cv_text(x, 4, &FONT_XS, "/", K_DIM);
        itoa_u(en_sect_len, b);
        x = cv_text(x, 4, &FONT_XS, b, K_DIM);
    } else {
        x = cv_text(x + 6, 4, &FONT_XS, "LOOP", K_DIM);
    }
    {   /* on the right: what waits for the bar, else the chord */
        const char *w = 0;
        uint16_t c = blink() ? K_WARN : K_DIM;
        if (en_pending & EN_PEND_BREAK)
            w = en_sect == S_BREAK ? "> BUILD" : en_sect == S_BUILD ? "> DROP" : "> BREAKDOWN";
        else if (en_pending & EN_PEND_NEXT)
            w = "> NEXT";
        else if (en_pending & EN_PEND_MUTATE)
            w = "> MUTATE";
        if (w) {
            cv_text(234 - text_w(&FONT_XS, w), 4, &FONT_XS, w, c);
        } else {
            x = 234 - text_w(&FONT_B, EN_NOTE_NAME[en_chord_root % 12u]);
            cv_text(x, 1, &FONT_B, EN_NOTE_NAME[en_chord_root % 12u], K_TEXT);
            cv_text(x - 6 - text_w(&FONT_XS, "ON"), 4, &FONT_XS, "ON", K_DIM);
        }
    }
    {   /* the song: a block a section, as long as it lasts; this one lit, filling */
        int i, tot = 0, n = en_plan_n;
        int32_t xx = 6, wall = 228;
        for (i = 0; i < n; i++)
            tot += en_plan_len[i] ? en_plan_len[i] : 8;
        for (i = 0; i < n; i++) {
            int len = en_plan_len[i] ? en_plan_len[i] : 8, cur = i == en_plan_pos && !en_jumped;
            int32_t w = (int32_t)(len * (wall - 2 * (n - 1)) / (tot ? tot : 1));
            uint16_t c = SECT_C[en_plan[i]];
            if (w < 3)
                w = 3;
            cv_rect(xx, 20, w, 5, cur ? mix(c, K_BG, 6) : mix(c, K_BG, 11));
            if (cur) {
                int32_t f = en_sect_len ? (int32_t)((en_sect_bar + 1) * w / en_sect_len) : w;
                cv_rect(xx, 20, f, 5, c);
                cv_rect(xx, 26, w, 1, K_TEXT);
            } else if (i == en_plan_pos && en_jumped) {
                cv_rect(xx, 26, w, 1, K_DIM);     /* left for a jump: comes back after it */
            }
            xx += w + 2;
        }
        if (en_jumped) {                         /* a jump plays outside the plan: its colour at the end */
            cv_rect(234 - 4, 18, 4, 9, SECT_C[en_sect]);
        }
    }
}

static void draw_grid(void)
{
    int r, s, step = en_playing ? en_step : -1;
    if (step >= 0)                               /* the playhead's column */
        cv_rect(cell_x(step) - 1, GRID_Y - 1, 12, EN_NROW * ROW_H + 1, K_PANEL);
    for (r = 0; r < EN_NROW; r++) {
        int l = ROW_LAYER[r], muted = (en_muted >> l & 1u) != 0, active = (en_active >> l & 1u) != 0;
        int32_t y = GRID_Y + r * ROW_H;
        uint16_t c = LAYER_C[l], lc = muted ? K_RED : active ? mix(K_DIM, c, q16(en_glow[r]) / 2 + 6) : K_LINE;
        cv_text(6, y - 1, &FONT_XS, ROW_NAME[r], lc);
        for (s = 0; s < EN_NSTEP; s++) {
            int v = en_grid_vel[r][s];
            int32_t x = cell_x(s);
            if (v) {
                uint16_t cc = mix(K_LINE, c, 6 + v * 10 / 127);
                if (muted)
                    cc = mix(K_LINE, K_RED, 5);
                else if (s == step)
                    cc = mix(c, K_TEXT, 8);
                cv_rect(x, y + 1, 10, ROW_H - 3, cc);
            } else {
                cv_rect(x + 4, y + 5, 2, 2, (s & 3) ? K_LINE : K_DIM);
            }
        }
    }
}

static void draw_layers(void)                   /* the bottom: the six layers, playing, resting or muted */
{
    static const char *const SHORT[EN_NLAYER] = {"DRM", "BAS", "GTR", "SYN", "MTL", "NOI"};
    int l;
    for (l = 0; l < EN_NLAYER; l++) {
        int32_t x = 6 + l * 38, y = 126;
        int muted = (en_muted >> l & 1u) != 0, active = (en_active >> l & 1u) != 0;
        int32_t w = (int32_t)(fm_clampf(en_level[l], 0.0f, 1.0f) * 32.0f + 0.5f);
        cv_text(x, y - 2, &FONT_XS, SHORT[l], muted ? K_RED : active ? K_TEXT : K_DIM);
        cv_rect(x, y + 12, 34, 3, K_LINE);
        if (muted)
            cv_rect(x, y + 12, 34, 3, mix(K_LINE, K_RED, 8));
        else if (w > 0)
            cv_rect(x, y + 12, w, 3, LAYER_C[l]);
    }
}

/* a button's card (help_tick): its name, then what it does, in the machine's place */
#define HELP_LINES 4
static const char *const BTN_LABEL[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                          "PLAY", "REC", "OCT-", "OCT+"};
static int help_lines(int b, const char *l[HELP_LINES])
{
    int n = 0;
#define L(t) (l[n++] = (t))
    switch (b) {
    case B_HOME: L("THE MACHINE"); L("TEMPO, ARRANGE, EVOLVE,"); L("INTENSITY"); L("+ SELECT: TEMPO"); break;
    case B_SEQ: L("THE DRUMS"); L("LEVEL, GROOVE, CRUSH, KICK"); L("EDIT + SEQ: MUTE"); break;
    case B_LFO: L("THE BASS"); L("LEVEL, CUTOFF, SQUELCH,"); L("DRIVE"); L("EDIT + LFO: MUTE"); break;
    case B_ENV: L("THE GUITAR"); L("LEVEL, GAIN, CHUG, RIFF"); L("EDIT + ENV: MUTE"); break;
    case B_ARP: L("THE SYNTH"); L("LEVEL, TONE, DECAY, BUSY"); L("EDIT + ARP: MUTE"); break;
    case B_SEL: L("METAL AND NOISE"); L("METAL, HITS, NOISE,"); L("TEXTURE"); L("EDIT + SEL: MUTE"); break;
    case B_FX: L("THE SPACE"); L("REVERB, GATE, DELAY, PUMP"); break;
    case B_GLO: L("SETUP"); L("ROOT, TUNE, MIDI OUT, LIGHTS"); break;
    case B_EDIT: L("THE NEXT SECTION:"); L("THE BAR FILLS, THEN IT"); L("HOLD + A LAYER'S PAGE"); L("BUTTON: MUTE IT"); break;
    case B_REC: L("A BREAKDOWN, ON THE BAR"); L("AGAIN: THE BUILD"); L("AGAIN: THE DROP"); break;
    case B_OCTDN: L("A FILL: THE REST"); L("OF THIS BAR"); L("WITH OCT+, 5 S: UPDATE MODE"); break;
    case B_OCTUP: L("MUTATE: NEW RIFFS AND"); L("LINES FROM THE NEXT BAR"); L("WITH OCT-, 5 S: UPDATE MODE"); break;
    case B_SAVE: L("SAVES ALL EIGHT PIECES"); L("KEEP HOLDING: THE PIECE"); L("BACK TO ITS RECIPE"); break;
    default: break;
    }
#undef L
    return n;
}

static void draw_help(void)
{
    const char *l[HELP_LINES];
    int n = help_lines(ui.help - 1, l), k;
    cv_round(12, 10, 216, 124, 4, K_PANEL);
    cv_round(12, 10, 216, 26, 4, K_ACC);
    cv_rect(12, 30, 216, 6, K_ACC);
    text_c(120, 14, &FONT_B, BTN_LABEL[ui.help - 1], K_BG);
    for (k = 0; k < n; k++)
        text_c(120, 48 + k * 20, &FONT_S, l[k], K_TEXT);
}

static void draw_main(void)
{
    uint32_t h = hash(2166136261u, (uint32_t)en_sect | (uint32_t)en_sect_bar << 4 | (uint32_t)en_sect_len << 12 |
                                       (uint32_t)en_playing << 20 | (uint32_t)en_jumped << 21 | (uint32_t)en_plan_pos << 24),
             i, s;
    if (ui.frame % 2u)                           /* 30 frames a second is plenty */
        return;
    if (ui.help) {
        h = hash(0x48454C50u, ui.help);
        if (h != ui.sig[1]) {
            ui.sig[1] = h;
            cv_begin(240, MAIN_H, K_BG);
            draw_help();
            cv_blit(0, MAIN_Y);
        }
        return;
    }
    h = hash(h, (uint32_t)en_step | (uint32_t)en_active << 8 | (uint32_t)en_muted << 16 | (uint32_t)en_pending << 24);
    h = hash(h, (uint32_t)en_chord_root | (uint32_t)en_plan_n << 8 | (uint32_t)(en_pending ? blink() : 0) << 16);
    for (i = 0; i < EN_NROW; i++) {
        h = hash(h, en_grid[i] | (uint32_t)q16(en_glow[i]) << 16);
        for (s = 0; s < EN_NSTEP; s++)
            h = hash(h, en_grid_vel[i][s]);
    }
    for (i = 0; i < EN_NLAYER; i++)
        h = hash(h, (uint32_t)(en_level[i] * 32.0f));
    for (i = 0; i < en_plan_n && i < EN_PLAN_MAX; i++)
        h = hash(h, (uint32_t)en_plan[i] | (uint32_t)en_plan_len[i] << 8);
    if (h == ui.sig[1])
        return;
    ui.sig[1] = h;
    cv_begin(240, MAIN_H, K_BG);
    draw_section();
    draw_grid();
    draw_layers();
    cv_blit(0, MAIN_Y);
}

/* the knob strip (X0X's): a rule with the page's name and eight page dots at its right end; under
 * it a cell a knob: its name, a ring (or pips, for a switch of a few positions), the value. The
 * knob being turned is drawn on a panel. Only redrawn when what it shows changes (the rings cost a
 * square root and an arctangent per pixel). */
static void draw_knobs(void)
{
    uint32_t h = hash(2166136261u, ui.view | (uint32_t)ui.touched << 8 | (uint32_t)en_muted << 16), i;
    int now_touch = plat_ms() < ui.touch_until;
    for (i = 0; i < 4; i++)
        h = hash(h, (uint32_t)knob_get(VIEW_KNOB[ui.view][i]));
    h = hash(h, (uint32_t)now_touch);
    if (h == ui.sig[2])
        return;
    ui.sig[2] = h;
    cv_begin(240, KNB_H, K_BG);
    {   /* the rule; at its right end, where you are: the page (MUTED if it is), then the dots */
        char t[16];
        int k, muted = VIEW_MUTE[ui.view] && (en_muted & VIEW_MUTE[ui.view]) == VIEW_MUTE[ui.view];
        int32_t xd = 237 - NVIEWS * 6, xt;
        caps(t, VIEW_NAME[ui.view], sizeof t);
        xt = xd - 5 - text_w(&FONT_XS, t);
        if (muted)
            xt -= text_w(&FONT_XS, "MUTED") + 6;
        cv_rect(4, 5, xt - 9, 1, K_LINE);
        xt = cv_text(xt, -1, &FONT_XS, t, K_ACC);
        if (muted)
            cv_text(xt + 6, -1, &FONT_XS, "MUTED", K_RED);
        for (k = 0; k < NVIEWS; k++)
            cv_round(xd + k * 6, 3, 4, 4, 1, k == ui.view ? K_ACC : K_LINE);
    }
    for (i = 0; i < 4; i++) {
        int k = VIEW_KNOB[ui.view][i], lo, hi, v, hot = now_touch && ui.touched == (int)i;
        int32_t x = (int32_t)i * 60, cx = x + 30;
        char t[16];
        uint16_t bg = hot ? K_PANEL : K_BG, fill = hot ? K_ACC : K_TEXT;
        if (k == K_NONE)
            continue;
        if (i)
            cv_rect(x, 16, 1, 46, K_LINE);
        if (hot)
            cv_round(x + 3, 11, 54, 57, 4, K_PANEL);
        knob_range(k, &lo, &hi);
        v = knob_get(k);
        caps(t, KNOB_LABEL[ui.view][i], sizeof t);
        text_c(cx, 12, &FONT_XS, t, hot ? K_TEXT : K_DIM);
        if (k != K_CPU && hi - lo < 6) {         /* a switch: its positions as pips */
            int n = hi - lo + 1, j, pw = n > 4 ? 6 : 9, gap = 3, w0 = n * (pw + gap) - gap;
            for (j = 0; j < n; j++)
                cv_round(cx - w0 / 2 + j * (pw + gap), 33, pw, 8, 1, j == v - lo ? fill : K_LINE);
        } else if (k != K_CPU) {
            arc(cx, 37, 12, hi > lo ? (float)(v - lo) / (float)(hi - lo) : 0.0f, K_LINE, fill, bg, lo < 0);
        }
        knob_text(k, t);
        text_c(cx, 50, &FONT_B, t, hot ? K_ACC : K_TEXT);
    }
    cv_blit(0, KNB_Y);
}

static void leds(void)
{
    uint32_t b = 0, k = 0, i;
    const int bl = blink();
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
    if (proj.leds)                               /* the keys: the root's; the new one blinks on its way */
        for (i = 0; i < NKEYS; i++) {
            uint32_t pc = (5u + i) % 12u;
            if (pc == en_root_now || (pc == (uint32_t)proj.par[P_ROOT] && bl))
                k |= 1u << i;
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
    lcd_fill(0, 0, 240, 240, K_BG);
    if (!en_playing)
        en_play(1);                              /* it runs from the start */
}

void ui_frame(void)
{
    input();
    autosave();
    draw_header();
    draw_main();
    draw_knobs();
    leds();
    ui.frame++;
}

void ui_input_only(void) { input(); }
