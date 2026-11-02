# HANDOFF — read this first

Short, current, rewritten every session. Rules are in `AGENTS.md`; the plan is `PLAN_2600.md`; history is `PROGRESS.md` (grep it, do not read whole). The proposal is `docs/PROPOSAL_2600_RIVER_MODE.md`.

**Last updated:** 2026-11-02 · **Branch:** `river-2600` (from `main` at `95e0074`) · **Phase:** workspace prepared, no code changed yet. Next: Stage 1 task 1 (scope the mod)

## Where we are

- The GBC port is finished and merged on `main`: exact per-scanline river, NES and GBC from one tree. Its docs are in `archive/gbc/` (read `archive/gbc/HANDOFF.md` → "Where we are" for the architecture: banks, main loop, drawing, sprites, HUD).
- This branch tries a **2600-style river** (straight and angled edges, fixed tile atlas) mainly so a **DMG** build becomes possible. Stage 1 is a testbed on GBC; Stage 2 (DMG prototype) is the gate. Full text: the proposal.
- The Stage 1 gate numbers (X, Y in the proposal) are **not set yet**. The user sets them before any measuring.
- Facts already found: `xex/River Raid 2600.xex` is 10,754 bytes against 8,220 for the original, so more than the river may have changed.

## Next steps

Stage 1, in order (details and checklists in `PLAN_2600.md`):

1. **Scope the mod.** Byte-diff the two XEX files (`tools/dump_xex.py`), then disassemble the changed regions. Write the findings to `docs/MOD_2600_SPEC.md`.
2. **Capture the mod's river over time** with AltirraBridge (recipe: `info/LESSONS.md` → "Reverse Engineering Tools"). Record edge slopes, segment lengths, non-river changes.
3. **Baseline measurements** on the current exact-river ROM (proposal table, rows 1-8), saved in `PLAN_2600.md`. Needs the user's gate numbers first for the comparison, not for the baseline itself.
4. Model, C generator, atlas renderer, measure, playtest checklist (proposal Stage 1 steps 2-6).

## Decisions not to reopen

- **Logic keeps NES coordinates.** Only drawing uses the camera.
- **Collision and drawing share one edge description** in the 2600 mode.
- **GBC remains the testbed.** DMG work waits for the Stage 1 check and the user's approval.
- **Fast flight stays at 1.5 px/frame.**
- **HUD art comes from `charset.bin`.** Do not redraw it.
- **Sound values come from live Atari captures.** Not estimates.
- **The user runs the playtests.** Claude builds the ROM and hands over a checklist.
- **Rules that the proposal reopens** ("no river atlas", "DMG dropped", "no new assembly") are reopened only as far as the proposal says: atlas for the 2600 mode only, DMG only after Stage 1, no assembly in the prototype.

## Where to look

| Topic | Where |
|---|---|
| Proposal, stages, gates, measurement table | `docs/PROPOSAL_2600_RIVER_MODE.md` |
| Working plan, task checklists, baseline numbers | `PLAN_2600.md` |
| Rules, toolchain, Gearboy MCP | `AGENTS.md` |
| The mod | `xex/River Raid 2600.xex` (original: `xex/River Raid (1983)(Activision)(US)[a3].xex`) |
| River generator and cache (exact mode) | `tools/river_model.py`, `src/river.c`, `tests/host_river.ps1` |
| Atari behaviour | `docs/ATARI_RE_SPEC.md`, `docs/atari_disasm_*.txt` |
| Atari RE tools | `info/LESSONS.md`, `info/ALTIRRA_BRIDGE_NOTES.md`, `tools/README.md` |
| Gearboy pacing tools | `tests/gb/` (`profile_play.mjs`, `pass_log.mjs`, `run_emu.ps1`) |
| Bugs and feature ideas | `BUGS&FEATS.md` |
| GBC port history, baseline numbers | `archive/gbc/` |
| NES history | `archive/nes/` |

## Known gaps

- `river.c` emits harmless `always false` warnings (zero-width margin loops, `RIVER_LEFT_COL` is 0 on GBC).
- The "GBC REQUIRED" screen has never been run on a plain Game Boy. A DMG build would replace it.
- Player and entity group costs show only as `___sdcc_bcall_ehl` in the profiler; name them before measuring (proposal row 3).

## End of session (keep this cheap)

1. **Rewrite and trim this file.** Update the date, move finished items out, and correct the budgets. Delete what is no longer true instead of annotating it. Check the size first: `(Get-Content HANDOFF.md).Count` (target ~100, trim past ~150).
2. **Append one entry to `PROGRESS.md` without reading it.** The closing `'@` must stay at column 0:

```powershell
Add-Content -Path PROGRESS.md -Encoding utf8 -Value @'

### YYYY-MM-DD - Short title (branch `name`)

- What changed and why; ground truth used; build line; tests run and results.
'@
```

3. Do not edit `AGENTS.md` for status.
