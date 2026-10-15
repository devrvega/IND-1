/* SPDX-License-Identifier: GPL-3.0-only */
/* IND-1 project: the settings in one flash object (OBJ_PROJ), the songs in two more (OBJ_SONG0, OBJ_SONG1,
 * four pieces each). A project of another format, or none (or AMB-1's, which lives at the same place),
 * gives the defaults; a song bank missing or broken gives its pieces songs on auto. Part of the unity
 * build, after app.h.
 *
 * The pieces (their recipes are the engine's, en_piece): each keeps its own settings and song as you
 * leave it; Tune and MIDI out are global. A song on auto is written anew whenever its piece is taken up,
 * so only a locked one's notes are kept. */
project_t proj;

typedef struct {
    uint32_t magic, format;
    en_song_t song[BANK_SONGS];
} bank_t;
static bank_t bank[NPIECES / BANK_SONGS];
static uint8_t bank_dirty;                       /* bit b: bank b differs from the flash */

static en_song_t *song_of(int piece) { return &bank[piece / BANK_SONGS].song[piece % BANK_SONGS]; }

static void bank_defaults(int b)
{
    int i;
    bank[b].magic = BANK_MAGIC;
    bank[b].format = BANK_FORMAT;
    for (i = 0; i < BANK_SONGS; i++)
        en_song_defaults(&bank[b].song[i]);
}

static void song_keep(void)                      /* the song playing, into its piece's place */
{
    en_song_t *d = song_of(proj.piece);
    if (!en_song.locked && !d->locked)
        return;                                  /* auto, and it was: written anew when taken up anyway */
    if (memcmp(d, &en_song, sizeof *d)) {
        memcpy(d, &en_song, sizeof *d);
        if (!d->locked) {                        /* back to auto: only that matters */
            en_song_defaults(d);
        }
        bank_dirty |= (uint8_t)(1u << (proj.piece / BANK_SONGS));
    }
}

void project_song_take(void)
{
    const en_song_t *s = song_of(proj.piece);
    const uint8_t *src = (const uint8_t *)s;
    uint8_t *dst = (uint8_t *)&en_song;
    uint32_t i;
    en_lock(1);                                  /* the render writes none of it while it comes */
    for (i = 0; i < sizeof en_song; i++)
        if (dst + i != &en_song.locked)
            dst[i] = src[i];
    en_lock(s->locked);
    en_song_loaded();
}

const char *piece_name(int i) { return en_piece_name(i); }

static int global_par(int p) { return p == P_TUNE || p == P_MIDI; }

static void recipe(int i, slot_t *s) { en_piece(i, s->par); }

static void take(const slot_t *s)                /* a piece's settings into play, the globals kept */
{
    int p;
    for (p = 0; p < P_NPARAMS; p++)
        if (!global_par(p))
            proj.par[p] = s->par[p];
}

static void keep(void)                           /* the piece playing, into its slot */
{
    int p;
    for (p = 0; p < P_NPARAMS; p++)
        proj.slot[proj.piece].par[p] = proj.par[p];
}

void project_defaults(void)
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    for (i = 0; i < NPIECES; i++)
        recipe(i, &proj.slot[i]);
    for (i = 0; i < P_NPARAMS; i++)
        proj.par[i] = en_param_info(i)->def;
    take(&proj.slot[0]);
    proj.leds = 1;
}

static int pars_ok(const int16_t *par)
{
    int i;
    for (i = 0; i < P_NPARAMS; i++) {
        const en_param_t *p = en_param_info(i);
        if (par[i] < p->lo || par[i] > p->hi)
            return 0;
    }
    return 1;
}

static int project_valid(void)
{
    int i;
    if (proj.magic != PROJ_MAGIC || proj.format != PROJ_FORMAT || !pars_ok(proj.par) || proj.piece >= NPIECES ||
        proj.leds > 2u)
        return 0;
    for (i = 0; i < NPIECES; i++)
        if (!pars_ok(proj.slot[i].par))
            return 0;
    return 1;
}

int project_load(void)
{
    int n, b, i;
    for (b = 0; b < NPIECES / BANK_SONGS; b++) {
        n = plat_store_load(OBJ_SONG0 + (uint32_t)b, &bank[b], sizeof bank[b]);
        if (n != (int)sizeof bank[b] || bank[b].magic != BANK_MAGIC || bank[b].format != BANK_FORMAT) {
            bank_defaults(b);
            continue;
        }
        for (i = 0; i < BANK_SONGS; i++)
            if (!en_song_valid(&bank[b].song[i]))
                en_song_defaults(&bank[b].song[i]);
    }
    bank_dirty = 0;
    memset(&proj, 0, sizeof proj);
    n = plat_store_load(OBJ_PROJ, &proj, sizeof proj);
    if (n == (int)sizeof proj && project_valid())
        return 0;
    project_defaults();
    return -1;
}

int project_save(void)
{
    int b, rc;
    keep();
    song_keep();
    rc = plat_store_save(OBJ_PROJ, &proj, sizeof proj);
    for (b = 0; b < NPIECES / BANK_SONGS; b++)
        if (bank_dirty >> b & 1u) {
            int r = plat_store_save(OBJ_SONG0 + (uint32_t)b, &bank[b], sizeof bank[b]);
            if (r == 0)
                bank_dirty &= (uint8_t)~(1u << b);
            else if (rc == 0)
                rc = r;
        }
    return rc;
}

void project_piece(int i)
{
    keep();
    song_keep();
    proj.piece = (uint8_t)((i % NPIECES + NPIECES) % NPIECES);
    take(&proj.slot[proj.piece]);
}

void project_piece_reset(void)
{
    slot_t s;
    recipe(proj.piece, &s);
    take(&s);
}

void project_apply(void)
{
    int i;
    for (i = 0; i < P_NPARAMS; i++)
        en_set(i, proj.par[i]);
}
