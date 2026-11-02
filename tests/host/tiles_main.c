/* Host harness: prints every river tilemap write as "slot col <16 pattern bytes>",
 * resolving the tile id through the pattern uploads (set_bkg_data), so two builds
 * of src/river.c that draw the same picture print the same text even when they
 * use different tile ids.  tests/host_river_fast.ps1 diffs the exact build of the
 * 2600 mode against the -DRIVER_FAST build. */
#include <stdio.h>
#include <string.h>
#include "nes/nes.h"
#include "river.h"
#include "config.h"
volatile uint8_t playfield_scroll_y;
uint8_t game_ticks = 1;
void entities_row(void){}
void hud_set_playfield_scroll(uint8_t y){ playfield_scroll_y=y; }
static uint8_t vram[256][16];
void set_bkg_data(uint8_t f,uint8_t n,const uint8_t*d){ memcpy(vram[f], d, (size_t)n * 16u); }
void set_vram_byte(uint8_t*a,uint8_t v){
  unsigned long ad=(unsigned long)(uintptr_t)a;
  printf("%lu %lu ", (ad - PPU_NT1_BASE) >> 5, (ad - PPU_NT1_BASE) & 31u);
  for (int i=0;i<16;i++) printf("%02x", vram[v][i]);
  printf("\n");
}
#ifdef RIVER_FAST
extern uint16_t river_fast_bands, river_slow_bands, river_slow_why[3];
#endif
int main(int argc,char**argv){
  int rows = argc>1? atoi(argv[1]) : 1000;
  river_init(argc>2? atoi(argv[2]) : 0);
  uint16_t last = river_distance;
  int guard=0;
  while ((int)river_distance < rows && guard < rows*9+100) {
    river_update(); guard++;
    if (river_distance != last) { last = river_distance; printf("row %u\n", river_distance); }
  }
#ifdef RIVER_FAST
  fprintf(stderr, "fast bands %u, exact bands %u (not plain %u, irregular %u, atlas full %u)\n", river_fast_bands, river_slow_bands, river_slow_why[0], river_slow_why[1], river_slow_why[2]);
#endif
  return 0;
}




