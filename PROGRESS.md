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
