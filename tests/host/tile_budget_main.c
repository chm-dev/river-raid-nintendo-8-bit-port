/* Host harness: how many river cache tiles does the exact river need on a DMG?
 * Runs src/river.c with the REAL spawner (src/entities.c, #included for its statics), on the
 * same schedule as the main loop in src/gbc/main.c, so roads and the banks' object avoidance
 * are in the river, unlike tiles_main.c and main.c.  See tests/host_tile_budget.ps1.
 *
 * Geometry, in "T" = pixels scrolled so far.  The generator finishes tile row d (river_distance)
 * at T = 8 * (d - 1) with its top at ring line S - 16, where S = -T is the logic scroll.  The
 * screen starts at S + view_off (publish_pass: 82 today) and is 128 lines tall, and the display
 * may lag the logic by L pixels.  So row d has left the screen for good at
 *     T_off(d) = 8 * (d + 1) + view_off + 128 + L.
 *
 * The cache is a FIFO ring and a slot only serves the row that uploaded it, so the smallest safe
 * size comes straight from the upload log: for each upload, the number of uploads made before
 * its row is gone.  A smaller view_off models uploading later (same river, same schedule).
 *
 * Usage: budget.exe <tile rows> <start index> <speed 0|1|2> [view_off L jitter]
 * jitter n: every n-th pass takes 2 frames (a pass rate of about 1 - 1/n).
 *
 * The real allocator (-DTILE_RIVER_COUNT=n) also runs a clobber check that does not use the
 * formula above: an upload that changes the pixels of a map cell the hardware is showing, decided
 * from the cell's map row and SCY as publish_pass sets it (scroll + view_off; L older scrolls are
 * larger, so they widen the window downward).  It must go from >0 to 0 at the size the log
 * predicts; that ties the formula and the FIFO model to the code.  The ring is the host's 240 px,
 * which is enough: 16 + 82 + 128 < 240. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "nes/nes.h"
volatile uint16_t sys_time;     /* GBDK's frame counter, read by entities_update() */
#include "../../src/entities.c"

volatile uint8_t playfield_scroll_y;
uint8_t game_ticks = 1;
volatile OAM_item_t shadow_OAM[64];
uint8_t hud_score_bcd[3], hud_hi_bcd[3], hud_bridge, hud_lives, player_disable;
uint8_t player_state;           /* 0: not attract, the jet is playing */
void sound_play_bonus(void) {}
void sound_play_missile(void) {}
void sound_play_shell(void) {}
void set_sprite_palette(uint8_t f, uint8_t n, const palette_color_t *p) { (void)f; (void)n; (void)p; }
void set_sprite_data(uint8_t f, uint8_t n, const uint8_t *d) { (void)f; (void)n; (void)d; }
void set_bkg_palette_entry(uint8_t p, uint8_t e, palette_color_t c) { (void)p; (void)e; (void)c; }

static long total_t;            /* T: pixels scrolled */
void hud_set_playfield_scroll(uint8_t y) {
    total_t += (long)(uint8_t)((playfield_scroll_y + RIVER_SCROLL_HEIGHT - y) % RIVER_SCROLL_HEIGHT);
    playfield_scroll_y = y;
}

#define FIRST_CACHE_ID  TILE_RIVER_BASE
#define LAST_CACHE_ID   (TILE_RIVER_BASE + TILE_RIVER_COUNT - 1u)
#define MAX_ALLOCS      4000000
typedef struct { int row; long t; uint8_t pat[16]; } alloc_t;
static alloc_t *allocs;
static int n_allocs;

/* real clobber check, for this build's cache size */
static int check_view_off = 82, check_lag;
static uint8_t vram[256][16];
static struct { int row; uint8_t id; uint8_t pat[16]; } cell[32][32];
static long clobbers;
static int clobber_worst_age;

/* Is any line of map row r inside [scy, scy + 127 + L], modulo the ring? */
static int row_on_screen(int r) {
    int scy = (playfield_scroll_y + check_view_off) % RIVER_SCROLL_HEIGHT;

    for (int line = r * 8; line < r * 8 + 8; line++) {
        if ((line - scy + RIVER_SCROLL_HEIGHT) % RIVER_SCROLL_HEIGHT <= 127 + check_lag) {
            return 1;
        }
    }
    return 0;
}

void set_bkg_data(uint8_t f, uint8_t n, const uint8_t *d) {
    for (int i = 0; i < n; i++, f++, d += 16) {
        if (f < FIRST_CACHE_ID || f > LAST_CACHE_ID) {
            continue;
        }
        if (n_allocs < MAX_ALLOCS) {
            allocs[n_allocs].row = river_distance;
            allocs[n_allocs].t = total_t;
            memcpy(allocs[n_allocs].pat, d, 16);
            n_allocs++;
        }
        for (int r = 0; r < 32; r++) {
            for (int c = 0; c < 32; c++) {
                if (cell[r][c].row && cell[r][c].id == f && memcmp(d, cell[r][c].pat, 16) && row_on_screen(r)) {
                    int age = river_distance - cell[r][c].row;

                    clobbers++;
                    if (age < clobber_worst_age || !clobber_worst_age) {
                        clobber_worst_age = age;
                    }
                }
            }
        }
        memcpy(vram[f], d, 16);
    }
}

void set_vram_byte(uint8_t *a, uint8_t v) {
    unsigned long ad = (unsigned long)(uintptr_t)a;
    unsigned r = (ad - PPU_NT1_BASE) >> 5, c = (ad - PPU_NT1_BASE) & 31u;

    if (r < 32u) {
        cell[r][c].row = (v >= FIRST_CACHE_ID && v <= LAST_CACHE_ID) ? river_distance : 0;
        cell[r][c].id = v;
        memcpy(cell[r][c].pat, vram[v], 16);
    }
}

/* Smallest FIFO that never reuses a slot before its row is gone. */
static int need_slots(int view_off, int lag) {
    int worst = 0, j = 0;

    for (int a = 0; a < n_allocs; a++) {
        long off = 8L * (allocs[a].row + 1) + view_off + 128 + lag;

        if (j < a) {
            j = a;
        }
        while (j < n_allocs && allocs[j].t < off) {
            j++;
        }
        if (j == n_allocs) {
            break;              /* the run ended before this row left */
        }
        if (j - a > worst) {
            worst = j - a;
        }
    }
    return worst;
}

/* Tiles with distinct pixels among the rows not gone yet: the floor for a cache that would share
 * tiles across rows (today only columns of one row share). */
#define HASH_BITS 20
typedef struct { uint64_t key; int count; } hslot_t;
static hslot_t *table;
static uint64_t pat_hash(const uint8_t *p) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (int i = 0; i < 16; i++) {
        h = (h ^ p[i]) * 0x100000001b3ull;
    }
    return h | 1u;
}
static hslot_t *hfind(uint64_t k) {
    unsigned i = (unsigned)(k >> 20) & ((1u << HASH_BITS) - 1u);
    while (table[i].key && table[i].key != k) {
        i = (i + 1u) & ((1u << HASH_BITS) - 1u);
    }
    table[i].key = k;
    return &table[i];
}
static int distinct_live(int view_off, int lag) {
    int live = 0, worst = 0, tail = 0;

    memset(table, 0, sizeof(hslot_t) << HASH_BITS);
    for (int a = 0; a < n_allocs; a++) {
        while (tail < a && 8L * (allocs[tail].row + 1) + view_off + 128 + lag <= allocs[a].t) {
            if (--hfind(pat_hash(allocs[tail].pat))->count == 0) {
                live--;
            }
            tail++;
        }
        if (hfind(pat_hash(allocs[a].pat))->count++ == 0) {
            live++;
        }
        if (live > worst) {
            worst = live;
        }
    }
    return worst;
}

int main(int argc, char **argv) {
    int rows = argc > 1 ? atoi(argv[1]) : 20000;
    int variant = argc > 2 ? atoi(argv[2]) : 0;
    int speed = argc > 3 ? atoi(argv[3]) : 2;
    int jitter = argc > 6 ? atoi(argv[6]) : 0;
    long passes = 0;
    uint8_t logic_acc = 0;
    long min_lead = 1000000, max_lead = -1000000, guard = 0;
    int peak_row = 0, this_row = 0, last_row = 0;
    static const int views[] = { 82, 40, 16, 8, 0 };
    static const int lags[] = { 0, 2, 6 };

    if (argc > 4) check_view_off = atoi(argv[4]);
    if (argc > 5) check_lag = atoi(argv[5]);
    allocs = malloc(sizeof(alloc_t) * MAX_ALLOCS);
    table = malloc(sizeof(hslot_t) << HASH_BITS);
    river_init((uint8_t)variant);
    entities_init();
    river_speed = (uint8_t)speed;

    /* the schedule of src/gbc/main.c at 1.00 passes/frame */
    while ((int)river_distance < rows && guard++ < 8L * rows * 8L + 100000L) {
        game_ticks = (jitter && ++passes % jitter == 0) ? 2u : 1u;
        sys_time = (uint16_t)(sys_time + game_ticks);
        river_gen_budget = ((uint8_t)(logic_acc + game_ticks) >= 2u) ? 3u : 1u;
        river_update();
        logic_acc = (uint8_t)(logic_acc + game_ticks);
        if (logic_acc >= 2u) {
            game_ticks = logic_acc;
            logic_acc = 0u;
            entities_follow();
        } else {
            entities_update();
        }
    }

    /* how the schedule really leads the display (T at upload, minus the nominal one) */
    for (int a = 0; a < n_allocs; a++) {
        long lead = allocs[a].t - 8L * (allocs[a].row - 1);

        if (lead < min_lead) min_lead = lead;
        if (lead > max_lead) max_lead = lead;
        if (allocs[a].row != last_row) {
            last_row = allocs[a].row;
            this_row = 0;
        }
        if (++this_row > peak_row) peak_row = this_row;
    }

    printf("start %d speed %d rows %d uploads %d (%.2f/row, peak %d/row), T-nominal at upload %ld..%ld, level %u\n",
           variant, speed, (int)river_distance, n_allocs, (double)n_allocs / river_distance, peak_row,
           min_lead, max_lead, rg_level);
    for (unsigned v = 0; v < sizeof(views) / sizeof(views[0]); v++) {
        printf("  view_off %2d:", views[v]);
        for (unsigned l = 0; l < sizeof(lags) / sizeof(lags[0]); l++) {
            printf("  L%d need %3d (distinct %3d)", lags[l], need_slots(views[v], lags[l]), distinct_live(views[v], lags[l]));
        }
        printf("\n");
    }
    /* A queue that delays uploads by q tile rows must hold this many patterns. */
    printf("  uploads in q rows:");
    for (int q = 8; q <= 12; q++) {
        int worst = 0, tail = 0;

        for (int a = 0; a < n_allocs; a++) {
            while (allocs[a].row - allocs[tail].row >= q) {
                tail++;
            }
            if (a - tail + 1 > worst) {
                worst = a - tail + 1;
            }
        }
        printf("  q%d %d", q, worst);
    }
    printf("\n");
    printf("  cache %u: clobbers at view_off %d L%d: %ld, youngest %d rows\n", (unsigned)TILE_RIVER_COUNT, check_view_off, check_lag, clobbers, clobber_worst_age);
    return 0;
}
