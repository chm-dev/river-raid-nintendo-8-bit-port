# PROGRESS — 2600 river mode

Append-only. Do not read whole; grep for a keyword and read the matching `### date - title` entry. Earlier history: `archive/gbc/PROGRESS.md`, `archive/nes/PROGRESS.md`.

### 2026-11-02 - Workspace prepared for the 2600 river mode (branch `river-2600`)

- Branched from `main` (`95e0074`). Moved `HANDOFF.md`, `GBC_PORT_PLAN.md`, `PROGRESS.md`, `BUGS&FEATS.md` to `archive/gbc/` with `git mv` (history kept) and added `archive/gbc/README.md`.
- New `HANDOFF.md`, `PLAN_2600.md` (Stage 1 checklist, gates, baseline table), `PROGRESS.md`, `BUGS&FEATS.md`. `AGENTS.md` updated to point at them. Proposal's Decision section records the start of Stage 1.
- Found: the mod XEX is 10,754 bytes against 8,220 for the original.
- No code changed, no build run.

### 2026-11-02 - Stage 1 of the 2600 river: mod scope, baseline, blocky fast path (branch `river-2600`)

- Mod = original cart + 961 patched bytes (booted XEX in AltirraBridge, dumped RAM). River patches: `$A89F` cmp operand `$EA` (straight banks), `$BA8F` walk table zeroed, `$A976` opcode change (feature rate, not modelled). `tools/river_validate.py` with `RIVER_CART=` reproduces the mod trace except the feature select.
- Baseline (exact river): fast 0.76 passes/frame, DMG proxy 0.37; `river_update` avg 33.9k. Written in `PLAN_2600.md`.
- Built `RIVER_2600`/`RIVER_BLOCKY`/`RIVER_NOFEAT`/`RIVER_FAST` in `src/river.c` and the model. Plain tile rows become atlas tile ids without rendering; the previous band's ids are reused on straight stretches. User gates: X=1.00, Y=0.60.
- Result: fast 0.97, DMG proxy 0.57 (level 1). Not met. Remaining: band replay peaks, islands, oam_camera.
- Tests: `host_river.ps1` default/-Mode2600/-Blocky/-Blocky -Grid 4 all match the model; `host_river_fast.ps1` (grid 8) identical on 4 start levels; NES build OK; GBC bank 0 checked (`$3E70` end without ProfExport). Playtest not run (user).

### 2026-11-02 - Stage 1b planned: performance to an acceptable DMG speed (branch `river-2600`)

- The user played `roms/river_raid_2600_slow.gb` (single-speed proxy): "almost acceptable but still not there"; the blocky look is disliked. Assessment of the remaining room (no code changed): entities+player ~40% of a pass, river ~21%, `oam_camera` ~12%.
- Added Stage 1b to `PLAN_2600.md` (8 ordered steps, results table), the "smooth blocky edges" idea to `BUGS&FEATS.md` (parked), `tools/resolve_sub.py` (names profiler `SUB_` entries), and rewrote HANDOFF next steps.

### 2026-11-02 - Stage 1b: DMG-clock performance, gates met (branch `river-2600`)

- Fast flight at the DMG proxy 0.57 -> 0.99 passes/frame, double speed 0.97 -> 1.00, default exact ROM 0.76 -> 1.00 (10 s windows; proxy start indexes 0/2/3 all 0.99, clamps 0/2/1). Work per pass 82k -> 36k units.
- New tools: `tests/gb/trace_prof.mjs` + `tools/trace_hist.py` (cycle-exact trace, per phase and worst passes; the profiler charges banked bodies to `.call_hl`), `tools/prof_summary.py`, `-PassLog` split from `-ProfExport`, `GB_START_LEVEL`.
- Cuts: HUD returns when its inputs are unchanged (8k per pass); entity rows above the view not drawn; missile search behind a flag; `oam_camera` walks live slots; river convert/copy use the NES solid-column scheme in C (the GBC ran a slow reference version); inline stripe write.
- Pacing: `vsync()` made pass time a step function. A finished pass now publishes scroll and a double-buffered OAM page to the VBlank handler; the next pass starts at once after an overrun.
- User lifted the no-assembly gate: sm83 for `oam_camera`, entity draw/emit/follow/scan and river scan/mixed/copy/compare/fill, each checked against its C (`-OamCheck`, `-RiverCheck`: 0 errors). The plan's bcall-overhead estimate was wrong: ~1% of time.
- Tests: host_river_fast 20000 rows x 4 identical, host_river -Blocky and exact match, golden diff against the previous commit identical, NES builds (3 bytes free). Playtest pending: `roms/river_raid_2600_slow.gb`.

### 2026-11-02 - Fix: entity sprites invisible in the Stage 1b ROMs (branch `river-2600`)

- User playtest: all sprites but the jet invisible, both ROMs. `ent_emit_row`'s pointer rewrite and `ent_emit_asm` wrote the NES byte order (y, tile, prop, x), not the GB's (y, x, tile, prop); `-OamCheck` compared them with each other. Fixed both; the C uses named fields. New `tests/gb/sprite_check.mjs` checks the hardware OAM against `ent_x`/`ent_y`: 26/26 missing before, 0/49 after. Screenshots show enemies. Re-measured: proxy 0.99, double speed 1.00, as before. Playtest pending again.

### 2026-11-02 - Exact river accepted at the DMG clock; next step planned (branch `river-2600`, then `dmg-exact`)

- User playtested the slow exact ROM for some time: "truly acceptable". 2600 river parked, not removed. New `PLAN_DMG.md` (tile budget 388 of 384 by count, river figure unmeasured; tasks, gates, fallbacks). Docs updated; branch `dmg-exact` created from `river-2600`. No code changed.

### 2026-11-02 - Exact river on DMG: measured, delay queue, shared tiles, ROM built (branch `dmg-exact`)

- Task 1: new `tests/host_tile_budget.ps1` with the real spawner and on-screen geometry. `SCY = scroll + 82`, so a row is written 98 px above the view and holds a slot for 28 tile rows: 259-266 slots needed, 178 left. The clobber test decided from map row and SCY alone confirms the predicted sizes (6 of 6 cases, 4 starts). The 250-slot GBC exact ROM does clobber rarely, in the bottom ~10 px; the NES has the same lifetime by geometry, not measured (`BUGS&FEATS.md`, not fixed). Reported to the user, who chose the delay queue and tile sharing.
- `build_gbc.ps1 -Dmg` / `PLATFORM_DMG`: queue in `river.c` (rows upload 32 px above the view, 206 needed, 226 allocated), `river_vram_copy_asm`, boot-time sprite tile sharing (167 to 124) with `dmg_tile_map` in `oam_camera`, HUD 26 tiles, one BGP + OBP0/1, low-fuel E/F blink, no VBK writes.
- Results: fast 0.99 / 0.98 / 0.96 passes/frame on starts 0 / 2 / 3, clamps 1 / 2 / 1-2; cruise 1.00. The accepted proxy ROM: 1.00 / 0.97 / 0.95.
- Tests: `host_river.ps1` (exact, `-Blocky`, `-Mode2600`), `host_river_fast.ps1 -Rows 20000 -JobCols 10`, new `host_river_queue.ps1` (picture identical, 0 clobbers in 24 runs, negative control 36), `-Dmg -OamCheck` and `-RiverCheck` 0 errors, new `tests/gb/dmg_check.mjs` and `sprite_check.mjs` pass. `river_raid.gb`, `_slow`, `_2600`, `_2600_slow` and `.nes` are byte-identical to before. Screens looked at. Not run on a real DMG; the user's playtest is next.

### 2026-11-02 - DMG ROM passes the user's playtest; FUEL and balloon colours (branch `dmg-exact`)

- The user played `roms/river_raid_dmg.gb`: "absolutely flawless", every checklist item passes. Then asked for FUEL in dark and light grey, and a light grey balloon with a dark grey stripe and basket.
- `OBP0` colour 2 is light grey. That would have washed out enemy jets (colour 2 only), so they and their explosion move to colour 3 at upload. Balloon on `OBP1` (`gen_entity_gfx.py`: DMG `ent_gfx_pal`), colours shifted up at upload. `dmg_check.mjs` expects those indexes.
- Tests: GBC, slow, 2600, 2600 slow and NES ROMs byte-identical; `host_river.ps1`, `host_river_queue.ps1`, `-Dmg -OamCheck` and `-RiverCheck` 0 errors, `dmg_check.mjs`, `sprite_check.mjs`; fast 1.00 / 0.98 / 0.96. Screens of balloon, FUEL and enemy jet looked at. Not yet seen by the user.

### 2026-11-02 - User confirms the rest; 2600 river and smooth edges stale (branch `dmg-exact`)

- The user confirmed the DMG colours, the high-score save and the sprite Y position. Marked `[x]` in `BUGS&FEATS.md`. The user made 2600 river mode and smooth blocky edges stale (new status `[-]`); no code changed, `-River2600` stays.

### 2026-11-02 - Next task set: one ROM for GBC and DMG (branch `dmg-exact`)

- The user wants a single ROM supporting both, before merging to `main`. Outline in `PLAN_DMG.md` "Dual ROM"; not started, no code changed.
