/*
 * gbc/main.c - River Raid Game Boy Color port, milestone 1.
 *
 * Boot: refuse to run on anything but a GBC (an original Game Boy ignores the
 * CGB-only header flag), switch to double speed, load the BG palette, build
 * the river ring in BG map 0 and run the frame loop.
 *
 * Camera: the ring is 256 px, so the river's scroll value is also SCY (plus a
 * constant that anchors the 128 px view to the bottom of the logic world).
 * There is no split-screen interrupt: the VBlank handler writes SCX/SCY.  Game
 * logic keeps NES coordinates (X = Atari HPOS, Y = NES scanlines); only this file
 * maps them to the screen.
 *
 * Frames: a pass that ends publishes what it computed (scroll, a finished OAM page)
 * and the VBlank handler shows the latest one.  The next pass starts at once if a
 * frame has already gone by, so a pass of 1.1 frames costs 1.1 frames, not 2.
 *
 * Sprites: the logic files are compiled with shadow_OAM renamed to
 * gbc_logic_oam (see build_gbc.ps1), so they keep writing NES-coordinate
 * sprites.  oam_camera() translates them into the real shadow_OAM each frame.
 *
 * PLATFORM_DMG (build_gbc.ps1 -Dmg): the same game on one VRAM bank and four shades.  See
 * gbc/config_gbc.h for the tile layout.
 */
#include <gb/gb.h>
#include <gb/cgb.h>
#include <gbdk/console.h>
#include <gbdk/font.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "hud.h"
#include "river.h"
#include "player.h"
#include "entities.h"
#include "collision.h"
#include "sound.h"

#ifndef LOGIC_FRAMES
#define LOGIC_FRAMES 2u             /* player/entities every n-th frame: 2 = 30 Hz */
#endif

/* Declared in hud.h for the NES build, where hud.c owns the split. */
volatile uint8_t playfield_scroll_y;

/* The logic sprites; this file is compiled without the shadow_OAM rename.  The
 * NES code addresses 48 slots (the shot is slot 47, entities use up to 43), but
 * the GBC has 40 hardware sprites: oam_camera() packs the visible ones. */
#define LOGIC_OAM_SLOTS     64u     /* NES slots 0..63: the pause menu letters use 56..63 */
volatile struct OAM_item_t gbc_logic_oam[LOGIC_OAM_SLOTS];

void hud_gbc_init(void);

/* Read by tests through the .noi. */
volatile uint16_t frame_counter;
uint8_t game_ticks = 1u;
static uint8_t ent_skipped;         /* 1 = last pass ran the player group only */
static uint8_t main_last;
#ifdef PROF_PASSLOG
/* Per-pass log for tests/gb/pass_log.mjs (build_gbc.ps1 -PassLog; ~2k units per pass, so
 * it is not in the -ProfExport ROMs): frames elapsed (game_ticks), pass kind
 * (0 entities, 1 player, 2 player+entities), LY at pass start / after the river /
 * after the logic, and sys_time deltas for the last two. */
#define PASS_LOG_N 64u
uint8_t pl_idx;
uint8_t pl_rec[PASS_LOG_N][10];
#endif
static uint8_t logic_acc;      /* frames since the last logic pass */

/* Camera. SCX runs 0..GBC_CAM_MAX_X: 24 map columns (192 px) in a 160 px
 * screen leave 32 px to pan. */
#define GBC_CAM_MAX_X       32u
#define GBC_CAM_STEP        1u     /* px per frame the camera may move */
#define GBC_CAM_DEAD        12u     /* px the jet may drift from centre before the camera pans */
#define GBC_CAM_CENTER     (GBC_CAM_MAX_X / 2u)
#define GBC_CAM_JET_X       (SOURCE_X_OFFSET + 76u) /* jet X that puts the camera at 0 */
static uint8_t camera_x = GBC_CAM_CENTER;
static uint8_t cam_spawn_set;       /* camera_x holds this river's spawn view */
#define GBC_PLAYER_STATE_ATTRACT 5u /* = PLAYER_STATE_ATTRACT in player.h */
#define GBC_PLAYER_STATE_WAIT 3u    /* = PLAYER_STATE_WAIT in player.c */
#define GBC_PLAYER_STATE_READY 4u   /* = PLAYER_STATE_READY in player.c */
#define GBC_PLAYER_STATE_PAUSE 6u   /* = PLAYER_STATE_PAUSE in player.c */
#define GBC_PLAYER_WIDTH      8u    /* = PLAYER_WIDTH in player.c */

#ifdef PLATFORM_DUAL
uint8_t gb_cgb;                     /* 1 on a Game Boy Color; set first thing in main() */
#endif

#ifdef PLATFORM_DMG
/* Logic sprite id -> hardware tile.  Page aligned for oam_camera_core.  On a Game Boy Color it
 * is the identity: the sprites keep their ids in VRAM bank 1. */
uint8_t __at(DMG_TILE_MAP_ADDR) dmg_tile_map[256];
uint8_t dmg_spr_used;               /* hardware sprite tiles allocated; read by tests */
#ifdef PLATFORM_DUAL
#undef set_sprite_data              /* platform.h points it at dmg_sprite_data; below it is the library's */
#endif

/* Upload sprite tiles for the logic ids first..first + n - 1, sharing any tile whose pixels are
 * already there.  Boot only: the display is off, so VRAM can be read and written freely.  Past
 * the budget the tile shows as tile 0; tests/gb/dmg_check.mjs checks dmg_spr_used.  On a Game
 * Boy Color the tiles go to the current VRAM bank (1: main() sets it) under their own ids. */
void dmg_sprite_data(uint8_t first, uint8_t n, const uint8_t *src) {
    static uint8_t sums[129];       /* a memcmp per candidate took 3 s at this clock */
    uint8_t hw, sum, k;

#ifdef PLATFORM_DUAL
    if (gb_cgb) {
        set_sprite_data(first, n, src);
        return;
    }
#endif
    for (; n != 0u; n--, first++, src += 16u) {
        sum = 0u;
        for (k = 0u; k < 16u; k++) {
            sum = (uint8_t)(sum + src[k]);
        }
        for (hw = 0u; hw < dmg_spr_used; hw++) {
            if (hw <= 128u && sums[hw] == sum &&
                !memcmp((const uint8_t *)(0x8000u + ((uint16_t)((hw < 128u) ? hw : DMG_SPRITE_SPILL) << 4)), src, 16u)) {
                break;
            }
        }
        if (hw == dmg_spr_used) {
            dmg_spr_used++;
            if (hw > 128u) {
                hw = 0u;            /* over budget */
            } else {
                sums[hw] = sum;
                memcpy((uint8_t *)(0x8000u + ((uint16_t)((hw < 128u) ? hw : DMG_SPRITE_SPILL) << 4)), src, 16u);
            }
        }
        dmg_tile_map[first] = (hw < 128u) ? hw : DMG_SPRITE_SPILL;
    }
}
#define OAM_HW_TILE(t)      (dmg_tile_map[t])
#ifdef PLATFORM_DUAL
/* One property byte for both machines: a DMG ignores bit 3 (the bank), a GBC in colour mode
 * ignores bit 4 (OBP1). */
#define OAM_HW_PROP(p)      ((uint8_t)((((p) & 3u) ? ((p) | S_PALETTE) : (p)) | S_BANK))
#else
#define OAM_HW_PROP(p)      ((uint8_t)(((p) & 3u) ? ((p) | S_PALETTE) : (p)))
#endif
#else
#define OAM_HW_TILE(t)      (t)
#define OAM_HW_PROP(p)      ((uint8_t)((p) | S_BANK))
#endif

#ifdef GB_HAS_CGB
/* Index meaning is set by the river tiles: 0 outline (black), 1 land,
 * 3 water; 2 is spare (road line).  One palette for now; per-row colour is
 * milestone 3. */
static const palette_color_t bkg_palette[4][4] = {
    { GBC_RGB_BLACK, GBC_RGB_LAND, GBC_RGB_BROWN, GBC_RGB_WATER },     /* 0 land */
    { GBC_RGB_BLACK, GBC_RGB_BLACK, GBC_RGB_BLACK, GBC_RGB_BLACK },     /* 1 HUD (hud_gbc_init) */
    { GBC_RGB_BLACK, GBC_RGB_ALT, GBC_RGB_BROWN, GBC_RGB_WATER },      /* 2 bright land */
    { GBC_RGB_BLACK, GBC_RGB_ROAD, GBC_RGB_YELLOW, GBC_RGB_WATER }      /* 3 road */
};

/* Sprite palettes: 0 jet / fuel depot / shot, 1 bridge, 2 helicopters, 3 ships
 * (the colours gen_entity_gfx.py picks on the NES, as RGB555).  Colour 0 is
 * transparent. */
static const palette_color_t spr_palette[4][4] = {
    { RGB(0, 0, 0), RGB(31, 28, 6), RGB(31, 31, 31), RGB(30, 6, 4) },
    { RGB(0, 0, 0), RGB(3, 14, 4), GBC_RGB_BROWN, RGB(20, 20, 6) },
    { RGB(0, 0, 0), RGB(0, 0, 0), RGB(31, 28, 6), RGB(4, 6, 18) },
    { RGB(0, 0, 0), RGB(0, 0, 0), RGB(22, 4, 4), RGB(6, 28, 28) }
};
#endif

void hud_set_playfield_scroll(uint8_t y) {
    playfield_scroll_y = y;
}

#ifndef PLATFORM_DMG
/* An original Game Boy ignores the header's CGB-only flag, so say why nothing
 * will run instead of showing garbage. */
static void gbc_required(void) {
#if !defined(PROF_OAMCHECK) && !defined(PROF_RIVERCHECK)      /* the check ROMs need its bank 0 bytes */
    font_init();
    font_set(font_load(font_ibm));
    gotoxy(4, 8);
    puts("GBC REQUIRED");
#endif
    SHOW_BKG;
    DISPLAY_ON;
    for (;;) {
        wait_vbl_done();
    }
}
#endif

/* Camera X for a jet at logic X `jet_x`, clamped to the ring. */
static uint8_t camera_for(int16_t jet_x) {
    jet_x -= (int16_t)GBC_CAM_JET_X;
    if (jet_x < 0) {
        return 0u;
    }
    return (jet_x > (int16_t)GBC_CAM_MAX_X) ? (uint8_t)GBC_CAM_MAX_X : (uint8_t)jet_x;
}

/* Centre the view on the jet, clamped to the ring, a few pixels per frame.
 *
 * Attract and the (re)spawn scroll-in (WAIT) have no jet: the view holds the
 * X the jet will give it when it appears.  player_center() puts the jet on the
 * centre of RIVER_SPAWN_SLOT, so that is computed with the same formula and
 * set at once, not eased.  river_start() zeroes every slot's bounds and a
 * generated row is never 0, so a zero there means a fresh river whose spawn
 * slot is not generated yet: keep the old X meanwhile (the slot is written
 * ~12 scrolled rows after the start, while the whole view is still black).
 * The value is latched because attract keeps scrolling and the ring later
 * overwrites the slot; it is re-read after every river_init/river_restart,
 * including the attract-menu level change, which stays in ATTRACT. */
static void camera_follow(void) {
    uint8_t want;
    int16_t ideal;

    if (player_state == GBC_PLAYER_STATE_ATTRACT || player_state == GBC_PLAYER_STATE_WAIT) {
        if (river_right_x[RIVER_SPAWN_SLOT] == 0u) {
            cam_spawn_set = 0;
        } else if (!cam_spawn_set) {
            cam_spawn_set = 1u;
            camera_x = camera_for((int16_t)(SOURCE_X_OFFSET - (GBC_PLAYER_WIDTH >> 1)) +
                                  (int16_t)(((uint16_t)river_left_x[RIVER_SPAWN_SLOT] +
                                             river_right_x[RIVER_SPAWN_SLOT]) >> 1));
        }
        return;
    }
    cam_spawn_set = 0;

    want = camera_for((int16_t)player.x);
    if (player_state == GBC_PLAYER_STATE_READY) {
        camera_x = want;                /* the jet just appeared: no easing (already equal) */
        return;
    }
    /* Dead zone: hold the camera while the jet stays within GBC_CAM_DEAD px of
     * the view centre; past that, aim for the spot that puts the jet on the
     * zone's edge.  Measured from the unclamped centred camera so the pan still
     * reaches 0 / GBC_CAM_MAX_X. */
    ideal = (int16_t)player.x - (int16_t)GBC_CAM_JET_X;
    if (ideal - (int16_t)camera_x > (int16_t)GBC_CAM_DEAD) {
        ideal -= (int16_t)GBC_CAM_DEAD;
    } else if ((int16_t)camera_x - ideal > (int16_t)GBC_CAM_DEAD) {
        ideal += (int16_t)GBC_CAM_DEAD;
    } else {
        ideal = (int16_t)camera_x;
    }
    want = (ideal < 0) ? 0u : (ideal > (int16_t)GBC_CAM_MAX_X) ? (uint8_t)GBC_CAM_MAX_X : (uint8_t)ideal;
    if (want > camera_x) {
        camera_x = ((uint8_t)(want - camera_x) > GBC_CAM_STEP) ? (uint8_t)(camera_x + GBC_CAM_STEP) : want;
    } else if (want < camera_x) {
        camera_x = ((uint8_t)(camera_x - want) > GBC_CAM_STEP) ? (uint8_t)(camera_x - GBC_CAM_STEP) : want;
    }
}

static uint8_t oam_cam_x;           /* also publish_pass's scratch */

/* OAM is double buffered: the pass fills the page the DMA is not reading, then
 * _shadow_OAM_base flips to it (the VBlank handler's DMA reads that byte). */
#define OAM_PAGE2_ADDR      0xD000u
uint8_t __at(OAM_PAGE2_ADDR) oam_page2[160];
uint8_t *oam_back = oam_page2;
static uint8_t oam_cam_filled = 40u;  /* hardware slots last written on the back page (40: unknown) */
static uint8_t oam_cam_filled2 = 40u; /* ... on the page being shown */
static volatile uint8_t pub_scx, pub_scy;

/* The pass is finished: show it at the next VBlank. */
static void publish_pass(void) {
    __critical {
        pub_scx = camera_x;
        pub_scy = (uint8_t)(playfield_scroll_y + (GBC_VIEW_TOP_SCANLINE - PLAYFIELD_SCANLINE));
        _shadow_OAM_base = (uint8_t)((uint16_t)oam_back >> 8);
    }
    oam_back = (oam_back == oam_page2) ? (uint8_t *)shadow_OAM : oam_page2;
    oam_cam_x = oam_cam_filled;
    oam_cam_filled = oam_cam_filled2;
    oam_cam_filled2 = oam_cam_x;
}

/* Translate the logic sprites (NES coordinates) to hardware OAM: subtract the
 * camera, add the +16/+8 hardware offsets and hide whatever is outside the
 * view.  Sprite tiles live in VRAM bank 1 (OAM attribute bit 3). */
/* oam_camera's 8-bit range tests wrap correctly only with SOURCE_X_OFFSET + camera >= 8. */
typedef char oam_camera_x_assert[(SOURCE_X_OFFSET >= 8u) ? 1 : -1];

static void oam_camera_slow(void) {
    OAM_item_t *d = (OAM_item_t *)oam_back;
    uint8_t i, n = 0, y, x, xbase;

    /* All 8-bit: y < 240 spans fewer than 256 values and x spans exactly 256,
     * so "-8 < v < limit" becomes one unsigned compare of v + 8 - 1 (the sum
     * wraps for everything off-view). */
    xbase = (uint8_t)(SOURCE_X_OFFSET + camera_x - 8u);
    for (i = 0; i < LOGIC_OAM_SLOTS && n < 40u; i++) {
        y = gbc_logic_oam[i].y;
        if (y >= ENT_CULL_Y) {
            continue;
        }
        /* Pause menu slots (player.c) hold hardware coordinates: no camera. */
        if (player_state == GBC_PLAYER_STATE_PAUSE && (i >= 56u || (i >= 2u && i <= 4u))) {
            d->y = y;
            d->x = gbc_logic_oam[i].x;
            d->tile = OAM_HW_TILE(gbc_logic_oam[i].tile);
            d->prop = OAM_HW_PROP(gbc_logic_oam[i].prop);
            d++;
            n++;
            continue;
        }
        y = (uint8_t)(y - (GBC_VIEW_TOP_SCANLINE - 8u));        /* vy + 8 */
        x = (uint8_t)(gbc_logic_oam[i].x - xbase);              /* vx + 8 */
        if ((uint8_t)(y - 1u) >= (uint8_t)(GBC_VIEW_LINES + 7u) ||
            (uint8_t)(x - 1u) >= 167u) {
            continue;
        }
        d->y = (uint8_t)(y + 8u);
        d->x = x;
        d->tile = OAM_HW_TILE(gbc_logic_oam[i].tile);
        d->prop = OAM_HW_PROP(gbc_logic_oam[i].prop);
        d++;
        n++;
    }
    while (n < 40u) {
        d->y = 0u;
        d++;
        n++;
    }
}

#define OAM_SHOT_SLOT       47u     /* = PLAYER_SHOT_SLOT in player.c */
static uint8_t oam_cam_xbase;

/* Translate oam_cam_n logic slots from oam_cam_s into the OAM page at oam_cam_d, stopping when
 * the page is full.  Hand-written: ~15 M-cycles per hidden slot and ~40 per sprite against
 * 25 and 75 in C.  HL = source, DE = destination (the pages are aligned, so inc e is enough),
 * B = slots left, C = camera base.  y + 7 - view top < 135 and x + 8 - 1 < 167 as in the C walk;
 * nothing is written until both tests pass. */
static const uint8_t *oam_cam_s;
static uint8_t *oam_cam_d;
static uint8_t oam_cam_n;
typedef char oam_asm_consts[(GBC_VIEW_TOP_SCANLINE == 112u && GBC_VIEW_LINES == 128u && S_BANK == 8u) ? 1 : -1];
#ifdef PLATFORM_DMG
typedef char oam_asm_dmg_consts[(DMG_TILE_MAP_ADDR == 0xD100u && S_PALETTE == 0x10u) ? 1 : -1];
#endif

static void oam_camera_core(void) {
    __asm
        ld      hl, #_oam_cam_n
        ld      a, (hl)
        or      a, a
        ret     Z
        ld      b, a
        ld      a, (_oam_cam_xbase)
        ld      c, a
        ld      a, (_oam_cam_d)
        ld      e, a
        ld      a, (_oam_cam_d + 1)
        ld      d, a
        ld      a, (_oam_cam_s)
        ld      l, a
        ld      a, (_oam_cam_s + 1)
        ld      h, a
    1$:
        ld      a, (hl+)            ; y
        sub     a, #105
        cp      a, #135
        jr      NC, 2$
        ld      a, (hl)             ; x
        sub     a, c
        dec     a
        cp      a, #167
        jr      NC, 2$
        inc     a
        inc     e
        ld      (de), a
        dec     e
        dec     hl
        ld      a, (hl+)
        sub     a, #96              ; y + 16 - 112
        ld      (de), a
        inc     e
        inc     e
        inc     hl
        ld      a, (hl+)            ; tile
#ifdef PLATFORM_DMG
        push    hl                  ; through dmg_tile_map, page aligned
        ld      h, #0xD1            ; DMG_TILE_MAP_ADDR
        ld      l, a
        ld      a, (hl)
        pop     hl
#endif
        ld      (de), a
        inc     e
        ld      a, (hl+)            ; prop
#ifdef PLATFORM_DMG
        bit     0, a                ; any colour palette but 0 is OBP1
        jr      NZ, 5$
        bit     1, a
        jr      Z, 6$
    5$:
        set     4, a                ; S_PALETTE
    6$:
#ifdef PLATFORM_DUAL
        set     3, a                ; S_BANK: a DMG ignores it
#endif
#else
        set     3, a                ; S_BANK
#endif
        ld      (de), a
        inc     e
        ld      a, e
        cp      a, #160
        jr      Z, 4$
        jr      3$
    2$:
        inc     hl
        inc     hl
        inc     hl
    3$:
        dec     b
        jr      NZ, 1$
    4$:
        ld      a, l
        ld      (_oam_cam_s), a
        ld      a, h
        ld      (_oam_cam_s + 1), a
        ld      a, e
        ld      (_oam_cam_d), a
        ld      a, d
        ld      (_oam_cam_d + 1), a
    __endasm;
}

#ifdef PLATFORM_DMG
/* An original Game Boy ranks overlapping sprites by X (the smaller X wins, OAM order only breaks
 * a tie); a Game Boy Color in colour mode ranks by OAM order, which is what puts the bridge tank
 * (slots 2, 3) in front of the bridge.  So on a DMG a bridge tile just left of the tank paints
 * over its left part.  While the tank overlaps such a tile, hide that tile for this pass (only
 * the hardware copy: the logic y is restored after the translation).  Bridge sprite tiles are
 * logic ids 102..113 (ENT_TILE_BASE + ent_gfx_first[14], 12 tiles; entities.c). */
#define DMG_BRIDGE_TILE_FIRST   102u
#define DMG_BRIDGE_TILES        12u
#define DMG_HIDE_MAX            6u
static uint8_t dmg_hid_n;
static uint8_t dmg_hid_i[DMG_HIDE_MAX];
static uint8_t dmg_hid_y[DMG_HIDE_MAX];
static uint8_t dmg_tank_s, dmg_tank_i, dmg_tank_x, dmg_tank_y;

static void dmg_tank_hide(void) {
    dmg_hid_n = 0u;
    for (dmg_tank_s = 2u; dmg_tank_s <= 3u; dmg_tank_s++) {
        dmg_tank_y = gbc_logic_oam[dmg_tank_s].y;
        if (dmg_tank_y >= ENT_CULL_Y) {
            continue;
        }
        dmg_tank_x = gbc_logic_oam[dmg_tank_s].x;
        for (dmg_tank_i = 7u; dmg_tank_i < ent_oam_used && dmg_hid_n < DMG_HIDE_MAX; dmg_tank_i++) {
            if ((uint8_t)(gbc_logic_oam[dmg_tank_i].tile - DMG_BRIDGE_TILE_FIRST) >= DMG_BRIDGE_TILES ||
                (uint8_t)(dmg_tank_x - gbc_logic_oam[dmg_tank_i].x - 1u) >= 7u ||      /* 0 < dx < 8 */
                (uint8_t)(dmg_tank_y - gbc_logic_oam[dmg_tank_i].y + 7u) >= 15u ||     /* |dy| < 8 */
                gbc_logic_oam[dmg_tank_i].y >= ENT_CULL_Y) {
                continue;
            }
            dmg_hid_i[dmg_hid_n] = dmg_tank_i;
            dmg_hid_y[dmg_hid_n] = gbc_logic_oam[dmg_tank_i].y;
            dmg_hid_n++;
            gbc_logic_oam[dmg_tank_i].y = 240u;
        }
    }
}

static void dmg_tank_restore(void) {
    while (dmg_hid_n != 0u) {
        dmg_hid_n--;
        gbc_logic_oam[dmg_hid_i[dmg_hid_n]].y = dmg_hid_y[dmg_hid_n];
    }
}
#endif

/* The common case (no pause menu): same output as oam_camera_slow, walking the
 * arrays by pointer.  The per-slot work is what costs here (64 slots, most of
 * them empty), so index arithmetic and the pause test stay out of the loop. */
static void oam_camera(void) {
    uint8_t *d;
    uint8_t i;

    if (player_state == GBC_PLAYER_STATE_PAUSE) {
        oam_camera_slow();
        oam_cam_filled = 40u;
        return;
    }
    oam_cam_xbase = (uint8_t)(SOURCE_X_OFFSET + camera_x - 8u);
    oam_cam_d = oam_back;
    /* Outside the pause menu only slots below ent_oam_used and the shot are live. */
    oam_cam_s = (const uint8_t *)gbc_logic_oam;
    oam_cam_n = ent_oam_used;
#ifdef PLATFORM_DMG
    if (!gb_cgb) {
        dmg_tank_hide();
    }
#endif
    oam_camera_core();
#ifdef PLATFORM_DMG
    if (!gb_cgb) {
        dmg_tank_restore();
    }
#endif
    if ((uint8_t)(uint16_t)oam_cam_d != 160u) {
        oam_cam_s = (const uint8_t *)&gbc_logic_oam[OAM_SHOT_SLOT];
        oam_cam_n = 1u;
        oam_camera_core();
    }
    /* Hardware slots past d are hidden already, up to where the last pass ended. */
    d = oam_cam_d;
    i = oam_cam_filled;
    oam_cam_filled = (uint8_t)((uint8_t)(uint16_t)d >> 2);
    i = (i > oam_cam_filled) ? (uint8_t)(i - oam_cam_filled) : 0u;
    while (i != 0u) {
        *d = 0u;
        d += 4u;
        i--;
    }
}

/* The HUD Window starts at screen line GBC_VIEW_LINES and sprites draw over it,
 * so entities would stay visible on the HUD until culled.  LYC fires on the last
 * playfield line; once its HBlank is reached (the line's sprites are drawn) the
 * sprite layer is switched off, and a VBlank handler switches it on again.  It must not be
 * the main loop: a pass that overruns a frame would leave the whole next frame without
 * sprites (they blinked whenever an enemy near the HUD made a pass run long).
 * Short on purpose, no banked calls (rule 2). */
static void hud_hide_sprites_isr(void) {
    while ((STAT_REG & STATF_BUSY) != 0u) {
    }
    LCDC_REG &= (uint8_t)~LCDCF_OBJON;
}

static void hud_show_sprites_isr(void) {
    LCDC_REG |= LCDCF_OBJON;
    SCX_REG = pub_scx;
    SCY_REG = pub_scy;
}

void main(void) {
    uint8_t i;

#ifdef PLATFORM_DUAL
    gb_cgb = (_cpu == CGB_TYPE) ? 1u : 0u;
#elif !defined(PLATFORM_DMG)
    if (_cpu != CGB_TYPE) {
        gbc_required();
    }
#endif
#ifdef GB_HAS_CGB
#ifndef DMG_PROXY                       /* build_gbc.ps1 -DmgProxy: stay at the DMG clock (single speed) */
    if (gb_cgb) {
        cpu_fast();
    }
#endif
#endif

    DISPLAY_OFF;
#ifdef PLATFORM_DMG
    LCDC_REG &= (uint8_t)~LCDCF_BG8000; /* BG and window: $9000 and $8800 */
    if (!gb_cgb) {
        BGP_REG = DMG_BGP;
        OBP0_REG = DMG_OBP0;
        OBP1_REG = DMG_OBP1;
    }
#endif
#ifdef GB_HAS_CGB
    if (gb_cgb) {
        set_bkg_palette(0, GBC_RIVER_PALS, &bkg_palette[0][0]);
    }
#endif
    for (i = 0; i < LOGIC_OAM_SLOTS; i++) {
        gbc_logic_oam[i].y = 240u;
    }

#ifdef PLATFORM_DUAL
    if (gb_cgb) {
        i = 0u;
        do {
            dmg_tile_map[i] = i;        /* the logic ids are the hardware ids in bank 1 */
        } while (++i != 0u);
    }
#endif
#ifdef GB_HAS_CGB
    /* Sprite tiles go to VRAM bank 1: the ids the sprite code uses overlap the
     * river cache in bank 0. */
    if (gb_cgb) {
        VBK_REG = 1;
    }
#endif
    player_init_graphics();
    entities_init_graphics();
#ifdef GB_HAS_CGB
    if (gb_cgb) {
        VBK_REG = 0;
        /* The two modules above feed NES colour numbers to the palette call (a
         * no-op in entities.c, garbage in player.c); set the real ones last. */
        set_sprite_palette(0, 4, &spr_palette[0][0]);
    }
#endif
    hud_gbc_init();
    sound_init();

    river_init(0);
    player_init();
    entities_init();
    collision_init();
    camera_follow();
    oam_camera();
    publish_pass();
    hud_show_sprites_isr();
    SHOW_BKG;
    SHOW_SPRITES;
    LYC_REG = (uint8_t)(GBC_VIEW_LINES - 1u);
    STAT_REG |= STATF_LYC;
    add_LCD(hud_hide_sprites_isr);
    add_VBL(hud_show_sprites_isr);
    set_interrupts(VBL_IFLAG | LCD_IFLAG);
    DISPLAY_ON;

    main_last = (uint8_t)sys_time;
    for (;;) {
        /* Wait only if no frame has gone by: vsync() always waits for the NEXT
         * VBlank, so calling it after an overrun would turn 1.01 frames into 2. */
        if ((uint8_t)sys_time == main_last) {
            vsync();
        }

        game_ticks = (uint8_t)((uint8_t)sys_time - main_last);
        main_last = (uint8_t)sys_time;
        if (game_ticks > 4u) {
            game_ticks = 4u;
        }
        frame_counter++;
        sound_update();                 /* real frame count, before game_ticks is merged */
        if ((uint8_t)(logic_acc + game_ticks) < LOGIC_FRAMES) {
            hud_update();               /* VRAM writes (the library waits for STAT) */
        }

        /* Same schedule as the NES main: the river every frame, the player
         * group and the entity group on alternate frames. */
        river_gen_budget = ((uint8_t)(logic_acc + game_ticks) >= LOGIC_FRAMES) ? 3u : 1u;
#ifdef PROF_PASSLOG
        pl_rec[pl_idx][0] = game_ticks;
        pl_rec[pl_idx][1] = LY_REG;
        pl_rec[pl_idx][7] = (uint8_t)sys_time;
        pl_rec[pl_idx][8] = river_owed;
#endif
        river_update();
#ifdef PROF_PASSLOG
        pl_rec[pl_idx][2] = LY_REG;
        pl_rec[pl_idx][3] = (uint8_t)((uint8_t)sys_time - pl_rec[pl_idx][7]);
        pl_rec[pl_idx][9] = river_owed;
#endif

        logic_acc = (uint8_t)(logic_acc + game_ticks);
        if (logic_acc >= LOGIC_FRAMES) {
            game_ticks = logic_acc;
            logic_acc = 0;
            /* An overrun pass (game_ticks >= LOGIC_FRAMES) takes this branch
             * every time, which would starve the entity group: it moves,
             * spawns and removes them.  Run it after the player in that case. */
            if (ent_skipped) {
                player_update();
                collision_update();
                entities_update();
                ent_skipped = 0;
#ifdef PROF_PASSLOG
                pl_rec[pl_idx][5] = 2u;
#endif
            } else {
                entities_follow();
                player_update();
                collision_update();
                ent_skipped = 1;
#ifdef PROF_PASSLOG
                pl_rec[pl_idx][5] = 1u;
#endif
            }
        } else {
            entities_update();
            ent_skipped = 0;
#ifdef PROF_PASSLOG
            pl_rec[pl_idx][5] = 0u;
#endif
        }
#ifdef PROF_PASSLOG
        pl_rec[pl_idx][4] = LY_REG;     /* after the logic, before camera/OAM */
#endif
        camera_follow();
        oam_camera();
#ifdef PROF_OAMCHECK
        /* oam_camera must match the reference walk byte for byte (LESSONS 18), and
         * drawing only the rows near the view must not change the hardware OAM. */
        ent_check_oam(0u);
        oam_camera_slow();
        ent_check_oam(1u);
        if (!ent_skipped) {
            ent_check_redraw(HUD_SPLIT_SCANLINE);
            oam_camera_slow();
            ent_check_oam(2u);
            ent_check_redraw(96u);
            oam_camera_slow();
        }
#endif
        publish_pass();
#ifdef PROF_PASSLOG
        pl_rec[pl_idx][6] = LY_REG;
        pl_idx = (uint8_t)((pl_idx + 1u) & (PASS_LOG_N - 1u));
#endif
    }
}
