/*
 * config_gbc.h - GBC replacements for the NES playfield constants in config.h.
 *
 * The river ring lives in BG map 0 ($9800, 32x32 tiles) and wraps at 256 px,
 * the same as SCY, so the ring scroll value is also the hardware scroll.
 * PPU_NT1_ATTR is deliberately not defined: the NES attribute table has no
 * counterpart here (colour is the BG attribute map in VRAM bank 1), so any
 * code that still writes it fails to compile instead of corrupting map row 30.
 */
#ifndef CONFIG_GBC_H
#define CONFIG_GBC_H

#define PPU_NT1_BASE            0x9800u

/* The 24 river tile columns sit in map columns 0..23, but logic X stays the
 * Atari HPOS, where river pixel 0 is at X = 32 (as on the NES).  The camera
 * subtracts it when it draws sprites (see gbc/main.c). */
#define RIVER_LEFT_COL_PX       32u
#define SOURCE_X_OFFSET         (RIVER_LEFT_COL_PX)

#define RIVER_RING_ROWS         32u
#define RIVER_SCROLL_HEIGHT     256u

/* The 160x144 screen shows a 128 px playfield (the 16 px HUD is the Window at
 * the bottom).  The view is bottom-anchored: screen line 0 shows this logic
 * scanline.  It also sets the jet's height on screen, so keep it tunable. */
#define GBC_VIEW_LINES          128u
#define GBC_VIEW_TOP_SCANLINE   (240u - GBC_VIEW_LINES)

/* The view reaches logic scanline 239 (the NES HUD hid 232..239), so sprites
 * are culled and despawned only from 240. */
#define ENT_CULL_Y              240u

/* River BG palettes (BG attribute map, VRAM bank 1, bit 3 clear).  Index 0
 * outline black, 1 land / road grey, 2 hills brown / road line yellow, 3 water.
 * Palette 1 is the HUD's.  Palette 0 is also the default of a cleared map.
 * Used by gbc/main.c (load) and entities.c (bridge-hit water flash). */
#define GBC_PAL_LAND            0u      /* dark green land ($D2) */
#define GBC_PAL_ALT             2u      /* bright green land ($D6) */
#define GBC_PAL_ROAD            3u      /* grey road, yellow centre line */
#define GBC_RIVER_PALS          4u
#define GBC_RGB_BLACK           RGB(0, 0, 0)
#define GBC_RGB_YELLOW          RGB(28, 26, 6)
#define GBC_RGB_BROWN           RGB(10, 3, 0)   /* hills / trees: land palettes' index 2 */
#define GBC_RGB_WATER          RGB(6, 12, 26)
#define GBC_RGB_FLASH           RGB(24, 5, 3)
#define GBC_RGB_LAND            RGB(5, 17, 5)
#define GBC_RGB_ALT             RGB(3, 24, 3)
#define GBC_RGB_ROAD            RGB(14, 14, 14)

#ifdef PLATFORM_DMG
/* One VRAM bank, 384 tiles (PLAN_DMG.md).  With LCDC bit 4 clear, BG and window ids 0..127 are
 * $9000 and 128..255 are $8800; sprite ids 0..127 are $8000 and 128..255 are $8800.
 *
 *   BG:     0..2 land, water, black; 3..228 river cache; 229..254 HUD; 255 is a sprite's
 *   Sprite: 0..127, then 255, allocated at boot (dmg_sprite_data in gbc/main.c)
 *
 * The logic keeps its sprite ids; oam_camera translates them through dmg_tile_map. */
#define DMG_HUD_TILE_BASE       229u
#define DMG_SPRITE_SPILL        255u
#define DMG_TILE_MAP_ADDR       0xD100u /* page aligned: gbc/main.c, $D000 is the OAM page */
#define DMG_RQ_PAT_ADDR         0xD200u /* river.c delay queue, RQ_TILES * 16 bytes */

/* Colour index -> shade (0 white .. 3 black).  River: 0 outline, 1 land / road, 2 hills / road
 * line, 3 water.  The HUD shares BGP: ink is 3 on 0.  Sprite colour 0 is transparent. */
#define DMG_BGP                 DMG_PALETTE(DMG_BLACK, DMG_LITE_GRAY, DMG_DARK_GRAY, DMG_WHITE)
#define DMG_BGP_FLASH           DMG_PALETTE(DMG_BLACK, DMG_LITE_GRAY, DMG_DARK_GRAY, DMG_DARK_GRAY)
#define DMG_OBP0                DMG_PALETTE(DMG_WHITE, DMG_BLACK, DMG_LITE_GRAY, DMG_DARK_GRAY)
#define DMG_OBP1                DMG_PALETTE(DMG_WHITE, DMG_BLACK, DMG_LITE_GRAY, DMG_DARK_GRAY)
#endif

#endif /* CONFIG_GBC_H */
