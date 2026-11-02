# archive/gbc — the exact-river GBC port's documents

Archived on 2026-11-02 when the `river-2600` branch started. The GBC port itself is finished and merged on `main` (NES and GBC build from one tree). These files are reference only and are not required reading.

- `HANDOFF.md`: status at the end of the GBC port (milestone 2 passed, sound, HUD, pacing work). Its "Where we are" section is the best description of the GBC architecture (banks, main loop, drawing, sprites, HUD).
- `GBC_PORT_PLAN.md`: decisions, measurements (including the baseline numbers the 2600 proposal compares against), technical design, milestones, risks, Gearboy notes.
- `PROGRESS.md`: append-only history. Grep it, do not read it whole.
- `BUGS&FEATS.md`: the GBC bug and feature list. All items are confirmed except the sprite Y item and the high-score save, which were waiting for the user's playtest.

Paths inside these files refer to the layout before the move (`HANDOFF.md` and the others were at the repository root). Find history with `Select-String -Path archive\gbc\PROGRESS.md -Pattern 'keyword'`, then read only the matching `### date - title` entry. The full pre-archive tree is `main`.
