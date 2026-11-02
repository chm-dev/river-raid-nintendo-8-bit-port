# HANDOFF — read this first

Short, current, rewritten every session. Rules are in `AGENTS.md`; the plan is `PLAN_2600.md`; history is `PROGRESS.md` (grep it, do not read whole). The proposal is `docs/PROPOSAL_2600_RIVER_MODE.md`.

**Last updated:** 2026-11-02 · **Branch:** `river-2600` · **Phase:** Stage 1b performance done on the numbers: gates met (X 1.00, Y 0.99 against 1.00 / 0.60). **Waiting for the user's playtest of the proxy ROM.**

## Where we are

- GBC port finished on `main` (architecture: `archive/gbc/HANDOFF.md`). This branch adds a **2600-style river** so a DMG build becomes possible.
- The mod (`xex/River Raid 2600.xex`) is the original cart with 961 patched bytes (`docs/MOD_2600_SPEC.md`). Its river = the original generator with straight banks (patch `$A89F`), no bridge wobble, more features. Captures: `assets/atari/river2600/`.
- **River (GBC, `build_gbc.ps1 -River2600`):** flags `RIVER_2600`, `RIVER_BLOCKY` (edges on an 8 px grid), `RIVER_NOFEAT` (no houses/trees), `RIVER_FAST` (a tile row of plain rows becomes ids from a lazily built edge atlas). Islands, roads, narrow rivers and jumps above 16 px take the exact path.
- **Pacing changed:** a finished pass publishes scroll and an OAM page (`publish_pass`, second page at `$D000`) and the VBlank handler shows it; the next pass starts at once if a frame has gone by. `vsync()` used to turn a pass of 1.01 frames into 2.
- **Assembly is allowed now** (user). sm83 routines: `oam_camera_core`, `ent_draw_asm`, `ent_emit_asm`, `ent_follow_asm`, `ent_scan_asm`, `river_scan_solid`, `river_mixed_col`, `river_copy_line_asm`, `river_pat_equal_asm`, `river_fill_core`. All have a C reference; the host harness only runs the C.
- **Measured** (fast flight, 10 s; start index 0 / 2 / 3; details in `PLAN_2600.md`):

| ROM | Passes/frame | Clamps | Before Stage 1b |
|---|---|---|---|
| 2600, double speed | 1.00 / 1.00 / 1.00 | 0 / 0 / 0 | 0.97 (index 0) |
| 2600, DMG proxy | 0.99 / 0.99 / 0.99 | 0 / 2 / 1 | 0.57 (index 0) |
| Default exact river, double speed | 1.00 (index 0) | 0 | 0.76 |

Cruise is 1.00 with 0 clamps on both 2600 ROMs.

## Next steps

1. **User playtests `roms/river_raid_2600_slow.gb`** (checklist below). Rebuild: `.\build_gbc.ps1 -River2600 -DmgProxy -Name river_raid_2600_slow`.
2. Act on what the playtest shows. If hitches remain, the known peaks are in `PLAN_2600.md` ("What is left in the peaks"): first-use atlas builds, road rows, band replays (step 5).
3. Stage 1 check and the DMG decision are the user's. Gates A and Z are unset.

Parked: **smooth blocky edges** (user's idea, in `BUGS&FEATS.md`; do not start before the playtest unless asked), island bands on the fast path (step 6: not needed for the gates, and needs the user's OK to move code between banks), houses/trees snapped to the grid.

**Per-step check:** `host_river_fast.ps1 -Rows 20000 -JobCols 10`, `host_river.ps1 -Blocky` and `host_river.ps1` (exact), `.\build.ps1` (NES, then rebuild GBC), bank 0 end in the `.map`, profile fast + cruise with `.\build_gbc.ps1 -ProfExport -River2600 [-DmgProxy] -Name <rom>` and `$env:GB_NOI='roms/<rom>.noi'; node tests/gb/profile_play.mjs fast|cruise 40` (`$env:GB_START_LEVEL=n` for another start). **After touching assembly or its C twin:** `-OamCheck` and `-RiverCheck` ROMs, run with `profile_play.mjs`: all error counts must be 0.

## Playtest checklist (`roms/river_raid_2600_slow.gb`; `roms/river_raid_2600.gb` for double speed)

- **Hold Up on level 1 and on a later start level (Up in the attract menu):** is fast flight smooth now? Note any hitch and where (expected: briefly on first use of new edge shapes in the first seconds, and at bridges).
- **Sprites against the river (new pacing and new assembly):** enemies, fuel and the jet sit still relative to the water; no flicker, no sprite left behind, none popping in at the top of the view. Mirrored ships and helicopters, the balloon's brown basket, explosions, bridge debris, tank and shell look right.
- **Collision:** the jet dies exactly where the bank is drawn; enemies and fuel hit where drawn (the player scan is new assembly); shots hit.
- **HUD:** score, fuel gauge, lives and bridge number update promptly, including the low-fuel colours and the bridge flash.
- **Islands (even levels), bridges, roads:** look right, the land colour flips at a bridge.
- **Flows:** lives lost and restart, game over, pause menu (letters, cursor, resume, exit), level change, attract screen.
- River look: straight edges in 8 px steps, no houses or trees. Does it read as a river and feel like a 2600?

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
| Profiling | `tests/gb/profile_play.mjs` (with `tools/prof_summary.py`), `tests/gb/trace_prof.mjs` + `tools/trace_hist.py` (per-phase, worst passes; sees inside banked calls) |
| Atari behaviour / RE tools | `docs/ATARI_RE_SPEC.md`, `info/LESSONS.md`, `tools/README.md` |
| GBC / NES history | `archive/gbc/`, `archive/nes/` |

## Known gaps

- Bank 0 in the 2600 build ends at `$3F40` (~190 bytes free; `-ProfExport` no longer adds to it). The check ROMs drop the "GBC REQUIRED" text to fit. **Houses/trees were dropped, not snapped.**
- Measured over 10 s windows from three start levels, with the jet invulnerable and only holding Up. Not measured: long runs, heavy shooting, level 13+ fast helicopters at speed.
- The proxy is a GBC at single speed: it still pays for the GBC attribute writes and has no real-DMG test behind it. Sound is ported in the ROM; a DMG build is not started.
- `river_mixed_col` needs `river_lut_p1` to follow `river_lut_p0` in ROM (declaration order in `river_lut.h`); `-RiverCheck` shows it if that ever changes. `ent_draw_asm` and friends have compile-time asserts for their constants.
- Host-only finding, not new: the default exact build (not 2600) reports `clobbered 134` on start index 3 (a tile id reused while still on screen, 27 rows after it was written). Same before and after this work; not investigated.
- `ent_skipped` pass kinds and the host check do not cover roads, bridges, the land-colour flip and restarts in the fast path (the ROM self-checks do cover the conversion there). The playtest covers them.
- Edges: left rounds down, right rounds up, so the player may be up to 7 px from a drawn bank before the true bank.
- Atlas: 32 tiles used over 20,000 rows x 4 start levels, 40 reserved, exact ring 210 slots (peak 182).
- `RIVER_GRID=4` fails `host_river_fast.ps1`; only grid 8 is verified. `river.c` emits harmless `always false` / `unreachable code` warnings.
- The "GBC REQUIRED" screen has never run on a plain Game Boy.

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
