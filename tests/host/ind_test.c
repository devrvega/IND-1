/* SPDX-License-Identifier: GPL-3.0-only */
/* The instrument on the host: the pure parts (Euclid, scales, degrees, section lengths) and the render
 * (sound, no NaN, bounded; the arranger moves through a song; the root moves on the bar; mutes, fills,
 * the breakdown and the next section; every layer sounds alone; every setting at its extremes). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "ind.h"

static int fails;
#define CHECK(c, ...)                                                                                              \
    do {                                                                                                           \
        if (!(c)) {                                                                                                \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                            \
            printf(__VA_ARGS__);                                                                                   \
            printf("\n");                                                                                          \
            fails++;                                                                                               \
        }                                                                                                          \
    } while (0)

static uint32_t midi_count, midi_drum, midi_bass, midi_gtr, midi_syn;
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2)
{
    (void)d1;
    midi_count++;
    if ((st & 0xF0u) == 0x90u && d2) {
        uint32_t ch = st & 15u;
        midi_drum += ch == 9u;
        midi_bass += ch == 1u;
        midi_gtr += ch == 2u;
        midi_syn += ch == 0u;
    }
}

static uint32_t pat(const char *s)
{
    uint32_t m = 0;
    int i;
    for (i = 0; s[i]; i++)
        if (s[i] == 'x')
            m |= 1u << i;
    return m;
}

static int popcount(uint32_t m)
{
    int c = 0;
    while (m) {
        c += (int)(m & 1u);
        m >>= 1;
    }
    return c;
}

static int32_t buf[2 * 256];
static float peak, sum2;
static long frames;
static int bad;
static void run(float secs)
{
    int blocks = (int)(secs * 44100.0f / 256.0f), b, j;
    for (b = 0; b < blocks; b++) {
        en_render(buf, 256, 2048);
        for (j = 0; j < 512; j++) {
            float x = (float)buf[j] / 8388608.0f;
            if (x != x)
                bad++;
            if (fabsf(x) > peak)
                peak = fabsf(x);
            sum2 += x * x;
        }
        frames += 256;
    }
}
static void reset_meter(void)
{
    peak = sum2 = 0.0f;
    frames = 0;
}
static float rms(void) { return frames ? sqrtf(sum2 / (float)(frames * 2)) : 0.0f; }

/* run in blocks until the section is s (or something else is true), at most secs */
#define RUN_UNTIL(cond, secs)                                                                                      \
    do {                                                                                                           \
        int k_;                                                                                                    \
        for (k_ = 0; k_ < (int)((secs) * 44100.0f / 256.0f) && !(cond); k_++)                                     \
            run(256.0f / 44100.0f + 1e-6f);                                                                        \
    } while (0)

int main(void)
{
    int i;

    /* Euclid: the classic patterns */
    CHECK(en_euclid(3, 8, 0) == pat("x..x..x."), "E(3,8) %x", en_euclid(3, 8, 0));
    CHECK(en_euclid(5, 8, 0) == pat("x.xx.xx."), "E(5,8) %x", en_euclid(5, 8, 0));
    CHECK(en_euclid(4, 16, 0) == pat("x...x...x...x..."), "E(4,16)");
    CHECK(en_euclid(0, 16, 0) == 0, "E(0,16)");
    for (i = 0; i <= 12; i++)
        CHECK(popcount(en_euclid(i, 12, 5)) == i, "E(%d,12) has %d hits", i, popcount(en_euclid(i, 12, 5)));

    /* scales and degrees */
    {
        int st[12];
        CHECK(en_scale(EN_MODE_POOL[MODE_PHRYGIAN], st) == 7 && st[1] == 1 && st[4] == 7, "Phrygian: the b2, the fifth");
        CHECK(en_scale(EN_MODE_POOL[MODE_LOCRIAN], st) == 7 && st[4] == 6, "Locrian: the b5");
        CHECK(en_scale(EN_MODE_POOL[MODE_BLUES], st) == 6, "blues has 6");
        CHECK(en_scale(EN_MODE_POOL[MODE_OCTATONIC], st) == 8, "octatonic has 8");
        CHECK(en_degree(EN_MODE_POOL[MODE_PHRYGIAN], 0) == 0, "degree 0");
        CHECK(en_degree(EN_MODE_POOL[MODE_PHRYGIAN], 1) == 1, "Phrygian degree 1: the b2");
        CHECK(en_degree(EN_MODE_POOL[MODE_PHRYGIAN], 7) == 12, "degree 7: the octave");
        CHECK(en_degree(EN_MODE_POOL[MODE_PHRYGIAN], -1) == -2, "degree -1: the b7 below (%d)",
              en_degree(EN_MODE_POOL[MODE_PHRYGIAN], -1));
        CHECK(en_degree(EN_MODE_POOL[MODE_BLUES], 6) == 12, "blues degree 6: the octave");
        CHECK(en_degree(EN_MODE_POOL[MODE_AEOLIAN], -8) == -14, "degree -8 (%d)", en_degree(EN_MODE_POOL[MODE_AEOLIAN], -8));
        for (i = 0; i < EN_NMODES; i++)
            CHECK((EN_MODE_POOL[i] & 1u) && !(EN_MODE_POOL[i] >> 12), "mode %d: the root, twelve notes", i);
    }

    /* the arranger's lengths */
    CHECK(en_sect_bars(S_VERSE, A_NORMAL) == 16 && en_sect_bars(S_VERSE, A_LONG) == 32 && en_sect_bars(S_VERSE, A_SHORT) == 8,
          "verse lengths");
    CHECK(en_sect_bars(S_VERSE, A_LOOP) == 0 && en_sect_bars(S_CHORUS, A_LOOP) == 0, "loop holds the verse and the chorus");
    CHECK(en_sect_bars(S_BUILD, A_LOOP) == 4 && en_sect_bars(S_INTRO, A_LOOP) == 8, "loop: the intro and the build lead on");
    CHECK(en_sect_bars(S_BUILD, A_SHORT) == 2, "a short build is 2 bars");

    /* texts */
    {
        char t[12];
        en_param_text(P_ARRANGE, A_LOOP, t);
        CHECK(!strcmp(t, "Loop"), "arrange: %s", t);
        en_param_text(P_GROOVE, G_THRASH, t);
        CHECK(!strcmp(t, "Thrash"), "groove: %s", t);
        en_param_text(P_TUNE, 7, t);
        CHECK(!strcmp(t, "+7"), "tune: %s", t);
        en_param_text(P_MODE, MODE_PHRYG_DOM, t);
        CHECK(!strcmp(t, "Phryg dom"), "mode: %s", t);
        for (i = 0; i < EN_NPIECES; i++) {
            int16_t p[P_NPARAMS];
            int k;
            en_piece(i, p);
            for (k = 0; k < P_NPARAMS; k++)
                CHECK(p[k] >= en_param_info(k)->lo && p[k] <= en_param_info(k)->hi, "piece %s: %s = %d out of range",
                      en_piece_name(i), en_param_info(k)->name, p[k]);
        }
    }

    /* the render: silent at rest, sound when playing, never NaN, bounded */
    en_init();
    run(1.0f);
    CHECK(peak == 0.0f, "silent before play (%f)", (double)peak);
    CHECK(en_sect == S_INTRO, "it starts with an intro");
    en_play(1);
    reset_meter();
    run(30.0f);
    CHECK(!bad, "NaN in the output");
    CHECK(peak > 0.1f && peak < 0.995f, "peak %f", (double)peak);
    printf("  30 s playing: peak %.3f, rms %.4f, bars %u, section %s bar %u\n", (double)peak, (double)rms(), en_bars / 16u,
           EN_SECT_NAME[en_sect], en_sect_bar);
    CHECK(en_bars / 16u >= 14u, "the grid ran (%u sixteenths)", en_bars);
    CHECK(en_sect != S_INTRO, "the intro led on (%s)", EN_SECT_NAME[en_sect]);

    /* a whole song: the arranger plays its plan through, a new song after it */
    {
        int seen = 0, k;
        uint16_t song0 = en_song;
        en_set(P_ARRANGE, A_SHORT);
        en_set(P_TEMPO, 180);
        for (k = 0; k < 4 * 60; k++) {
            run(0.25f);
            seen |= 1 << en_sect;
        }
        CHECK(seen == (1 << EN_NSECT) - 1, "every section heard in four minutes (seen %x)", seen);
        CHECK(en_song != song0, "a new song began");
        CHECK(!bad && peak < 0.995f, "bounded through a song (peak %f)", (double)peak);
    }

    /* the root moves on the bar, not before */
    {
        int k, moved_at = -1;
        (void)k;
        uint32_t b0;
        en_set(P_ARRANGE, A_LOOP);
        en_set(P_TEMPO, 120);
        run(0.3f);
        while ((en_bars & 15u) != 4u)
            run(0.01f);
        b0 = en_bars;
        en_set(P_ROOT, 9);
        run(0.02f);
        CHECK(en_root_now != 9 && (en_pending & EN_PEND_ROOT), "the new root waits for the bar (now %d)", en_root_now);
        for (k = 0; k < 2000 && moved_at < 0; k++) {
            run(256.0f / 44100.0f + 1e-6f);
            if (en_root_now == 9)
                moved_at = (int)(en_bars - 1u);
        }
        CHECK(moved_at >= 0 && (moved_at & 15) == 0, "the root moved on a downbeat (%d, from %u)", moved_at, b0);
        CHECK(!(en_pending & EN_PEND_ROOT), "nothing pending after");
    }

    /* the next section and the breakdown, on the bar; the breakdown leads to a build and a drop */
    {
        int s0;
        en_set(P_ARRANGE, A_LOOP);
        run(2.0f);
        s0 = en_sect;
        en_break();
        run(0.01f);
        CHECK(en_pending & EN_PEND_BREAK, "the breakdown is pending");
        RUN_UNTIL(en_sect == S_BREAK, 3.0f);
        CHECK(en_sect == S_BREAK, "a breakdown (from %s)", EN_SECT_NAME[s0]);
        run(6.0f);
        CHECK(en_sect == S_BREAK, "looping: the breakdown holds");
        en_break();
        RUN_UNTIL(en_sect == S_BUILD, 3.0f);
        CHECK(en_sect == S_BUILD, "REC in a breakdown: the build (%s)", EN_SECT_NAME[en_sect]);
        RUN_UNTIL(en_sect != S_BUILD, 20.0f);
        CHECK(en_sect == S_DROP, "after the build, the drop (%s)", EN_SECT_NAME[en_sect]);
        s0 = en_plan_pos;
        en_next();
        RUN_UNTIL(en_sect != S_DROP, 3.0f);
        CHECK(en_sect != S_DROP, "the next section");
        CHECK(en_plan_pos == (uint8_t)(s0 + 1) || en_plan_pos == 0, "back in the plan, one on (%d from %d)", en_plan_pos, s0);
    }

    /* a fill: the rest of the bar */
    {
        int k;
        en_set(P_ARRANGE, A_LOOP);
        while (en_sect == S_INTRO || en_sect == S_BREAK || en_sect == S_BUILD) {
            en_next();
            run(2.5f);
        }
        while ((en_bars & 15u) != 2u)
            run(0.01f);
        en_fill();
        run(0.01f);
        CHECK(en_pending & EN_PEND_FILL, "a fill in this bar");
        for (k = 8; k < 16; k++)
            CHECK(!(en_grid[EN_ROW_HAT] >> k & 1u), "no hats in the fill (step %d)", k);
    }

    /* mutes: a layer muted is silent; MIDI stops too */
    {
        uint32_t d0;
        en_set(P_ARRANGE, A_LOOP);
        en_mute(LY_DRUMS, 1);
        run(0.5f);
        d0 = midi_drum;
        run(3.0f);
        CHECK(en_muted == 1u << LY_DRUMS, "muted %x", en_muted);
        CHECK(midi_drum == d0, "a muted layer plays no notes (%u)", midi_drum - d0);
        en_mute(LY_DRUMS, 0);
        run(3.0f);
        CHECK(midi_drum > d0, "unmuted, it plays again");
    }

    /* mutate: new material */
    {
        uint8_t g0[EN_NROW][EN_NSTEP];
        int k, s, diff = 0, tries;
        en_set(P_ARRANGE, A_LOOP);
        while (en_sect != S_CHORUS && en_sect != S_VERSE && en_sect != S_DROP) {
            en_next();
            run(2.5f);
        }
        for (tries = 0; tries < 4 && !diff; tries++) {
            while ((en_bars & 15u) != 1u)
                run(0.01f);
            memcpy(g0, (const void *)en_grid_vel, sizeof g0);
            en_mutate();
            run(0.01f);
            CHECK(en_pending & EN_PEND_MUTATE, "mutate waits for the bar");
            RUN_UNTIL(!(en_pending & EN_PEND_MUTATE), 4.0f);
            run(0.05f);
            for (k = EN_ROW_BASS; k < EN_NROW; k++)
                for (s = 0; s < EN_NSTEP; s++)
                    diff += g0[k][s] != en_grid_vel[k][s];
        }
        CHECK(diff > 0, "mutate changed the bass, the guitar or the synth");
    }

    /* every layer sounds alone, in a chorus (where all may play) */
    {
        int l;
        en_set(P_INTENSITY, 100);
        en_set(P_ARRANGE, A_LOOP);
        en_set(P_REVERB, 0);
        en_set(P_DELAY, 0);
        en_set(P_HITS, 70);
        en_set(P_SBUSY, 70);
        en_set(P_GUITAR, 80);
        while (en_sect != S_CHORUS) {
            en_next();
            run(2.5f);
        }
        for (l = 0; l < EN_NLAYER; l++)
            en_set(EN_LAYER_PARAM[l], 0);
        run(0.1f);
        for (l = 0; l < EN_NLAYER; l++) {
            if (l)
                en_set(EN_LAYER_PARAM[l - 1], 0);
            en_set(EN_LAYER_PARAM[l], 80);
            run(2.0f);
            CHECK(en_active >> l & 1u, "%s turned up from nothing joins the chorus (%x)", EN_LAYER_NAME[l], en_active);
            reset_meter();
            run(4.0f);
            printf("  %-6s alone: peak %.3f, rms %.4f\n", EN_LAYER_NAME[l], (double)peak, (double)rms());
            CHECK(rms() > 0.004f, "%s alone is heard (rms %f)", EN_LAYER_NAME[l], (double)rms());
            CHECK(peak < 0.98f, "%s alone is bounded (peak %f)", EN_LAYER_NAME[l], (double)peak);
        }
        for (l = 0; l < EN_NLAYER; l++)
            en_set(EN_LAYER_PARAM[l], en_param_info(EN_LAYER_PARAM[l])->def);
    }

    /* MIDI out: every channel had notes */
    CHECK(midi_drum > 50u && midi_bass > 20u && midi_gtr > 5u && midi_syn > 5u, "MIDI out: drums %u bass %u guitar %u synth %u",
          midi_drum, midi_bass, midi_gtr, midi_syn);

    /* every setting at its extremes: still bounded */
    {
        int p;
        for (p = 0; p < P_NPARAMS; p++)
            en_set(p, en_param_info(p)->hi);
        reset_meter();
        run(30.0f);
        CHECK(!bad && peak < 0.999f, "all at max: peak %f", (double)peak);
        for (p = 0; p < P_NPARAMS; p++)
            en_set(p, en_param_info(p)->lo);
        run(10.0f);
        CHECK(!bad, "all at min: NaN");
        for (p = 0; p < P_NPARAMS; p++)
            en_set(p, en_param_info(p)->def);
    }

    /* every piece plays, bounded */
    for (i = 0; i < EN_NPIECES; i++) {
        int16_t p[P_NPARAMS];
        int k;
        en_piece(i, p);
        for (k = 0; k < P_NPARAMS; k++)
            en_set(k, p[k]);
        en_regrow();
        reset_meter();
        run(25.0f);
        printf("  %-10s peak %.3f, rms %.4f (%s)\n", en_piece_name(i), (double)peak, (double)rms(), EN_SECT_NAME[en_sect]);
        CHECK(!bad && peak < 0.995f && rms() > 0.02f, "piece %s: peak %f rms %f", en_piece_name(i), (double)peak, (double)rms());
    }

    /* stop: the tails die away */
    en_play(0);
    run(8.0f);
    reset_meter();
    run(1.0f);
    CHECK(peak < 0.01f, "quiet after stopping (%f)", (double)peak);

    printf(fails ? "ind: %d FAILED\n" : "ind: ok\n", fails);
    return fails != 0;
}
