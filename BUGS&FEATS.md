# BUGS & FEATS — river modes and DMG

Status: `[ ]` open · `[~]` fixed, waiting for the user's playtest · `[x]` confirmed in play by the user · `[-]` stale: not being pursued, kept for reference.
Older lists: `archive/gbc/BUGS&FEATS.md`, `archive/nes/BUGS&FEATS.md`.

# BUGS
- [ ] **GBC exact ROM: rare wrong tiles in the bottom ~10 px of the view** (found 2026-11-02 by `tests/host_tile_budget.ps1` on the host, not seen in play). The 250-slot cache is reused while a row is still on screen: 67-151 cells per 60,000 tile rows, age 27-28 rows; 266 slots would be needed. The NES ROM has the same row lifetime by geometry, but uploads through its VBlank buffer and shows a different view: not measured. Not fixed: the user decides whether it matters. The DMG ROM does not have it.

# FEATURES
- [x] **Exact river on DMG** (`PLAN_DMG.md`). `roms/river_raid_dmg.gb`, delay queue and shared tiles, no art cut. The user's playtest, 2026-11-02: "absolutely flawless", every checklist item passes. Not merged; that is the user's call.
- [x] **DMG colours, user's request after that playtest:** FUEL in dark and light grey; balloon light grey with a dark grey stripe and basket. Enemy jets stay dark. Confirmed by the user, 2026-11-02.
- [x] **Sprite Y position** (carried over from the GBC list). Confirmed by the user, 2026-11-02.
- [x] **High-score save to SRAM** (`src/gbc/save_gbc.c`, carried over from the GBC list). Confirmed by the user, 2026-11-02.
- [-] **2600 river mode** (stale, user, 2026-11-02; `PLAN_2600.md`, proposal `docs/PROPOSAL_2600_RIVER_MODE.md`).
- [-] **Smooth blocky edges (stale, user, 2026-11-02; idea, never started).** The 8 px grid river looks bad. Keep the generator and the collision edges as they are, and draw the 8 px jump of each band as a 1 px/row diagonal over the band's 8 rows (`x0 + k*J/8`), so a ramp becomes a straight 45° line. Atlas key stays (side, edge position, jump): est. 30-60 tiles, no extra CPU. **Catches:** (1) drawing differs from collision by up to 7 px, so the drawn river must never be wider than the collision river (a narrowing river ramps before the jump, a widening one after it, which needs the previous band's jump in the key); (2) island, replayed and narrow bands use the exact renderer and stay blocky unless it interpolates too (islands: 4 boundaries); (3) `host_river_fast.ps1` needs a new reference (replay with quantisation off); (4) bank 0 has ~350 bytes free. Do after the performance work in `PLAN_2600.md` "Stage 1b".

- [x] Entity sprites invisible (only the jet shows) in the Stage 1b ROMs: wrong OAM byte order in `ent_emit_row`/`ent_emit_asm`. Fixed 2026-11-02; the user's later playtest of the slow exact ROM found it acceptable.
