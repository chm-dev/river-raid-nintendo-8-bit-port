# HANDOFF — read this first

Short, current, rewritten every session. Rules are in `AGENTS.md`; the plan is `PLAN_2600.md`; history is `PROGRESS.md` (grep it, do not read whole). The proposal is `docs/PROPOSAL_2600_RIVER_MODE.md`.

**Last updated:** 2026-11-02 · **Branch:** `river-2600` · **Phase:** Stage 1 built and measured; waiting for the user's playtest and the Stage 1 check. Gates X=1.00 / Y=0.60 are close but **not met**.

## Where we are

- GBC port finished on `main` (architecture: `archive/gbc/HANDOFF.md`). This branch adds a **2600-style river** so a DMG build becomes possible.
- The mod (`xex/River Raid 2600.xex`) is the original cart with 961 patched bytes (`docs/MOD_2600_SPEC.md`). Its river = the original generator with straight banks (patch `$A89F`), no bridge wobble, more features. Captures: `assets/atari/river2600/`.
- **Implemented (GBC, `build_gbc.ps1 -River2600`):** flags `RIVER_2600` (mod generator), `RIVER_BLOCKY` (edges on an 8 px grid, ramps one 8 px step per 8 rows; `RIVER_GRID=4` is experimental), `RIVER_NOFEAT` (no houses/trees), `RIVER_FAST` (a tile row of plain rows becomes tile ids from a lazily built edge atlas, no rendering). Islands, roads, narrow (8 px) rivers and jumps above 16 px still take the exact path. `tools/river_model.py` has the same switches (`RIVER_2600=1`, `RIVER_BLOCKY=1`, `RIVER_GRID`).
- **Measured** (level 1, `PLAN_2600.md` tables): fast flight 0.76 -> 0.97 passes/frame at double speed; DMG proxy (`-DmgProxy`) 0.37 -> 0.57. `river_update` avg 33.9k -> 8.9k units.

## Next steps

1. **User:** playtest `roms/river_raid_2600.gb` (checklist below). Decide whether the blocky look is acceptable and whether to go on.
2. **Islands on the fast path** (even levels, 25% of bands; the level-1 profile never runs them, so their peak is still the old one). Same shape as plain edges: 4 boundaries, jumps at row 7.
3. **Remove the 110k `river_band_replay` peaks** (about once per 25 s of fast flight): allow a third jump size (24 px, from grid rounding) and a minimum river width of 16 px in blocky mode (model + C), or spread the replay over passes.
4. Profile an island stretch (a profile that starts on level 20/50, `river_init` variant 2 or 3), re-measure X and Y, then the Stage 1 check.
5. Not yet done: sound (not ported), houses/trees snapped to the tile grid (the user allowed dropping them). `oam_camera` (9k units per pass, 14k at the proxy clock) and the stripe write are the next biggest per-pass costs.

## Playtest checklist (`roms/river_raid_2600.gb`)

- River edges are straight and move in 8 px steps; no houses or trees. Does it read as a river? Does it feel like a 2600?
- Fast flight (hold Up) feels smooth at 60 Hz, cruise too; note any hitch (expected: an occasional one when the river narrows or at an island).
- Collision: the jet dies exactly where the bank is drawn (left and right, also on slopes).
- Islands (even levels, e.g. after the first bridge): look and collision; fuel and enemies appear where they should.
- Bridges and the road across the land at the start; lives lost and restart; level change.
- Compare with `roms/river_raid.gb` (build `.\build_gbc.ps1`) if in doubt.

## Decisions not to reopen

- **Logic keeps NES coordinates.** Only drawing uses the camera.
- **Collision and drawing share one edge description** (`rg_row_left/right`, quantised in blocky mode).
- **GBC remains the testbed.** DMG work waits for the Stage 1 check and the user's approval.
- **Performance before authenticity** (user, 2026-11-02): no 2600 ROM or emulator unless this path fails. Features may be dropped.
- **Gates:** X = 1.00, Y = 0.60.
- **Fast flight stays at 1.5 px/frame**, **HUD art from `charset.bin`**, **sound values from live Atari captures**, **the user runs the playtests**.

## Where to look

| Topic | Where |
|---|---|
| Proposal, stages, gates, decisions | `docs/PROPOSAL_2600_RIVER_MODE.md` |
| Working plan, task checklists, numbers, log | `PLAN_2600.md` |
| The mod | `docs/MOD_2600_SPEC.md`, `assets/atari/river2600/` |
| Rules, toolchain, Gearboy MCP | `AGENTS.md` |
| River code | `src/river.c` ("Band fast path" section), `tools/river_model.py` |
| Host checks | `tests/host_river.ps1` (`-Mode2600`, `-Blocky [-Grid 4]`), `tests/host_river_fast.ps1` (fast path = exact path, pixel for pixel) |
| Builds | `.\build_gbc.ps1 [-River2600] [-DmgProxy] [-ProfExport] [-Define RIVER_GRID=4]`; `.\build.ps1` (NES) |
| Profiling | `tests/gb/run_emu.ps1 tests/gb/profile_play.mjs fast\|cruise` (set `GB_NOI` for another ROM) |
| Atari behaviour / RE tools | `docs/ATARI_RE_SPEC.md`, `info/LESSONS.md`, `tools/README.md` |
| GBC / NES history | `archive/gbc/`, `archive/nes/` |

## Known gaps

- Bank 0 is nearly full in the 2600 build: ~350 bytes free (~60 with `-ProfExport`). `river_debug_state` and the stats counters are compiled out there (`RIVER_STATS` is host only). **Houses/trees were dropped, not snapped, and the island fast path will not fit** unless code leaves bank 0 (a structural choice for the user).
- Untested on the host: roads, bridges (mixed fast/exact bands, `river_band_replay`), the land-colour flip and restarts (atlas reset). The checklist covers them; a mixed band costs one 110k replay.
- Edges: left rounds down, right rounds up (drawn river contains the true river), so the spawner's objects stand on water; the player may be up to 7 px from a drawn bank before the true bank.
- Atlas: 32 tiles used over 20,000 rows x 4 start levels, 40 reserved, exact ring 210 slots (peak 182). `host_river_fast.ps1` flags clobbered visible cells.
- The default (exact) GBC build is +7 bytes of code against the pre-2600 ROM; the host check and the NES build pass.
- `river.c` emits harmless `always false` / `unreachable code` warnings (the 2600 flags make more).
- `RIVER_GRID=4` fails `host_river_fast.ps1` (the atlas fills up; a mismatch not yet analysed). Only grid 8 is verified.
- The "GBC REQUIRED" screen has never run on a plain Game Boy. Player and entity group costs show only as `___sdcc_bcall_ehl` / `SUB_` names.

## End of session (keep this cheap)

1. **Rewrite and trim this file.** Update the date, move finished items out. Check the size: `(Get-Content HANDOFF.md).Count` (target ~100, trim past ~150).
2. **Append one entry to `PROGRESS.md` without reading it.** The closing `'@` must stay at column 0:

```powershell
Add-Content -Path PROGRESS.md -Encoding utf8 -Value @'

### YYYY-MM-DD - Short title (branch `name`)

- What changed and why; ground truth used; build line; tests run and results.
'@
```

3. Do not edit `AGENTS.md` for status.
