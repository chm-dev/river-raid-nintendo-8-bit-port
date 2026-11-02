# BUGS & FEATS — river modes and DMG

Status: `[ ]` open · `[~]` fixed, waiting for the user's playtest · `[x]` confirmed in play by the user.
Older lists: `archive/gbc/BUGS&FEATS.md`, `archive/nes/BUGS&FEATS.md`.

# BUGS
- [ ] **Shipped GBC and NES ROMs: rare wrong tiles in the bottom ~10 px of the view** (found 2026-11-02 by `tests/host_tile_budget.ps1`, not seen in play). The 250-slot cache is reused while a row is still on screen: 67-151 cells per 60,000 tile rows, age 27-28 rows; 266 slots would be needed. Not fixed: the user decides whether it matters. The DMG ROM does not have it.

# FEATURES
- [~] **Exact river on DMG** (`PLAN_DMG.md`). Built: `roms/river_raid_dmg.gb`, delay queue and shared tiles, no art cut. Waits for the user's playtest.
- [ ] **2600 river mode** (parked; `PLAN_2600.md`, proposal `docs/PROPOSAL_2600_RIVER_MODE.md`).
- [ ] **Carried over from the GBC list, unconfirmed:** sprite Y position, and the high-score save to SRAM (`src/gbc/save_gbc.c`). Both were awaiting the user's playtest when the GBC docs were archived.
- [ ] **Smooth blocky edges (idea, user, 2026-11-02; not started).** The 8 px grid river looks bad. Keep the generator and the collision edges as they are, and draw the 8 px jump of each band as a 1 px/row diagonal over the band's 8 rows (`x0 + k*J/8`), so a ramp becomes a straight 45° line. Atlas key stays (side, edge position, jump): est. 30-60 tiles, no extra CPU. **Catches:** (1) drawing differs from collision by up to 7 px, so the drawn river must never be wider than the collision river (a narrowing river ramps before the jump, a widening one after it, which needs the previous band's jump in the key); (2) island, replayed and narrow bands use the exact renderer and stay blocky unless it interpolates too (islands: 4 boundaries); (3) `host_river_fast.ps1` needs a new reference (replay with quantisation off); (4) bank 0 has ~350 bytes free. Do after the performance work in `PLAN_2600.md` "Stage 1b".

- [x] Entity sprites invisible (only the jet shows) in the Stage 1b ROMs: wrong OAM byte order in `ent_emit_row`/`ent_emit_asm`. Fixed 2026-11-02; the user's later playtest of the slow exact ROM found it acceptable.
