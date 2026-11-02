#ifndef PLATFORM_H
#define PLATFORM_H

/* One place that picks the GBDK platform header, so shared files build for
 * both targets: SM83 (Game Boy Color, build_gbc.ps1) and 6502 (NES, build.ps1). */
#ifdef __SDCC_sm83
#include <gb/gb.h>
#else
#include <nes/nes.h>
#endif

#if defined(__SDCC_sm83) && defined(PLATFORM_DMG)
/* Sprite tiles go through the boot-time allocator in gbc/main.c: the ids in the logic stay. */
void dmg_sprite_data(uint8_t first, uint8_t n, const uint8_t *src);
#undef set_sprite_data
#define set_sprite_data dmg_sprite_data
#endif

#endif
