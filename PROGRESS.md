# PROGRESS — 2600 river mode

Append-only. Do not read whole; grep for a keyword and read the matching `### date - title` entry. Earlier history: `archive/gbc/PROGRESS.md`, `archive/nes/PROGRESS.md`.

### 2026-11-02 - Workspace prepared for the 2600 river mode (branch `river-2600`)

- Branched from `main` (`95e0074`). Moved `HANDOFF.md`, `GBC_PORT_PLAN.md`, `PROGRESS.md`, `BUGS&FEATS.md` to `archive/gbc/` with `git mv` (history kept) and added `archive/gbc/README.md`.
- New `HANDOFF.md`, `PLAN_2600.md` (Stage 1 checklist, gates, baseline table), `PROGRESS.md`, `BUGS&FEATS.md`. `AGENTS.md` updated to point at them. Proposal's Decision section records the start of Stage 1.
- Found: the mod XEX is 10,754 bytes against 8,220 for the original.
- No code changed, no build run.
