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

### 2026-11-02 - Fix: entity sprites invisible in the Stage 1b ROMs (branch `river-2600`)

- User playtest: all sprites but the jet invisible, both ROMs. `ent_emit_row`'s pointer rewrite and `ent_emit_asm` wrote the NES byte order (y, tile, prop, x), not the GB's (y, x, tile, prop); `-OamCheck` compared them with each other. Fixed both; the C uses named fields. New `tests/gb/sprite_check.mjs` checks the hardware OAM against `ent_x`/`ent_y`: 26/26 missing before, 0/49 after. Screenshots show enemies. Re-measured: proxy 0.99, double speed 1.00, as before. Playtest pending again.
