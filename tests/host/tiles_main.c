/* Host harness: prints every river tilemap write as "slot col <16 pattern bytes>",
 * resolving the tile id through the pattern uploads (set_bkg_data), so two builds
 * of src/river.c that draw the same picture print the same text even when they
 * use different tile ids.  tests/host_river_fast.ps1 diffs the exact build of the
 * 2600 mode against the -DRIVER_FAST build.
 *
 * It also checks the cache itself: an upload to a tile id that a recently written
 * (still visible) map cell uses, with different pixels, would corrupt the picture on
 * screen.  Those are counted and reported on stderr ("clobbered N"). */
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
static uint8_t cell_id[32][32];
static uint16_t cell_row[32][32];     /* tile row (river_distance) the cell was written in */
static uint8_t cell_pat[32][32][16];
static uint8_t cell_set[32][32];
static int clobbered;
#define VISIBLE_ROWS 28
void set_bkg_data(uint8_t f,uint8_t n,const uint8_t*d){
  for (int i = 0; i < n; i++) {
    for (int r = 0; r < 32; r++) for (int c = 0; c < 32; c++) {
      if (cell_set[r][c] && cell_id[r][c] == (uint8_t)(f + i) && (uint16_t)(river_distance - cell_row[r][c]) < VISIBLE_ROWS &&
          memcmp(cell_pat[r][c], d + i * 16, 16)) {
        if (clobbered++ < 5) fprintf(stderr, "clobber: id %u row %d col %d (written %u rows ago)\n", f + i, r, c, (unsigned)(river_distance - cell_row[r][c]));
      }
    }
  }
  memcpy(vram[f], d, (size_t)n * 16u);
}
void set_vram_byte(uint8_t*a,uint8_t v){
  unsigned long ad=(unsigned long)(uintptr_t)a;
  unsigned r = (ad - PPU_NT1_BASE) >> 5, c = (ad - PPU_NT1_BASE) & 31u;
  printf("%u %u ", r, c);
  for (int i=0;i<16;i++) printf("%02x", vram[v][i]);
  printf("\n");
  if (r < 32 && c < 32) { cell_set[r][c] = 1; cell_id[r][c] = v; cell_row[r][c] = river_distance; memcpy(cell_pat[r][c], vram[v], 16); }
}
#ifdef RIVER_FAST
extern uint16_t river_fast_bands, river_slow_bands, river_slow_why[3];
extern uint8_t af_next;
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
  fprintf(stderr, "fast bands %u, exact bands %u (not plain %u, irregular %u, atlas full %u), atlas tiles used %u, clobbered %d\n", river_fast_bands, river_slow_bands, river_slow_why[0], river_slow_why[1], river_slow_why[2], af_next, clobbered);
#else
  fprintf(stderr, "exact build, clobbered %d\n", clobbered);
#endif
  return 0;
}

