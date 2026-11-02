# HANDOFF — read this first

Short, current, rewritten every session. Rules are in `AGENTS.md`; **the plan is `PLAN_DMG.md`**; history is `PROGRESS.md` (grep it, do not read whole).

**Last updated:** 2026-11-02 · **Branch:** `dmg-exact` (from `river-2600`) · **Phase:** starting the exact river on DMG. Nothing of it is built yet.

## Where we are

- **GBC port finished on `main`** (architecture: `archive/gbc/HANDOFF.md`). `main` does **not** have the speedups below; they are on `river-2600` and `dmg-exact`, not pushed by us, not merged. Merging is the user's call.
- **Stage 1b made the game much faster at the DMG clock** (a GBC at single speed: `-DmgProxy`). The user played `roms/river_raid_slow.gb`, the **exact** river at that clock, and called it "truly acceptable".
- **So the next step is the exact river on a real DMG.** CPU no longer forces the blocky river; the open question is tiles. By count it needs 388 of 384, with an old, unmeasured river figure. `PLAN_DMG.md` task 1 measures it.
- **The 2600 river is parked, not dropped:** flags `RIVER_2600`, `RIVER_BLOCKY`, `RIVER_NOFEAT`, `RIVER_FAST`, `build_gbc.ps1 -River2600`, tasks and findings in `PLAN_2600.md`. Keep it building.

**Fast flight, 10 s, start index 0 / 2 / 3** (playing an invulnerable jet holding Up):

| ROM | Passes/frame | Clamps | Before Stage 1b |
|---|---|---|---|
| Exact river, DMG proxy (`river_raid_slow`) | 1.00 / 0.97 / 0.95 | 0 / 2 / 2 | 0.37, 187 clamps (index 0) |
| Exact river, double speed (`river_raid`) | 1.00 / 1.00 / 1.00 | 0 / 0 / 0 | 0.76 (index 0) |
| 2600, DMG proxy | 0.99 / 0.99 / 0.99 | 0 / 2 / 1 | 0.57 (index 0) |
| 2600, double speed | 1.00 / 1.00 / 1.00 | 0 / 0 / 0 | 0.97 (index 0) |

## What changed in the engine (Stage 1b; details in `PLAN_2600.md`)

- **Pacing:** a finished pass publishes scroll and an OAM page (`publish_pass`, second page at `$D000`); the VBlank handler shows it; the next pass starts at once if a frame has gone by. `vsync()` used to turn a pass of 1.01 frames into 2. All VRAM and palette writes may land in active display, and all wait on STAT.
- **Assembly is allowed** (user). sm83: `oam_camera_core`, `ent_draw_asm`, `ent_emit_asm`, `ent_follow_asm`, `ent_scan_asm`, `river_scan_solid`, `river_mixed_col`, `river_copy_line_asm`, `river_pat_equal_asm`, `river_fill_core`. Each has a C reference; the host harness only runs the C.
- **Cuts:** the HUD returns when its inputs are unchanged; sprite rows above the view are not drawn; `oam_camera` walks live slots only.

## Next steps

Do **`PLAN_DMG.md` in order.** Task 1 is host-only and decides the rest: how many river tiles are really needed on screen. If it does not fit, ask the user before cutting art or falling back.

Parked: 2600 river work (Stage 1b steps 5-8, Stage 2), **smooth blocky edges** (`BUGS&FEATS.md`), remaining exact-river peaks (`river_find_dup`, tile uploads, `river_copy_template`).

**Per-step check:** `host_river.ps1` (exact), `host_river.ps1 -Blocky`, `host_river_fast.ps1 -Rows 20000 -JobCols 10`, `.\build.ps1` (NES), then `.\build_gbc.ps1`, `-River2600`, and `-Dmg` once it exists; bank 0 end in the `.map`. Profile: `.\build_gbc.ps1 -ProfExport [-DmgProxy] -Name <rom>`, `$env:GB_NOI='roms/<rom>.noi'; node tests/gb/profile_play.mjs fast|cruise 40` (`$env:GB_START_LEVEL=n` for another start; Gearboy launch line in `AGENTS.md`). **After touching assembly or its C twin:** `-OamCheck` and `-RiverCheck` ROMs, run with `profile_play.mjs`: all error counts must be 0. **Those only compare assembly with C, so a shared mistake passes:** also run `tests/gb/sprite_check.mjs` on the player ROM and look at its screenshot before handing a ROM over.

## Decisions not to reopen

- **Logic keeps NES coordinates.** Only drawing uses the camera.
- **Collision and drawing share one edge description** (`rg_row_left/right`).
- **Performance before authenticity** (user, 2026-11-02). Features may be dropped, but cutting art is the user's call.
- **The slow exact ROM's feel is acceptable** (user's playtest, 2026-11-02): that is the reference for DMG.
- **Fast flight stays at 1.5 px/frame**, **HUD art from `charset.bin`**, **sound values from live Atari captures**, **the user runs the playtests**.

## Where to look

| Topic | Where |
|---|---|
| The plan: tile budget, tasks, gates | `PLAN_DMG.md` |
| Performance findings, 2600 tasks and numbers | `PLAN_2600.md` |
| 2600 decision text, the mod | `docs/PROPOSAL_2600_RIVER_MODE.md`, `docs/MOD_2600_SPEC.md`, `assets/atari/river2600/` |
| Rules, toolchain, Gearboy MCP | `AGENTS.md` |
| River code | `src/river.c`, `tools/river_model.py` |
| Host checks | `tests/host_river.ps1` (`-Mode2600`, `-Blocky`), `tests/host_river_fast.ps1`, `tests/host/tiles_main.c` (clobber count) |
| Builds | `.\build_gbc.ps1 [-River2600] [-DmgProxy] [-ProfExport] [-OamCheck] [-RiverCheck] [-PassLog]`; `.\build.ps1` (NES) |
| Profiling | `tests/gb/profile_play.mjs` + `tools/prof_summary.py`; `tests/gb/trace_prof.mjs` + `tools/trace_hist.py` (phases, worst passes; sees inside banked calls) |
| Atari behaviour / RE tools | `docs/ATARI_RE_SPEC.md`, `info/LESSONS.md`, `tools/README.md` |
| GBC / NES history | `archive/gbc/`, `archive/nes/` |

## Known gaps

- The exact host build reports `clobbered 134` on start index 3 ("written 27 rows ago"). Probably off screen; `PLAN_DMG.md` task 1 settles it.
- All timings come from 10 s windows with an invulnerable jet holding Up. Not measured: long runs, heavy shooting, level 13+ fast helicopters at speed.
- The proxy is a GBC at single speed; nothing has run on a real or emulated DMG. The "GBC REQUIRED" screen has never run on a plain Game Boy.
- Bank 0: exact build ends `$3928` (~1.7 KB free), 2600 build `$3F40` (~190 bytes free). The check ROMs drop the "GBC REQUIRED" text to fit.
- `river_mixed_col` needs `river_lut_p1` right after `river_lut_p0` in ROM (order in `river_lut.h`); `-RiverCheck` shows it if that changes.
- `sprite_check.mjs` runs on the first start level only, checks position and tile range but not the exact tile or palette, and covers types 7..15 only.
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
