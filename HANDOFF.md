# HANDOFF — read this first

Short, current, rewritten every session. Rules are in `AGENTS.md`; **the plan is `PLAN_DMG.md`**; history is `PROGRESS.md` (grep it, do not read whole).

**Last updated:** 2026-11-02 · **Branch:** `dmg-exact` (from `river-2600`) · **Phase:** the exact river on DMG is confirmed by the user, colours included. Next: a single ROM for GBC and DMG, not started.

## Where we are

- **GBC port finished on `main`** (architecture: `archive/gbc/HANDOFF.md`). `main` does **not** have the Stage 1b speedups or the DMG work; they are not pushed by us and not merged. Merging is the user's call.
- **`roms/river_raid_dmg.gb` (`.\build_gbc.ps1 -Dmg -Name river_raid_dmg`) is the exact river on one VRAM bank, with no art cut.** All gates pass, including the user's playtest (2026-11-02, every checklist item).
- **The 2600 river is stale (user, 2026-11-02): not being pursued, but the code stays and keeps building:** flags `RIVER_2600`, `RIVER_BLOCKY`, `RIVER_NOFEAT`, `RIVER_FAST`, `build_gbc.ps1 -River2600`, tasks and findings in `PLAN_2600.md`.

**Fast flight, 10 s, start index 0 / 2 / 3** (playing an invulnerable jet holding Up):

| ROM | Passes/frame | Clamps |
|---|---|---|
| **DMG, exact river** (`river_raid_dmg`) | **0.99-1.00 / 0.98 / 0.96** | 0-1 / 2 / 1-2 |
| Exact river, DMG proxy (`river_raid_slow`), the user accepted it | 1.00 / 0.97 / 0.95 | 0 / 2 / 2 |
| Exact river, double speed (`river_raid`) | 1.00 / 1.00 / 1.00 | 0 / 0 / 0 |
| 2600, DMG proxy | 0.99 / 0.99 / 0.99 | 0 / 2 / 1 |

## How the DMG build fits (details in `PLAN_DMG.md`)

- **The surprise:** `SCY = scroll + 82`, since logic keeps NES coordinates. Rows are written 98 px above the view and hold a cache slot for 28 tile rows: 259-266 slots, with 178 left.
- **Delay queue** (`river.c`, `PLATFORM_DMG`): rows wait in WRAM and upload 32 px above the view: 206 slots needed, 226 allocated. Generator, spawner and collision do not change.
- **Sprites:** identical tiles shared at boot, 167 → 124. The logic keeps its ids; `oam_camera` translates through `dmg_tile_map` (`$D100`). **HUD:** 36 → 26.
- **Grey:** one `BGP`, `OBP0`/`OBP1` (`config_gbc.h`), plus per-object colour indexes at upload (`entities_init_graphics`: balloon, enemy jet). Low fuel: E and F go out with the note.

## Next steps

**Next: one ROM for both GBC and DMG, before merging to `main`** (user, 2026-11-02). Do it on `dmg-exact`; the outline is in `PLAN_DMG.md` "Dual ROM". Not started. Merging is still the user's call, after this. The user's call too: the rare bottom-row clobber on the GBC exact ROM (the dual ROM's queue should fix it there); the NES is unmeasured (`BUGS&FEATS.md`). Optional: boot time (~1.5-1.7 s blank), remaining peaks (`river_find_dup`, `river_copy_template`).

Stale (user, 2026-11-02; do not start unless asked): 2600 river work (Stage 1b steps 5-8, Stage 2), **smooth blocky edges** (`BUGS&FEATS.md`).

**Playtest checklist (passed 2026-11-02; repeat after big changes):** land and water read apart at a glance · every enemy, FUEL and the bridge are visible on water and on land · trees, houses and roads look right, no wrong tiles anywhere, including the bottom rows and just below the top of the view · fast flight feels like `river_raid_slow.gb` on start levels 1, 20 and 50 · low fuel: E and F blink · bridge shot: water flashes, and by the code the HUD text flickers with it (one `BGP`): does that bother you? · pause menu readable · crash and restart · high score after power-off · a long game past level 13 (fast helicopters).

**Per-step check:** `host_river.ps1` (exact), `host_river.ps1 -Blocky`, `host_river_fast.ps1 -Rows 20000 -JobCols 10`, `host_river_queue.ps1`, `.\build.ps1` (NES), then `.\build_gbc.ps1`, `-River2600`, `-Dmg`. **After shared code, compare ROM hashes:** GBC, 2600 and NES ROMs must be byte-identical unless the change is meant for them. Bank 0 end in the `.map`. Profile: `.\build_gbc.ps1 -ProfExport -Dmg -Name rr_dmg_prof`, `$env:GB_NOI='roms/rr_dmg_prof.noi'; node tests/gb/profile_play.mjs fast|cruise 40` (`$env:GB_START_LEVEL=n` for another start; Gearboy launch line in `AGENTS.md`). **After touching assembly or its C twin:** `-OamCheck` and `-RiverCheck` ROMs, run with `profile_play.mjs`: all error counts must be 0. **Before a handover:** `tests/gb/dmg_check.mjs` and `tests/gb/sprite_check.mjs` on the player ROM, each on a freshly started emulator, and look at a screenshot.

**Scripting trap:** do not pass build switches as a string array with `@a` to `build_gbc.ps1`: they bind as positional arguments and the build silently does not run. Its "Built" line goes through `Write-Host`, so a captured output does not show it either: check the file time.

## Decisions not to reopen

- **Logic keeps NES coordinates.** Only drawing uses the camera. This is why the delay queue exists instead of a shorter generator lead.
- **Collision and drawing share one edge description** (`rg_row_left/right`).
- **Performance before authenticity** (user, 2026-11-02). Features may be dropped, but cutting art is the user's call.
- **The slow exact ROM's feel is acceptable** (user's playtest, 2026-11-02): that is the reference for DMG.
- **Delay queue and shared tiles for the DMG** (user, 2026-11-02). **The DMG ROM's feel is accepted** (user's playtest, 2026-11-02).
- **Fast flight stays at 1.5 px/frame**, **HUD art from `charset.bin`**, **sound values from live Atari captures**, **the user runs the playtests**.

## Where to look

| Topic | Where |
|---|---|
| The plan: tile budget, measurements, gates | `PLAN_DMG.md` |
| Performance findings, 2600 tasks and numbers | `PLAN_2600.md` |
| 2600 decision text, the mod | `docs/PROPOSAL_2600_RIVER_MODE.md`, `docs/MOD_2600_SPEC.md`, `assets/atari/river2600/` |
| Rules, toolchain, Gearboy MCP | `AGENTS.md` |
| River code | `src/river.c`, `tools/river_model.py` |
| DMG tile layout and shades | `src/gbc/config_gbc.h`, `dmg_sprite_data` in `src/gbc/main.c` |
| Host checks | `tests/host_river.ps1` (`-Mode2600`, `-Blocky`), `tests/host_river_fast.ps1`, `tests/host_tile_budget.ps1` (slots needed), `tests/host_river_queue.ps1` (the queue) |
| Builds | `.\build_gbc.ps1 [-Dmg] [-River2600] [-DmgProxy] [-ProfExport] [-OamCheck] [-RiverCheck] [-PassLog]`; `.\build.ps1` (NES) |
| Profiling | `tests/gb/profile_play.mjs` + `tools/prof_summary.py`; `tests/gb/trace_prof.mjs` + `tools/trace_hist.py` |
| Atari behaviour / RE tools | `docs/ATARI_RE_SPEC.md`, `info/LESSONS.md`, `tools/README.md` |
| GBC / NES history | `archive/gbc/`, `archive/nes/` |

## Known gaps

- **My checks only ran in Gearboy's DMG mode;** what the user's playtest ran on was not recorded. On hardware watch the 2-writes-per-STAT-test timing in `river_vram_copy_asm` and how the shades read. The GBC ROM's "GBC REQUIRED" screen has never run on a plain Game Boy.
- All timings come from 10 s windows with an invulnerable jet holding Up. Not measured: long runs, heavy shooting, level 13+ fast helicopters at speed. Tile needs are measured over 60,000 rows without player deaths or shots.
- `dmg_check.mjs`'s river test is coarse (plain rows, middle of the channel); the exact picture is proved on the host (`host_river_queue.ps1`). It and `sprite_check.mjs` run on the first start level only.
- Not looked at in a screenshot: the bridge flash, level 13+ fast helicopters and their missile.
- `-Dmg -River2600` builds but has never run, and its ring is unmeasured.
- The `need` line of `tile_budget_main.c` is meaningless in a `-DPLATFORM_DMG` build (the log tags uploads with the generator's row); its clobber count is valid.
- Bank 0: exact build ends `$3928`, 2600 build `$3F40` (~190 bytes free), DMG `$3336`. The check ROMs drop the "GBC REQUIRED" text to fit.
- `river_mixed_col` needs `river_lut_p1` right after `river_lut_p0` in ROM (order in `river_lut.h`); `-RiverCheck` shows it if that changes.
- `RIVER_GRID=4` fails `host_river_fast.ps1`; only grid 8 is verified. `river.c` emits harmless `always false` / `unreachable code` warnings.

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
