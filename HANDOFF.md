# HANDOFF — read this first

Short, current, rewritten every session. Rules are in `AGENTS.md`; **the plan is `PLAN_DMG.md`**; history is `PROGRESS.md` (grep it, do not read whole).

**Last updated:** 2026-11-03 · **Branch:** `dmg-exact` (from `river-2600`) · **Phase:** the dual ROM (GBC and DMG in one) is built and checked in both modes; **waiting for the user's playtest of `roms/river_raid.gb` in both modes.** Merging to `main` is the user's call, after that.

## Where we are

- **`.\build_gbc.ps1` → `roms/river_raid.gb`: one ROM, header `$80`.** On a GBC it is the finished GBC game (colour, double speed); on a DMG the accepted DMG game (grey, single speed). Chosen at boot from `_cpu` (`gb_cgb`, `src/platform.h`). Both modes use the DMG VRAM layout and the delay queue (226 cache slots); GBC attributes are written at job time, not queued (`PLAN_DMG.md` "Dual ROM").
- **Builds:** default = dual (`PLATFORM_DMG` + `PLATFORM_DUAL`). `-Dmg` = DMG-only ROM (hash `9EA404E4`). `-GbcOnly` = the old GBC-only ROM (`5598B71F`, reference). `-River2600` implies `-GbcOnly` (`09F504E0`). NES `C1D44CF0`. All unchanged except `-Dmg` (rebaselined for the `di`/`ei` fix in `river_vram_copy_asm`). Reference ROMs of the pre-dual code: `roms/ref/` (git-ignored).
- **GBC finished on `main`** (`archive/gbc/HANDOFF.md`). `main` does **not** have the Stage 1b speedups, the DMG work or the dual ROM. Not pushed, not merged.
- **The 2600 river is stale (user, 2026-11-02):** not pursued, but `RIVER_2600`, `RIVER_BLOCKY`, `RIVER_NOFEAT`, `RIVER_FAST`, `-River2600` and `PLAN_2600.md` stay and keep building.

**Fast flight, 10 s, start 0 / 2 / 3** (invulnerable jet holding Up): **dual, GBC 1.00 / 1.00 / 1.00** (0 clamps); **dual, DMG 1.00 / 0.98 / 0.95** (0 / 2 / 2). Before: `river_raid` 1.00 ×3, `river_raid_dmg` 0.99-1.00 / 0.98 / 0.96.

## Next steps

1. **The user plays `roms/river_raid.gb` on a GBC and on a DMG** (checklist below). On a PC emulator the DMG mode needs a DMG model (Gearboy: `ForceDMG` with `--portable`, see below).
2. Then: the user decides about merging `dmg-exact` to `main`. Optional: boot time (~1.5-1.9 s blank on a DMG), remaining peaks (`river_find_dup`, `river_copy_template`).
3. **After the merge, when asked:** NES pacing ideas from the GB work (`archive/nes/BUGS&FEATS.md`, top of FEATURES). The NES ROM stays byte-identical until then.

Stale (do not start unless asked): 2600 river work, **smooth blocky edges**.

**Playtest checklist (both modes, same ROM):**
- GBC: colours as before: land flip after a bridge, road, low fuel (gauge red, text yellow), bridge flash (red water, HUD not flickering) · fast flight feels as before · **no wrong tiles in the bottom rows** (the old GBC bug should be gone; the user to confirm) · pause, crash, restart, high score after power-off, a long game past level 13.
- DMG: as the accepted DMG ROM: shades, FUEL, balloon, low fuel (E and F blink), bridge flash (HUD flickers with the water: does it bother you?) · fast flight on start levels 1, 20, 50.
- Same save file for both (`river_raid.sav`).

**Per-step check:** `host_river.ps1` (exact), `-Blocky`, `host_river_fast.ps1 -Rows 20000 -JobCols 10`, `host_river_queue.ps1`, `.\build.ps1` (NES hash), then `.\build_gbc.ps1`, `-GbcOnly`, `-River2600`, `-Dmg`; compare hashes (above). Bank 0 end in the `.map` (`$36F3` now). **After assembly or its C twin:** `-OamCheck` and `-RiverCheck` ROMs, run in **both** machines with `profile_play.mjs`: all error counts 0. **Before a handover, both machines:** `sprite_check.mjs`, `gbc_check.mjs` (GBC; needs a `-ProfExport` ROM) and `dmg_check.mjs` (DMG), then look at screenshots (`collect_shots.mjs` + `montage.mjs`).

**Running a test (use this, not a hand-started Gearboy):** `.\tests\gb\run_emu.ps1 <script> <arg> [<arg2>] -Rom roms/NAME -Machine gbc|dmg` starts its own Gearboy on port 7790, confirms the machine from the boot registers (`mode_probe.mjs`) and stops only its own PID (other agents may run theirs; never stop `Gearboy` by name). `-Machine dmg` sets `ForceDMG = true` in `gearboy/config.ini`, read only with `--portable`; the script restores it. `get_media_info.is_cgb` is the header flag, not the mode.

**Profile:** `.\build_gbc.ps1 -ProfExport -Name rr_x`, then `$env:GB_START_LEVEL=n; .\tests\gb\run_emu.ps1 tests/gb/profile_play.mjs fast 24 -Rom roms/rr_x -Machine gbc`.

**Scripting traps:** do not pass build switches as `@array` to `build_gbc.ps1` (they bind positionally and nothing builds); its "Built" line is `Write-Host`, so capture with `*>&1` and check the file time. A build overflow check fails but later commands in the line still run: read the `.map`. Source files are CRLF: edit with the Edit tool, or normalise line endings in scripts.

## Decisions not to reopen

- **Logic keeps NES coordinates.** Only drawing uses the camera. This is why the delay queue exists.
- **Collision and drawing share one edge description** (`rg_row_left/right`).
- **Performance before authenticity** (user, 2026-11-02). Cutting art is the user's call.
- **Delay queue and shared tiles for the DMG; the DMG ROM's feel is accepted** (user, 2026-11-02). The GBC uses the same queue and layout in the dual ROM; its sprites and HUD stay in bank 1.
- **Fast flight stays at 1.5 px/frame**, **HUD art from `charset.bin`**, **sound values from live Atari captures**, **the user runs the playtests**.

## Where to look

| Topic | Where |
|---|---|
| The plan, tile budget, dual ROM decisions and results | `PLAN_DMG.md` |
| Machine flags | `src/platform.h` (`PLATFORM_DMG`, `PLATFORM_DUAL`, `GB_HAS_CGB`, `gb_cgb`) |
| Boot, OAM, camera | `src/gbc/main.c` |
| HUD (runtime tile base, both machines) | `src/gbc/hud_gbc.c` |
| River code, queue, attributes | `src/river.c`, `tools/river_model.py` |
| DMG layout and shades | `src/gbc/config_gbc.h`, `dmg_sprite_data` in `src/gbc/main.c` |
| Entity palette tables | `tools/gen_entity_gfx.py` → `src/entity_gfx.c` (`ent_gfx_pal`, `ent_gfx_pal_dmg`) |
| Host checks | `tests/host_river.ps1`, `host_river_fast.ps1`, `host_tile_budget.ps1`, `host_river_queue.ps1` |
| Emulator checks | `tests/gb/`: `run_emu.ps1`, `mode_probe`, `sprite_check`, `gbc_check` (+`gbc_colours.json`), `dmg_check`, `collect_shots`, `montage`, `profile_play` |
| Rules, toolchain, Gearboy | `AGENTS.md` |
| 2600 work | `PLAN_2600.md`, `docs/PROPOSAL_2600_RIVER_MODE.md` |
| GBC / NES history | `archive/gbc/`, `archive/nes/` |

## Known gaps

- **Nothing has run on real hardware.** Gearboy does not model the mode-3 VRAM write lockout (a build with the wait removed gives 0 readback errors), so the timing of `river_vram_copy_asm` (now with `di`/`ei`) is reasoning, not measurement. Shades on a real DMG LCD and colours on a real GBC are unchecked.
- **A fully white screenshot in DMG mode, once or twice after a long collector run, in state 3, not reproduced** (`BUGS&FEATS.md`). Short sequences are identical on the dual and the old `-Dmg` ROM.
- `sprite_check.mjs` has an occasional false miss in DMG mode (also on the old `-Dmg` ROM). `gbc_check.mjs` and `dmg_check.mjs` test plain rows and the first start levels; the exact picture is proved on the host.
- Not looked at in a screenshot: level 13+ fast helicopters and their missile; the GBC low-fuel HUD colours by pixel.
- `-River2600` is GBC-only and does not use the queue (its bank 0 had ~190 bytes free); `-Dmg -River2600` has never run. `river_mixed_col` needs `river_lut_p1` right after `river_lut_p0`; `RIVER_GRID=4` fails `host_river_fast.ps1`.
- The `need` line of `tile_budget_main.c` is meaningless in a `-DPLATFORM_DMG` build; its clobber count is valid.

## End of session (keep this cheap)

1. **Rewrite and trim this file.** Update the date, move finished items out. Check the size: `(Get-Content HANDOFF.md).Count` (target ~100, trim past ~150).
2. **Append one entry to `PROGRESS.md` without reading it** (`Add-Content -Path PROGRESS.md -Encoding utf8 -Value @'...'@`, the closing `'@` alone at column 0), as a `### YYYY-MM-DD - Short title (branch name)` heading and bullets: what changed and why, ground truth, build line, tests and results.
3. Do not edit `AGENTS.md` for status.
