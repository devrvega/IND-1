/* SPDX-License-Identifier: GPL-3.0-only */
/* IND-1's quest: the song as a journey through a dark keep, on an LCD of the old handheld kind (near-black
 * glass, ember segments, every pose faintly there behind the lit one). Each section is a place: the
 * intro the gate, a verse the halls, a build the tower stair, a chorus a battle, the breakdown a campfire,
 * the drop the Warden (an iron giant with a furnace for a heart), the outro the dawn. The layers move the
 * wanderer: the drums the sword, the bass the footsteps, the guitar the torches, the synth the magic, the
 * metal the sparks, the noise the mist. Drawn on a grid of LCD pixels (2 x 2 screen pixels). All the art
 * is IND-1's own. Part of ui.c. */

#define SCX 4                          /* the scene's corner on the screen */
#define SCY 27
#define AW 116                         /* its size, in LCD pixels */
#define AH 46
#define FEET 42                        /* the ground the wanderer stands on */

typedef struct {
    uint32_t ms, frame;
    int playing, step, sect, part;
    float prog;                        /* through the section, 0..1 (a looping one: through four bars) */
    uint32_t ago[EN_NROW];             /* ms since each row last played */
    uint8_t cnt[EN_NROW];              /* and how many times */
    int tom;                           /* the last drum was a tom */
    float noise;                       /* the noise layer's level */
    int sect_bar, sect_len;
} scn_t;

/* a scene is drawn twice: the print and the ghosts first, then the lit segments over them (a ghost never
 * hides a lit segment it overlaps). The caption owns the rows above ART_Y. */
#define ART_Y 8
static int scn_pass;
static int pass_ok(uint16_t c)
{
    int lit = c == L_INK || c == L_HOT || c == L_MID;
    return scn_pass ? lit : !lit;
}
static inline uint16_t col(int on) { return on ? L_INK : L_GHOST; }
static void cp(int x, int y, uint16_t c)
{
    if ((unsigned)x < AW && y >= ART_Y && y < AH && pass_ok(c))
        cv_rect(SCX + 2 * x, SCY + 2 * y, 2, 2, c);
}
static void ap(int x, int y, int on) { cp(x, y, col(on)); }
static void cbox(int x, int y, int w, int h, uint16_t c)
{
    if (y < ART_Y) {
        h -= ART_Y - y;
        y = ART_Y;
    }
    if (h > 0 && pass_ok(c))
        cv_rect(SCX + 2 * x, SCY + 2 * y, 2 * w, 2 * h, c);
}
static void abox(int x, int y, int w, int h, int on) { cbox(x, y, w, h, col(on)); }
static void pbox(int x, int y, int w, int h) { cbox(x, y, w, h, L_PRINT); }
static void cline(int x0, int y0, int x1, int y1, uint16_t c)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1, dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, guard = 400;
    while (guard--) {
        int e2 = 2 * err;
        cp(x0, y0, c);
        if (x0 == x1 && y0 == y1)
            break;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}
static void aline(int x0, int y0, int x1, int y1, int on) { cline(x0, y0, x1, y1, col(on)); }
static void pline(int x0, int y0, int x1, int y1) { cline(x0, y0, x1, y1, L_PRINT); }
static void climb(int x0, int y0, int x1, int y1, uint16_t c)   /* a limb: two thick */
{
    cline(x0, y0, x1, y1, c);
    cline(x0 + 1, y0, x1 + 1, y1, c);
}
static void carc(int cx, int cy, int r, float a0, float a1, uint16_t c)   /* turns: 0 right, 0.25 down */
{
    int i, n = r * 4 + 4;
    for (i = 0; i <= n; i++) {
        float a = (a0 + (a1 - a0) * (float)i / (float)n) * FM_TWO_PI;
        cp(cx + (int)fm_floorf(fm_cosf(a) * (float)r + 0.5f), cy + (int)fm_floorf(fm_sinf(a) * (float)r + 0.5f), c);
    }
}
static void cdisc(int cx, int cy, int r, uint16_t c)
{
    int y;
    for (y = -r; y <= r; y++) {
        if (cy + y < ART_Y || !pass_ok(c))
            continue;
        int w = 0;
        while ((w + 1) * (w + 1) + y * y <= r * r + r)
            w++;
        cv_rect(SCX + 2 * (cx - w), SCY + 2 * (cy + y), 2 * (2 * w + 1), 2, c);
    }
}
static void cart(int x, int y, const char *const *a, uint16_t c, int mirror)
{
    int r, i;
    for (r = 0; a[r]; r++) {
        int w = 0;
        while (a[r][w])
            w++;
        for (i = 0; i < w; i++)
            if (a[r][i] == '#')
                cp(mirror ? x + w - 1 - i : x + i, y + r, c);
    }
}
static void art(int x, int y, const char *const *a, int on) { cart(x, y, a, col(on), 0); }
static void partp(int x, int y, const char *const *a) { cart(x, y, a, L_PRINT, 0); }
static int hot(uint32_t ago, uint32_t ms) { return ago < ms; }

/* ------------------------------------------------------------ the wanderer --- */
/* a helmed wanderer with a round shield, facing right: the body is one piece, the legs, the sword, the
 * magic and the sparks are segments */
static const char *const W_BODY[] = {
    "..####...",
    ".######..",
    ".####..#.",
    ".#######.",
    "..#####..",
    "...###...",
    ".#######.",
    "##.###.##",
    "##.###.##",
    "...###...",
    "..#####..", 0};
static const char *const W_SHIELD[] = {".##.", "####", "####", "####", ".##.", 0};
static const char *const SPARK[] = {".#.", "###", ".#.", 0};
static const char *const STAR[] = {"#.#", ".#.", "#.#", 0};

enum { LEGS_STAND, LEGS_STRIDE, LEGS_SIT };
enum { SWORD_REST, SWORD_UP, SWORD_SLASH };
/* the wanderer with feet at (x, fy): every pose there, the ones given lit (on: lit at all) */
static void wanderer(int x, int fy, int legs, int sword, int on)
{
    const int y = fy - 14 + (legs == LEGS_SIT ? 3 : 0);
    int k;
    if (legs == LEGS_SIT) {                      /* sitting: the knees up, the feet before it */
        art(x, y, W_BODY, on);
        climb(x + 3, y + 11, x + 8, y + 10, col(on));
        climb(x + 8, y + 10, x + 9, y + 11, col(on));
        art(x - 3, y + 6, W_SHIELD, on);
        return;
    }
    art(x, y, W_BODY, on);
    art(x - 3, y + 6, W_SHIELD, on);
    /* the legs: standing together, or a stride */
    climb(x + 3, y + 11, x + 3, y + 14, col(on && legs == LEGS_STAND));
    climb(x + 5, y + 11, x + 5, y + 14, col(on && legs == LEGS_STAND));
    climb(x + 3, y + 11, x + 1, y + 14, col(on && legs == LEGS_STRIDE));
    climb(x + 5, y + 11, x + 7, y + 14, col(on && legs == LEGS_STRIDE));
    /* the sword: down at the side, raised, a slash */
    for (k = 0; k < 3; k++) {
        int lit = on && sword == k;
        if (k == SWORD_REST) {
            aline(x + 8, y + 9, x + 12, y + 13, lit);
            ap(x + 8, y + 8, lit);
        } else if (k == SWORD_UP) {
            aline(x + 8, y + 6, x + 10, y - 3, lit);
            aline(x + 7, y + 6, x + 9, y + 6, lit);
        } else {
            aline(x + 10, y + 7, x + 17, y + 7, lit);
            aline(x + 9, y + 6, x + 9, y + 8, lit);
        }
    }
}

/* the drums swing the sword: the kick a slash, the snare (or a tom) raises it */
static int sword_of(const scn_t *s)
{
    if (hot(s->ago[EN_ROW_KICK], 130u) && s->ago[EN_ROW_KICK] <= s->ago[EN_ROW_SNARE])
        return SWORD_SLASH;
    if (hot(s->ago[EN_ROW_SNARE], 130u))
        return SWORD_UP;
    return SWORD_REST;
}
/* the bass walks: a step a note, standing when it rests */
static int legs_of(const scn_t *s)
{
    if (!hot(s->ago[EN_ROW_BASS], 500u))
        return LEGS_STAND;
    return (s->cnt[EN_ROW_BASS] & 1u) ? LEGS_STRIDE : LEGS_STAND;
}

/* the synth: four sparks of magic circling over the wanderer's head */
static void magic(const scn_t *s, int x, int fy)
{
    static const int8_t P[4][2] = {{-1, -19}, {4, -22}, {9, -19}, {4, -16}};
    int k, on = hot(s->ago[EN_ROW_SYNTH], 260u);
    for (k = 0; k < 4; k++)
        art(x + P[k][0], fy + P[k][1], SPARK, on && (s->cnt[EN_ROW_SYNTH] & 3u) == (uint32_t)k);
}

/* the metal: sparks off the blade */
static void sparks(const scn_t *s, int x, int y)
{
    static const int8_t P[3][2] = {{0, 0}, {5, -4}, {6, 3}};
    int k, on = hot(s->ago[EN_ROW_METAL], 160u);
    for (k = 0; k < 3; k++)
        art(x + P[k][0], y + P[k][1], STAR, on && (s->cnt[EN_ROW_METAL] % 3u) == (uint32_t)k);
}

/* the noise: mist along the ground, as much as it is loud, drifting */
static void mist(const scn_t *s)
{
    int i, n = (int)(fm_clampf(s->noise * 2.2f, 0.0f, 1.0f) * 14.0f + 0.5f), d = (int)((s->ms / 400u) % 3u);
    for (i = 0; i < 14; i++) {
        int x = 2 + i * 8 + d;
        abox(x, 44 + (i & 1), 5, 1, i < n || (14 - i) <= n / 2);
    }
}

/* the guitar: a torch, its holder printed, the flame small (always, flickering) or roaring */
static const char *const FLAME_S[] = {".#.", "###", 0};
static const char *const FLAME_B[] = {"..#..", ".###.", "#####", ".###.", 0};
static void torch(const scn_t *s, int x, int y)
{
    int roar = hot(s->ago[EN_ROW_GUITAR], 180u), flick = (int)((s->ms / 120u + (uint32_t)x) & 1u);
    pbox(x, y + 4, 3, 1);
    pbox(x + 1, y + 5, 1, 3);
    art(x - 1, y - 0, FLAME_B, roar);
    art(x, y + 2, FLAME_S, !roar && (flick || !s->playing));
}

/* ------------------------------------------------------------- the places --- */
static void scene_gate(const scn_t *s)          /* the intro: the keep's gate rises as it goes */
{
    int k, open = (int)(s->prog * 6.0f);
    /* the gatehouse: two towers, the arch between */
    pbox(62, 12, 7, 31);
    pbox(103, 12, 7, 31);
    for (k = 0; k < 4; k++) {
        pbox(62 + 2 * k, 10, 1, 2);
        pbox(103 + 2 * k, 10, 1, 2);
    }
    carc(86, 24, 17, 0.5f, 1.0f, L_PRINT);
    pline(69, 24, 69, 42);
    pline(103, 24, 103, 42);
    pline(0, FEET + 1, 115, FEET + 1);
    /* the portcullis: six bands, shut from the bottom up; the intro lifts them */
    for (k = 0; k < 6; k++) {
        int y = 19 + 4 * k, lit = k >= open, b;
        for (b = 0; b < 6; b++)
            abox(72 + 5 * b, y, 1, 4, lit);
        abox(71, y + 3, 31, 1, lit);
    }
    torch(s, 58, 18);
    torch(s, 112, 18);
    /* the kick at the gate: dust at its foot */
    for (k = 0; k < 2; k++)
        art(72 + 24 * k, FEET - 2, STAR, hot(s->ago[EN_ROW_KICK], 140u));
    wanderer(24, FEET, legs_of(s), sword_of(s), 1);
    magic(s, 24, FEET);
    sparks(s, 41, 31);
}

static const char *const BANNER[] = {"#####", "#####", "#####", "#####", "##.##", "#...#", 0};
static const char *const BAT[2][4] = {{"#.....#", "##.#.##", ".#####.", 0}, {"...#...", ".#####.", "##...##", 0}};
static void scene_halls(const scn_t *s)         /* a verse: the halls go by, a step a bass note */
{
    int k, ph = (int)(s->cnt[EN_ROW_BASS] & 1u), bat;
    pline(0, 10, 115, 10);
    pline(0, FEET + 1, 115, FEET + 1);
    for (k = 0; k < 8; k++) {                    /* the floor's flagstones, moving with the steps */
        abox(4 + 15 * k + (ph ? 7 : 0), FEET + 2, 6, 1, 1);
        abox(4 + 15 * k + (ph ? 0 : 7), FEET + 2, 6, 1, 0);
    }
    for (k = 0; k < 4; k++) {                    /* the banners, too */
        art(6 + 30 * k, 11, BANNER, !ph);
        art(21 + 30 * k, 11, BANNER, ph);
    }
    torch(s, 36, 22);
    torch(s, 96, 22);
    /* a bat under the vault, a beat of its wings a hat */
    bat = (int)(s->cnt[EN_ROW_HAT] / 4u % 3u);
    for (k = 0; k < 3; k++) {
        int lit = k == bat && s->playing && hot(s->ago[EN_ROW_HAT], 600u);
        art(60 + 14 * k, 18 - (k & 1) * 2, BAT[0], lit && (s->cnt[EN_ROW_HAT] & 1u));
        art(60 + 14 * k, 18 - (k & 1) * 2, BAT[1], lit && !(s->cnt[EN_ROW_HAT] & 1u));
    }
    wanderer(40, FEET, legs_of(s), sword_of(s), 1);
    magic(s, 40, FEET);
    sparks(s, 57, 31);
}

static void scene_tower(const scn_t *s)         /* the build: up the tower's stair, to the beacon */
{
    int k, at = (int)(s->prog * 7.99f), rays = (int)(s->prog * 4.0f) + 1;
    for (k = 0; k < 8; k++) {                    /* the stair */
        int x = 1 + 9 * k, y = FEET + 1 - 3 * k;
        pline(x, y, x + 8, y);
        pline(x + 8, y, x + 8, y - 3);
    }
    pbox(78, 19, 2, 24);                         /* the tower's wall, the beacon on its top */
    pbox(110, 19, 2, 24);
    pline(78, 19, 111, 19);
    pbox(89, 16, 9, 3);
    for (k = 0; k < 8; k++)                      /* the wanderer at every landing; lit where the build is */
        if (k != at)
            art(3 + 9 * k, FEET + 1 - 3 * k - 14, W_BODY, 0);
    wanderer(3 + 9 * at, FEET + 1 - 3 * at, legs_of(s), sword_of(s), 1);
    /* the beacon: its rays grow through the build and flash with the snare */
    for (k = 0; k < 4; k++) {
        int lit = k < rays && (!hot(s->ago[EN_ROW_SNARE], 70u) || k + 1 < rays);
        aline(88 - 3 * k, 15 - k, 86 - 3 * k, 13 - k, lit);
        aline(99 + 3 * k, 15 - k, 101 + 3 * k, 13 - k, lit);
        abox(92, 15 - k, 3, 1, lit);
    }
    sparks(s, 100, 28);
}

static const char *const SKEL[] = {"..###..", ".#.#.#.", "..###..", "...#...", ".#####.", "#..#..#", "..###..",
                                   "..#.#..", ".#...#.", ".#...#.", 0};
static const char *const BONES[] = {".......", "#.....#", ".#.#.#.", "..#.#..", ".#...#.", "#.###.#", 0};
static const char *const PENNANT[2][5] = {{"####..", "######", "####..", 0}, {"#####.", "####..", "#####.", 0}};
static void scene_battle(const scn_t *s)        /* a chorus: the skeletons come; the kick strikes them down */
{
    int k, at = (int)(s->cnt[EN_ROW_KICK] % 4u);
    for (k = 0; k < 7; k++) {                    /* the mountains far away */
        pline(16 * k, 26, 16 * k + 8, 18 + (k & 1) * 4);
        pline(16 * k + 8, 18 + (k & 1) * 4, 16 * k + 16, 26);
    }
    pline(0, FEET + 1, 115, FEET + 1);
    cbox(96, 11, 4, 4, L_PRINT);                 /* the moon */
    /* the war banner behind the wanderer, flying with the guitar */
    pbox(12, 16, 1, 27);
    art(13, 16, PENNANT[0], (s->cnt[EN_ROW_GUITAR] & 1u) != 0);
    art(13, 16, PENNANT[1], (s->cnt[EN_ROW_GUITAR] & 1u) == 0);
    for (k = 0; k < 3; k++) {                    /* a skeleton at each of three places; the kick brings it on */
        int x = 86 - 18 * k;
        art(x, FEET - 9, SKEL, s->playing && at == k);
    }
    art(50, FEET - 5, BONES, s->playing && at == 3);
    wanderer(26, FEET, legs_of(s), sword_of(s), 1);
    magic(s, 26, FEET);
    sparks(s, 44, 31);
}

static const char *const TREE[] = {"...#...", "..###..", ".#####.", "..###..", ".#####.", "#######", "...#...", "...#...", 0};
static const char *const FIRE[3][8] = {{"...#...", "..###..", ".#####.", 0},
                                        {"...#...", "..##...", "..###..", ".#####.", ".#####.", 0},
                                        {".#...#.", "...#...", "..###..", ".##.##.", ".#####.", "#######", ".#####.", 0}};
static void scene_camp(const scn_t *s)          /* the breakdown: a campfire under the stars */
{
    static const int8_t ST[8][2] = {{40, 12}, {52, 17}, {66, 11}, {78, 15}, {90, 10}, {104, 18}, {28, 16}, {112, 12}};
    int k, blaze = hot(s->ago[EN_ROW_SNARE], 220u) ? 2 : (int)((s->ms / 150u) & 1u), smoke;
    partp(2, 26, TREE);
    partp(10, 22, TREE);
    pline(0, FEET + 1, 115, FEET + 1);
    cdisc(100, 14, 4, L_PRINT);                  /* the moon, a crescent */
    cdisc(102, 13, 4, L_BG);
    pline(52, FEET, 61, FEET - 2);               /* the logs */
    pline(52, FEET - 2, 61, FEET);
    for (k = 0; k < 3; k++)                      /* the fire: the drums (the toms) stoke it */
        art(54, FEET - 4 - 2 * k, FIRE[k], k == blaze);
    for (k = 0; k < 8; k++)                      /* the stars twinkle with the synth */
        ap(ST[k][0], ST[k][1], hot(s->ago[EN_ROW_SYNTH], 400u) ? (s->cnt[EN_ROW_SYNTH] + (uint32_t)k) % 3u != 0u : (k & 1));
    smoke = (int)((s->ms / 300u) % 3u);          /* smoke, as loud as the noise */
    for (k = 0; k < 3; k++)
        abox(56 + (k & 1), FEET - 13 - 4 * k, 2, 2, s->noise > 0.05f && k == smoke);
    wanderer(30, FEET, LEGS_SIT, SWORD_REST, 1);
    magic(s, 30, FEET + 3);
}

static void scene_warden(const scn_t *s)        /* the drop: the Warden, an iron giant with a furnace for a heart */
{
    int k, up = (s->step / 4) & 1, hp = 16 - (int)(s->prog * 16.0f), fur = hot(s->ago[EN_ROW_KICK], 120u);
    int eyes = hot(s->ago[EN_ROW_SYNTH], 300u) || (s->ms / 500u) & 1u;
    pline(0, FEET + 1, 115, FEET + 1);
    /* the giant: two smokestacks, a head, shoulders, the furnace, legs */
    abox(78, 9, 2, 6, 1);
    abox(104, 9, 2, 6, 1);
    abox(86, 11, 12, 7, 1);
    abox(88, 13, 3, 2, 0);
    abox(93, 13, 3, 2, 0);
    if (eyes) {
        cbox(88, 13, 3, 2, L_HOT);
        cbox(93, 13, 3, 2, L_HOT);
    }
    abox(76, 18, 32, 5, 1);
    abox(80, 23, 24, 12, 1);
    abox(86, 26, 12, 6, 0);
    if (fur)
        cbox(86, 26, 12, 6, L_HOT);
    else
        abox(88, 28, 8, 3, 1);
    abox(81, 35, 8, 8, 1);
    abox(95, 35, 8, 8, 1);
    /* its arms: raised, then a slam, on the beats */
    climb(77, 20, 70, 11, col(s->playing && up));
    climb(77, 20, 68, 32, col(!s->playing || !up));
    abox(67, 8, 4, 4, s->playing && up);
    abox(65, 32, 5, 5, !s->playing || !up);
    climb(106, 20, 112, 30, L_INK);
    for (k = 0; k < 16; k++)                     /* its strength, falling through the drop */
        abox(2 + 3 * k, 10, 2, 2, k < hp);
    /* the drop's first beat: the impact */
    if (s->sect_bar == 0 && s->step < 3 && s->playing)
        for (k = 0; k < 4; k++)
            aline(30 + 6 * k, FEET - 18 - (k & 1) * 3, 33 + 6 * k, FEET - 22 - (k & 1) * 3, 1);
    wanderer(30, FEET, legs_of(s), sword_of(s), 1);
    magic(s, 30, FEET);
    sparks(s, 70, 24);
}

static const char *const BIRD[2][3] = {{"#...#", ".#.#.", 0}, {".....", "#.#.#", 0}};
static void scene_dawn(const scn_t *s)          /* the outro: out of the keep, into the dawn */
{
    int k, at = (int)(s->prog * 5.99f), sun = (int)(s->prog * 3.99f), rays = hot(s->ago[EN_ROW_SYNTH], 300u);
    for (k = 0; k < 4; k++) {                    /* the sun, rising through the outro */
        int cy = 34 - 6 * k;
        carc(92, cy, 7, 0.5f, 1.0f, col(k == sun));
        if (k == sun)
            cdisc(92, cy, 5, L_INK);
    }
    {
        int cy = 34 - 6 * sun;
        aline(80, cy - 2, 77, cy - 4, rays);
        aline(92, cy - 9, 92, cy - 12, rays);
        aline(104, cy - 2, 107, cy - 4, rays);
    }
    carc(30, 54, 22, 0.62f, 0.88f, L_PRINT);    /* the hills, in front of the sun */
    carc(84, 58, 26, 0.6f, 0.9f, L_PRINT);
    pline(0, FEET + 1, 115, FEET + 1);
    for (k = 0; k < 2; k++) {                    /* birds, a wingbeat a hat */
        int f = (int)((s->cnt[EN_ROW_HAT] + (uint32_t)k) & 1u);
        art(20 + 26 * k, 14 + 4 * k, BIRD[0], f);
        art(20 + 26 * k, 14 + 4 * k, BIRD[1], !f);
    }
    for (k = 0; k < 6; k++)                      /* the wanderer, walking away */
        if (k != at)
            art(6 + 13 * k, FEET - 14, W_BODY, 0);
    wanderer(6 + 13 * at, FEET, legs_of(s), sword_of(s), 1);
    magic(s, 6 + 13 * at, FEET);
}

static const char *const PLACE[EN_NSECT] = {"THE GATE", "THE HALLS", "THE STAIR", "THE BATTLE", "THE CAMPFIRE",
                                            "THE WARDEN", "THE DAWN"};

static void scene_draw(const scn_t *s)
{
    for (scn_pass = 0; scn_pass < 2; scn_pass++) {
        switch (s->sect) {
        case S_INTRO: scene_gate(s); break;
        case S_VERSE: scene_halls(s); break;
        case S_BUILD: scene_tower(s); break;
        case S_CHORUS: scene_battle(s); break;
        case S_BREAK: scene_camp(s); break;
        case S_DROP: scene_warden(s); break;
        default: scene_dawn(s); break;
        }
        mist(s);
    }
}

/* what the scene shows, as a number: drawn again when it changes */
static uint32_t scene_hash(const scn_t *s)
{
    uint32_t h = hash(2166136261u, (uint32_t)s->sect | (uint32_t)s->playing << 4 | (uint32_t)s->step << 8 |
                                       (uint32_t)(s->prog * 64.0f) << 16 | (uint32_t)s->sect_bar << 24);
    int i;
    for (i = 0; i < EN_NROW; i++)
        h = hash(h, (uint32_t)s->cnt[i] | (uint32_t)(s->ago[i] < 600u ? s->ago[i] / 40u : 99u) << 8);
    h = hash(h, (uint32_t)(s->noise * 30.0f) | (uint32_t)(s->ms / 120u) << 8);
    return h;
}
