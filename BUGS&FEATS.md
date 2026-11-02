# BUGS & FEATS — 2600 river mode

Status: `[ ]` open · `[~]` fixed, waiting for the user's playtest · `[x]` confirmed in play by the user.
Older lists: `archive/gbc/BUGS&FEATS.md`, `archive/nes/BUGS&FEATS.md`.

# BUGS
(none yet)

# FEATURES
- [ ] **2600 river mode** (proposal `docs/PROPOSAL_2600_RIVER_MODE.md`; tasks in `PLAN_2600.md`).
- [ ] **Carried over from the GBC list, unconfirmed:** sprite Y position, and the high-score save to SRAM (`src/gbc/save_gbc.c`). Both were awaiting the user's playtest when the GBC docs were archived.
- [ ] **Smooth blocky edges (idea, user, 2026-11-02; not started).** The 8 px grid river looks bad. Keep the generator and the collision edges as they are, and draw the 8 px jump of each band as a 1 px/row diagonal over the band's 8 rows (`x0 + k*J/8`), so a ramp becomes a straight 45° line. Atlas key stays (side, edge position, jump): est. 30-60 tiles, no extra CPU. **Catches:** (1) drawing differs from collision by up to 7 px, so the drawn river must never be wider than the collision river (a narrowing river ramps before the jump, a widening one after it, which needs the previous band's jump in the key); (2) island, replayed and narrow bands use the exact renderer and stay blocky unless it interpolates too (islands: 4 boundaries); (3) `host_river_fast.ps1` needs a new reference (replay with quantisation off); (4) bank 0 has ~350 bytes free. Do after the performance work in `PLAN_2600.md` "Stage 1b".

- [~] Entity sprites invisible (only the jet shows) in the Stage 1b ROMs: wrong OAM byte order in `ent_emit_row`/`ent_emit_asm`. Fixed 2026-11-02, waiting for the user's playtest.

- [~] Entity sprites invisible (only the jet shows) in the Stage 1b ROMs: wrong OAM byte order in `ent_emit_row`/`ent_emit_asm`. Fixed 2026-11-02, waiting for the user's playtest.
