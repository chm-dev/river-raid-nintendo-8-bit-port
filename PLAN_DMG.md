# Exact river on DMG — working plan

Started 2026-11-02, branch `dmg-exact`. An experiment: `river-2600` stays as the fallback. Update this file as tasks finish; it is the place for numbers.

**Status:** tasks 1-5 done. **The user played `roms/river_raid_dmg.gb` on 2026-11-02: "absolutely flawless", every checklist item passes.** Two colour requests followed (task 4); the user confirmed those too. The user chose the delay queue and tile sharing on 2026-11-02, after the task 1 report.

## Why this is worth trying

- The user played `roms/river_raid_slow.gb` (exact river, GBC at single speed, the DMG CPU proxy) for some time and called it "truly acceptable". Measured: 1.00 / 0.97 / 0.95 passes/frame fast on start indexes 0 / 2 / 3, 0 / 2 / 2 clamps per 10 s, cruise 1.00.
- So CPU no longer forces the blocky 2600 river. The open question was memory: a DMG has one VRAM bank.
- This reverses the earlier "DMG: 2600 is the only mode" line in `docs/PROPOSAL_2600_RIVER_MODE.md` as an experiment. The user asked for it.

## Rules for this work

- **Keep the 2600 work buildable.** Do not delete or break `RIVER_2600`, `RIVER_BLOCKY`, `RIVER_NOFEAT`, `RIVER_FAST`. `.\build_gbc.ps1 -River2600` and the host checks must keep passing. DMG support is independent of the `RIVER_*` flags.
- **Keep the GBC ROMs working.** DMG code goes behind `PLATFORM_DMG` (`build_gbc.ps1 -Dmg`). The GBC, 2600 and NES ROMs are byte-identical to before this work: compare hashes after touching shared code.
- **Do not merge to `main`** and do not touch `river-2600`: the user decides both.
- **A shared mistake passes a self-comparison** (`info/LESSONS.md` item 18): run `tests/gb/sprite_check.mjs`, `tests/gb/dmg_check.mjs` and look at a screenshot before any handover.

## Tile budget (measured 2026-11-02)

Hardware: 384 tiles in three blocks of 128. Sprites reach `$8000` and `$8800`; BG and window (with LCDC bit 4 clear) reach `$9000` and `$8800`. So BG + window <= 256, sprites <= 256, and `$8800` is the only shared block.

| Use | Before | Now | How |
|---|---|---|---|
| Fixed river tiles | 3 | 3 | Land, water, black. The road tiles 253/254 are margins, unused on GB, and not uploaded |
| HUD (window) | 36 | 26 | White digits are the digits: on DMG all ink is one shade |
| Sprites | 167 | **124** | Identical tiles shared at boot, across all sprite groups (`dmg_sprite_data`). `$8000` holds them all; 4 free |
| River cache needed | 259-266 | **206** | Delay queue, row uploaded 32 px above the view |
| River cache allocated | | **226** | ids 3..228: 20 spare |

Layout (`gbc/config_gbc.h`): BG 0..2 fixed, 3..228 cache, 229..254 HUD, 255 free (kept for a sprite if the count ever passes 128). Sprite ids in the logic are unchanged; `oam_camera` translates them through `dmg_tile_map` at `$D100`.

## Tasks

### 1. Measure what the river really needs: done
- `tests/host_tile_budget.ps1` + `tests/host/tile_budget_main.c`: the real spawner (so roads and object avoidance are in the river, unlike `tiles_main.c`), the main loop's schedule, cache size as a parameter.
- **Geometry finding.** `publish_pass` sets `SCY = scroll + 82`, because logic keeps NES coordinates and the view is the bottom 128 of 210 lines. A row is written at `scroll - 16`, so **98 px above the view top**, and lives 226 px (28 tile rows) from upload to leaving the bottom. The old "182" was the 18-row window, not this lifetime.
- Measured: 60,000 tile rows, 4 start levels, `-JobCols 10`, fast. Cruise and slow give the same or less (20,000 rows). Pacing jitter (every 20th or 7th pass taking 2 frames) does not raise it. Runs above 65,535 rows are invalid (`river_distance` wraps; the script refuses them).

  | Upload delayed so the row is written ... | `view_off` | Slots needed, lag 0 / 2 / 6 px | If tiles were shared across rows |
  |---|---|---|---|
  | As today, 98 px above the view | 82 | **259-266** | 225-244 |
  | 56 px above | 40 | 216 / 222 / 223 | 189-198 |
  | **32 px above (chosen, `RQ_DUE`)** | **16** | **200 / 206 / 206** | 165-179 |
  | 24 px above | 8 | 193 / 200 / 200 | 159-170 |
  | 16 px above | 0 | 186 / 193 / 193 | 152-165 |

  Lag is how far the shown scroll trails the logic when an upload lands (a pass publishes at its end).
- **Independent check:** the real allocator, with clobbers decided from map row and SCY alone, goes from >0 to exactly 0 at the predicted size in all 6 cases tried (216, 223, 200, 206, 186, 193) on all 4 starts.
- **The GBC exact ROM has this too:** at 250 slots: 67-151 clobbered cells per 60,000 rows, age 27-28 rows, so the bottom ~10 px. The NES has the same lifetime by geometry, not measured. See `BUGS&FEATS.md`. Not fixed.

### 2. VRAM layout: done
See the budget above.

### 3. DMG build variant: done
- `build_gbc.ps1 -Dmg`: no `-Wm-yC`, header check wants no CGB flag. No `gbc_required()`, no `cpu_fast()`.
- All VBK writes are behind `!PLATFORM_DMG`: `main()` sprite upload, `hud_gbc_init()`, `river_start`, the attribute pass in `river_emit_stripe`.
- **Delay queue** (`river.c`): the job copies patterns and a finished row's ids to WRAM; `river_drain()` uploads a row when it is `RQ_DUE` px above the view, at most `RQ_PASS_TILES` tiles per pass. Patterns at `$D200` (128 tiles, measured peak 118 in 11 rows), rows in `_DATA`. A full queue makes the job yield, as a full buffer always did. Generator, spawner and collision are untouched.
- Assembly: `river_vram_copy_asm` (1.8k units per tile against 3.8k in C). `-RiverCheck` reads VRAM back against the source; the host runs the C path.

### 4. Colour to grey: done
- `BGP`: outline black, land light grey, hills / road line dark grey, water white. The land-colour flip and road colour are gone; the road keeps its pixels. Bridge flash: water goes dark grey (`DMG_BGP_FLASH`).
- Sprites: colour palette 0 → `OBP0`, 1..3 → `OBP1`, in `oam_camera`. `OBP0`: black, light grey, dark grey, so FUEL is F and E dark, U and L light (user). No white, or U and L vanish on the water.
- Per object, at upload (`entities_init_graphics`, before tiles are shared), since `OBP0`'s colour 2 went light: enemy jets and their explosion 2 → 3, to stay dark. Balloon on `OBP1` (`ent_gfx_pal`, DMG table in the generator) with 1 → 2 and 2 → 3: light grey body, dark grey stripe and basket (user). Its explosion now draws on `OBP1` too.
- A light grey object on light grey land has no outline: FUEL's U and L, and the balloon's body. Both stay over water in normal play.
- HUD: ink is colour 3 on 0, the same index as water, so by the code the bridge flash also flickers the HUD text (not yet seen). If it bothers the user: `BGP` could be reset for the HUD in the LYC handler. Low fuel: E and F go out while the note sounds.

### 5. Test it: done, the user's playtest is next
- Gearboy detects the ROM by itself: `is_cgb: false`, boot AF `01B0`.
- `tests/host_river_queue.ps1`: the picture equals the plain build's, cell by cell, on all 4 starts over 20,000 rows; no on-screen clobber at 226 in 24 runs (jitter 0 / 20 / 7, lag 0 / 6). Negative control: cache 190 gives 36.
- `tests/gb/dmg_check.mjs`: running as a DMG; 141 entity tiles match `entity_gfx.c` through the map; river rows on screen have their water where collision says (a coarse test: plain rows only, the middle of the channel). `sprite_check.mjs` reads the map too.
- Screens looked at: flight, roads, bridge, islands, every enemy, fuel, pause menu, low fuel, crash and restart.

| `river_raid_dmg`, 10 s, invulnerable jet | Start 0 | Start 2 | Start 3 |
|---|---|---|---|
| Fast, passes/frame | 0.99-1.00 | 0.98 | 0.96 |
| Fast, clamps | 0-1 | 2 | 1-2 |
| Cruise, passes/frame | 1.00 | 1.00 | 1.00 |
| Proxy ROM the user accepted, fast | 1.00 | 0.97 | 0.95 |

## Gates

| Gate | Value | Result |
|---|---|---|
| Tiles (Z) | All must fit in 384 with no on-screen clobber, on the host | Pass: 206 needed, 226 allocated |
| Fast flight (A), proposed, the user to confirm | About 0.95 passes/frame | Pass: 0.96 at worst |
| Final | The user's playtest | **Pass** (2026-11-02), before the colour requests |

## Not verified

- Nothing has run on a real DMG; Gearboy's DMG mode is the only evidence. Worth watching on hardware: the 2-writes-per-STAT-test timing in `river_vram_copy_asm`, and how the shades read on the real LCD (white water against light grey land).
- Cross-row tile sharing (the last column above) would need 165-179, but costs CPU on every column. Not needed.
- `-Dmg -River2600` builds (bank 0 ends `$394E`) but has never run, and its ring is unmeasured.
- Boot on the DMG ROM takes ~1.5-1.7 s with a blank screen (measured in Gearboy); the tile sharing is part of it.

## Dual ROM: next, before merging to `main` (user, 2026-11-02; not started)

One ROM that is the full GBC game on a GBC and the DMG game on a DMG. An outline from reading the code, nothing built or measured yet:

- **Header and boot:** `$0143 = $80`. Branch on `_cpu == CGB_TYPE`: double speed and colour palettes, or single speed and `BGP`/`OBP0`/`OBP1`. The build check must accept `$80`.
- **Turn `PLATFORM_DMG` (32 lines in 8 files) into runtime branches or shared code.** Keep hot paths free of checks.
- **VRAM layout:** the DMG layout on both, so the GBC cache goes from 250 to 226 slots, with the queue on both. The GBC HUD's 10 white digits can stay in bank 1. LCDC bit 4 clear on both.
- **OAM attributes:** one byte can serve both, as a DMG ignores bit 3 and a GBC in colour mode ignores bit 4. `dmg_tile_map` becomes identity on a GBC, or the GBC uploads through the same allocator.
- **Hardest part:** the GBC's per-row attributes (land flip, road colour) must go through the queue with their row, and `river_slot_attr` with them. `river_vram_copy_asm` was reasoned for single speed; check it at double speed too.
- **Sprite recolouring** (balloon, enemy jet) runs on a DMG only, and the GBC keeps the original `ent_gfx_pal`, so that table's DMG variant becomes a runtime choice.
- **Bank 0:** about 1.7 KB free in the GBC build. Move code to a higher bank if it does not fit; check the `.map`.
- **Verification:** the GBC ROM can no longer be byte-identical, so that proof goes. Compare against today's ROMs as reference: `host_river_queue.ps1`, profile against `river_raid` (1.00 / 1.00 / 1.00) and `river_raid_dmg` (0.99-1.00 / 0.98 / 0.96), `-OamCheck`, `-RiverCheck`, `sprite_check.mjs` and `dmg_check.mjs` in both modes, screenshots with colours on GBC. **Both modes need the user's playtest.** The NES ROM must stay byte-identical. Gearboy's `ForceDMG` in `config.ini`, or the detection for a `$80` ROM, needs finding out: a dual ROM may boot as CGB by default.

## If it does not work

Fallback order, the user decides: simplify the river's features (fewer unique tiles), then `-River2600` on the same DMG plumbing (32 atlas tiles, 210-slot exact ring for islands and roads, and that ring would need the same measurement), then stay GBC only.
