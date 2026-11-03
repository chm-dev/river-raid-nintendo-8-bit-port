# AGENTS.md — Game Boy port workspace rules

Stable rules only. Status lives in `HANDOFF.md`.

## Project status
- **The project is done** (user, 2026-11-03): the NES, GBC and DMG ports are finished. Only occasional bug fixes and small new features may come. There is no active plan and no root `HANDOFF.md`.
- **The DMG work is merged** into `main` (`dmg-exact` has no commits ahead). One ROM, `roms/river_raid.gb`, runs as GBC or DMG, chosen at boot (`gb_cgb`, `src/platform.h`).
- **DMG history is in `archive/dmg/`:** `HANDOFF.md` (last state, build hashes, per-step checks, test recipes, scripting traps, decisions not to reopen), `PLAN_DMG.md`, `PLAN_2600.md` (parked 2600 river mode, Stage 1b performance findings), `PROGRESS.md`, `BUGS&FEATS.md`. **Read `archive/dmg/HANDOFF.md` before touching the GB code**: its "Per-step check", "Running a test" and "Decisions not to reopen" sections still apply. The 2600 decision text is in `docs/PROPOSAL_2600_RIVER_MODE.md`.
- **GBC history is in `archive/gbc/`** (old HANDOFF, PROGRESS, GBC_PORT_PLAN, BUGS&FEATS: architecture, baseline measurements, build and emulator notes). Open it only when needed.
- **NES history is in `archive/nes/`** (old HANDOFF, PROGRESS, PORTING_PLAN, BUGS&FEATS, Mesen notes, NES AGENTS and LESSONS). Open it only when a specific NES reference is needed.
- **Archived `PROGRESS.md` files are append-only history.** Do not read them whole. Grep for a keyword and read only the matching `### date - title` entry.
- **New work:** if the user reports a bug or asks for a feature, track it in a new root `BUGS&FEATS.md` (create it when first needed). Do not recreate `HANDOFF.md` or plans unless the user asks.

## Project overview
Port of the Atari 8-bit **River Raid** (`xex/River Raid (1983)(Activision)(US)[a3].xex`) to the **Game Boy Color** in C, using GBDK-2020 v4.5.0 (`./gbdk`).
- **GBC and DMG are both finished** (dual ROM, see Project status). A fix must keep both modes and the NES build working; DMG code stays behind its own flag.
- **Hand-written sm83 is allowed where it pays** (user, 2026-11-02); keep the C version as the reference for a byte-for-byte check (`build_gbc.ps1 -OamCheck`, `-RiverCheck`).
- **Atari ground truth:** `docs/ATARI_RE_SPEC.md`, `docs/atari_disasm_*.txt`, `assets/atari/extracted/`. The shared river generator is `tools/river_model.py` and `src/river.c`.
- **Logic coordinates:** game logic stays in NES-era coordinates (X = Atari HPOS, Y = NES scanlines 30..239). Only drawing goes through a camera.
- **HUD art:** do not redraw. `assets/atari/extracted/charset.bin` holds the original art; see `assets/atari/extracted/README.md` → "HUD glyph inventory".
- **River:** exact per-scanline bitmap. No fixed atlas: upload the exact 8x8 blocks, which depend on vertical phase.

## Toolchain & build (`./gbdk`)
- **Compile:** `gbdk/bin/lcc.exe -msm83:gb -Isrc` (SDCC `sm83` backend). `-Wf-bo1` for `entities` and `entity_gfx`.
- **Link:** `-Wl-j -Wl-m -Wl-yt0x19 -Wl-yo4` (MBC5, 4 banks). Always pass `-Wl-j` so `.noi` symbols are generated. `-DPROF_EXPORT_STATICS` exposes statics to the profiler.
- **Two builds, one tree.** `.\build_gbc.ps1` builds the GBC ROM (`roms/river_raid.gb`); `.\build.ps1` builds the NES ROM (`roms/river_raid.nes`). Both write `roms/river_raid.noi` and `.map`, so rebuild the one you are testing last. Shared files pick the SDK header through `src/platform.h` and branch on `__SDCC_sm83`; never include `<gb/gb.h>` or `<nes/nes.h>` directly in shared files. NES RAM has ~3 bytes free: GBC-only globals and `#define` hacks must sit inside `#ifdef __SDCC_sm83`. After touching a shared file, build both.
- **Asset conversion:** `./gbdk/bin/png2asset`, `./gbdk/bin/gbcompress`, and `tools/gen_entity_gfx.py` (it needs GBC palette output).

### GBC / SDCC rules
1. **Flat static state.** Use globals and fixed arrays, not deep call stacks or pointers to stack locals.
2. **ISR handlers stay short.** Never call a `BANKED` function from an ISR.
3. **SDCC local aliasing.** Do not keep a block-local value live across a call in a complex function. SDCC can alias it with the callee's state and the saved value silently changes. Use a file-scope `static` for values like a saved row offset. (This is the river-renderer `ysave` bug; the code comments cite this rule number.)
4. **Inline `__asm` goes in its own function.** SDCC keeps values in registers across an asm block and corrupts the surrounding C. Raw-row tests can still pass, so verify with real tile pixels (`tests/compare_tile_rows.mjs`, `tests/host_river.ps1`).
5. **Time-based river.** River code runs on `game_ticks`. Benchmarks must advance `sys_time` themselves: `entities_update()` reads it and it only advances with VBlank.
6. **No river atlas for the exact river.** Trees are template fragments; upload exact blocks. The 2600 river mode (`PLAN_2600.md`) is the only place a fixed edge atlas is allowed.
7. **Banking.** Keep banked modules in a real bank. A single-bank link with `BANKED` functions jumped into unmapped ROM (`RST_38`).
8. **Check the map after every link.** Each bank's code must stay inside its window (`$0000-$3FFF` bank 0, `$4000-$7FFF` for banked code). The linker may not fail on overflow, so treat `.map` as the source of truth until milestone 1 confirms the behaviour.
9. **Profile peaks, not averages** (`LESSONS.md` item 14). Prove an optimisation by diffing output, not only with a test (`LESSONS.md` item 18).

## Emulator and tests: Gearboy (`./gearboy`)
Gearboy is the GB/GBC emulator with an MCP server. The folder is gitignored and local to this machine.
- **Launch (headless):** `gearboy/Gearboy.exe --headless --mcp-http --mcp-http-port 7777 rom.gb rom.noi`. MCP endpoint: `http://127.0.0.1:7777/mcp`.
- **Starts paused:** call `debug_continue`.
- **Protocol:** requests after `initialize` need the header `MCP-Protocol-Version: 2025-11-25`.
- **Memory:** `read_memory` area 6 is WRAM, with offsets from `$C000`.
- **Profiler:** reports units of 70,224 per frame at any CPU speed. M-cycles = units / 4 in single speed, units / 2 in double speed (the ROM runs double). Recipe: `tests/gb/profile_play.mjs`.
- **Speed:** fast-forward did not speed up headless runs, so keep benchmarks short.
- **Shutdown:** stop the emulator process when finished.
- **Client:** `tests/gb/gearboy_client.mjs`. Pacing tools: `tests/gb/profile_play.mjs` (profiler) and `tests/gb/pass_log.mjs` (per-pass scanline log); `tests/gb/run_emu.ps1` starts a fresh emulator for one of them (both need `.\build_gbc.ps1 -ProfExport`; `pass_log` also `-PassLog`). `tests/gb/trace_prof.mjs` + `tools/trace_hist.py`: cycle-exact trace, per-phase and per-function cost (sees inside banked calls, which the profiler charges to `.call_hl`). `tests/gb/sprite_check.mjs`: hardware OAM against the entity positions; run it on the player ROM before a handover (it is the one automated check that does not compare assembly with its own C).
- `tests/host_river.ps1` checks the C generator against `tools/river_model.py` on the host (needs `zig cc`). Run it after touching `river.c` generator code.

## Atari reverse engineering (dynamic analysis)
- AltirraBridge: `F:\Emu\Atari800\Altirra\AlitrraBridge\` (note the spelling). Rules and the river-row capture recipe are in `info/LESSONS.md` → "Reverse Engineering Tools". API reference: `info/ALTIRRA_BRIDGE_NOTES.md`.
- Sound values come from live Atari captures, not estimates. Sample over time and do not trust one static read (see memory on the attract freeze).
- Tool list: `tools/README.md`.

## Workflow
- **The user runs the playtests.** Build the ROM, then hand over a checklist. Do not drive the emulator to confirm gameplay fixes.
- **Bugs and features:** root `BUGS&FEATS.md` (see Project status). Mark a fix `[~]` until the user confirms it in play, then `[x]`.
- **Branches:** work on `main`. Branch only for risky or throwaway experiments, never a scratch copy of the tree. `river-2600` (2600 river mode, Stage 1b performance work) is kept as a fallback. Merging any branch to `main` is the user's call.
- **No commits (user, 2026-11-03).** Do not run `git commit`, `git add`, `git push`, or any other command that changes git history or the index. Leave changes in the working tree; the user commits and pushes themselves. This replaces the older "commit locally" rule.
- **Session end:** summarize what changed and which ROMs or hashes were rebuilt. Do not edit `AGENTS.md` for status.
