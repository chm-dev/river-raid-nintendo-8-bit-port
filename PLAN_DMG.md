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

## Tile budget (counted 2026-11-02; the river line is an old figure)

Hardware: 384 tiles in three blocks of 128. Sprites reach `$8000` and `$8800`; BG and window (with LCDC bit 4 clear) reach `$9000` and `$8800`. So BG + window <= 256, sprites <= 256, and `$8800` is the only shared block.

| Use | Tiles | Source |
|---|---|---|
| Fixed river tiles | 3 | `TILE_RIVER_BASE` |
| River cache peak | **182** | Archive figure, **not re-measured**; the cache has 250 slots |
| HUD (window) | 36 | `HT_COUNT` in `hud_gbc.c` |
| Sprites | 167 | Jet 6, cursor 1, entities 141, debris 10, missile 1, pause letters 8 |
| **Total** | **388 of 384** | 4 over, with no margin |

Likely savings without visible loss: idle high score's 12 tiles (white digits, HI), pause letters shared with HUD glyphs through `$8800`, duplicate or blank entity tiles. A smaller cache may be enough (task 1).

## Tasks, in order

### 1. Measure what the river really needs (host only, decides everything else)
- [ ] `tests/host/tiles_main.c` counts a "clobber" for a tile reused while its row is still in the 32-row map. The reports seen so far say "written 27 rows ago", probably off screen and harmless. Change or extend it to count only rows that can be on screen. The view is 128 lines (16 rows, 17 with the scroll phase); check how far ahead of the view new rows are written.
- [ ] Make the cache size a build parameter on the host (`TILE_RIVER_COUNT`; check `RIVER_ALLOC` wrap and the ids), and find the smallest size with no on-screen clobber: exact river with features, all 4 start levels, 20,000+ tile rows, `-JobCols 10` as on the GBC.
- [ ] Write the result in the table above and tell the user. **The decision here is the user's if it does not fit:** simplify trees and houses, drop some sprite art, or fall back to the blocky river. Fitting needs about 180 cache slots or fewer after the HUD and trimmed sprites.

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
