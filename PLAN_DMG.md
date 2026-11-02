# Exact river on DMG — working plan

Started 2026-11-02, branch `dmg-exact`. An experiment: `river-2600` stays as the fallback. Update this file as tasks finish; it is the place for numbers.

## Why this is worth trying

- The user played `roms/river_raid_slow.gb` (exact river, GBC at single speed, the DMG CPU proxy) for some time and called it "truly acceptable". Measured: 1.00 / 0.97 / 0.95 passes/frame fast on start indexes 0 / 2 / 3, 0 / 2 / 2 clamps per 10 s, cruise 1.00.
- So CPU no longer forces the blocky 2600 river. The open question is memory: a DMG has one VRAM bank.
- This reverses the earlier "DMG: 2600 is the only mode" line in `docs/PROPOSAL_2600_RIVER_MODE.md` as an experiment. The user asked for it.

## Rules for this work

- **Keep the 2600 work buildable.** Do not delete or break `RIVER_2600`, `RIVER_BLOCKY`, `RIVER_NOFEAT`, `RIVER_FAST`. `.\build_gbc.ps1 -River2600` and the host checks must keep passing. Make the DMG support independent of the `RIVER_*` flags, so that a blocky river on DMG stays one flag away if the exact river does not fit.
- **Keep the GBC ROMs working.** DMG code goes behind one flag (suggested `PLATFORM_DMG`, `build_gbc.ps1 -Dmg`). The GBC builds must not change behaviour.
- **Do not merge to `main`** and do not touch `river-2600`: the user decides both.
- **A shared mistake passes a self-comparison** (`info/LESSONS.md` item 18): run `tests/gb/sprite_check.mjs` and look at a screenshot before any handover.

## Tile budget (river measured 2026-11-02, task 1)

Hardware: 384 tiles in three blocks of 128. Sprites reach `$8000` and `$8800`; BG and window (with LCDC bit 4 clear) reach `$9000` and `$8800`. So BG + window <= 256, sprites <= 256, and `$8800` is the only shared block.

| Use | Tiles | Source |
|---|---|---|
| Fixed river tiles | 3 | `TILE_RIVER_BASE` (land, water, black; the road tiles 253/254 are margins, unused on GB) |
| HUD (window) | 36 | `HT_COUNT` in `hud_gbc.c` |
| Sprites | 167 | Jet 6, cursor 1, entities 141, debris 10, missile 1, pause letters 8 |
| **Left for the river cache** | **178** | |
| The cache as it works today | **259-266** | **Does not fit** (task 1). The old "182" was the 18-row window, not the real lifetime |

Savings without visible change: **38 of the 141 entity tiles are exact duplicates** (4 blank), which leaves 216 for the cache (the BG side caps it at 217). Needs a tile index table in `ent_draw`/`ent_draw_asm`, today `first + offset`. Also possible: 10 of the 12 idle high score tiles (one digit set; a small look change).

## Tasks, in order

### 1. Measure what the river really needs (host only, decides everything else)
- [x] New harness `tests/host_tile_budget.ps1` + `tests/host/tile_budget_main.c`: the real spawner (so roads and object avoidance are in the river, unlike `tiles_main.c`), the main loop's schedule, cache size as a parameter (`-Cache`).
- [x] **Geometry finding.** `publish_pass` sets `SCY = scroll + 82`, because logic keeps NES coordinates and the view is the bottom 128 of 210 lines. A row is written at `scroll - 16`, so **98 px above the view top**, and lives 226 px (28 tile rows) from upload to leaving the bottom. "Written 27 rows ago" is therefore **on screen**: the bottom ~10 px.
- [x] Measured: 60,000 tile rows, 4 start levels, `-JobCols 10`, fast. Cruise and slow give the same or less (20,000 rows). Pacing jitter (every 20th or 7th pass taking 2 frames) does not raise it. Runs above 65,535 rows are invalid (`river_distance` wraps; the script refuses them).

  | Upload delayed so the row is written ... | `view_off` | Slots needed, lag 0 / 2 / 6 px | If tiles were shared across rows |
  |---|---|---|---|
  | As today, 98 px above the view | 82 | **259-266** | 225-244 |
  | 56 px above | 40 | 216 / 222 / 223 | 189-198 |
  | 32 px above | 16 | 200 / 206 / 206 | 165-179 |
  | 24 px above | 8 | 193 / 200 / 200 | 159-170 |
  | 16 px above | 0 | 186 / 193 / 193 | 152-165 |

  Lag is how far the shown scroll trails the logic when an upload lands (a pass publishes at its end; 1.5 px/frame at fast, so 2 is typical, 6 is 4 frames late).
- [x] **Independent check:** the real allocator with the clobber test decided from map row and SCY alone goes from >0 to exactly 0 at the predicted size, in all 6 cases tried (216, 223, 200, 206, 186, 193) on all 4 starts.
- [x] **The shipped caches have this too:** at 250 slots and `view_off` 82: 67-151 clobbered cells per 60,000 rows, age 27-28 rows, in the bottom ~10 px. Rare and brief; nobody has reported it. A finding only, not fixed here.
- Queue size if uploads are delayed (peak uploads in q consecutive tile rows): q8 91, q9 98, q10 108, q11 118, q12 127 tiles. WRAM has ~6 KB free in the exact build (`s__HEAP` `$C674`; `$D000` holds the OAM page).

### 2. VRAM layout
- [ ] Decide the map of ids. Today river ids are 3..252, HUD ids 0..35 in VRAM bank 1, sprite ids 64..247 in bank 1. Suggested: sprites fill `$8000` and spill into `$8800`; the river cache takes `$9000` and what is left of `$8800`, so its slot numbers are no longer one contiguous range.
- [ ] `ent_draw_asm` hard-codes `ENT_TILE_BASE` 88 (compile-time assert); `entity_gfx` ids may need regenerating with `tools/gen_entity_gfx.py`.

### 3. DMG build variant
- [ ] `build_gbc.ps1 -Dmg`: no `-Wm-yC`, and the header check must not require `$0143 = $C0`. Boot must not stop at `gbc_required()`, nor call `cpu_fast()`.
- [ ] **VBK trap:** on a DMG `VBK_REG` writes do nothing, so anything written "to bank 1" lands in bank 0 and corrupts it. Places: `main()` sprite upload, `hud_gbc_init()` (tiles and window attributes), the attribute pass in `river_emit_stripe`, `river_slot_attr`. Grep `VBK_REG` for the rest.
- [ ] `S_BANK` (`set 3, a` in `oam_camera_core`, and `oam_camera_slow`) is ignored by a DMG, so it is harmless; the palette bits differ (see task 4).
- [ ] The second OAM page at `$D000` and the publish pacing work the same on a DMG.

### 4. Colour to grey
- [ ] BG: one `BGP`. No per-row palette, so the land-colour flip and the coloured road go; the road's black edges and centre line are pixels and stay. The river's colour indexes are 0 outline, 1 land/road, 2 hills/road line, 3 water. Land and water must read clearly apart.
- [ ] Sprites: two palettes (`OBP0`, `OBP1`, prop bit 4) with 3 shades, against 4 GBC palettes today (`spr_palette` in `main.c`, `ent_gfx_pal`, the balloon's basket rule in `ent_draw_asm` and `ent_draw`). Check each object's contrast on land and on water.
- [ ] HUD: palette cells go; the low-fuel warning needs another signal (`hud_update`). The bridge flash becomes a `BGP` change (`ent_water_color`).

### 5. Test it
- [ ] Find how Gearboy runs it as a DMG: `gearboy/config.ini` has `ForceDMG = false`, and a ROM without the CGB flag may be detected by itself. Neither is tried yet. Confirm with `get_media_info` / `get_cpu_status`.
- [ ] Update `sprite_check.mjs` and `profile_play.mjs` if ids or symbols move. `-OamCheck` and `-RiverCheck` still apply.
- [ ] Measure like the table in `PLAN_2600.md`: fast + cruise, start indexes 0 / 2 / 3, clamps. Hand the user a ROM and a checklist.

## Gates

| Gate | Value |
|---|---|
| Tiles (Z) | All must fit in 384 with no on-screen clobber, measured on the host |
| Fast flight (A), proposed, the user to confirm | About 0.95 passes/frame, matching the proxy ROM the user accepted; a real DMG also saves the GBC attribute writes |
| Final | The user's playtest |

## If it does not work

Fallback order, the user decides: simplify the river's features (fewer unique tiles), then `-River2600` on the same DMG plumbing (32 atlas tiles, 210-slot exact ring for islands and roads, and that ring would need the same measurement), then stay GBC only.
