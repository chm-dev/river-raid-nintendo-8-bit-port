# BUGS & FEATS — river modes and DMG

Status: `[ ]` open · `[~]` fixed, waiting for the user's playtest · `[x]` confirmed in play by the user · `[-]` stale: not being pursued, kept for reference.
Older lists: `archive/gbc/BUGS&FEATS.md`, `archive/nes/BUGS&FEATS.md`.

# BUGS
- [~] On nes shots sometimes do not destroy enemy and pass through it (fix: NES shot now steps and tests once per elapsed frame like GBC, so multi-frame passes no longer jump over the 6 px hit box)
- [~] DMG road color palette modification (fixed: on a DMG the road's grey is dark grey and its centre line white; `river_road_paint`, `!gb_cgb` only; GBC unchanged)
- [~] Tank clips behind the bridge on level transition - only on DMG (fixed: a DMG ranks overlapping sprites by X, so the bridge tile left of the tank painted over it; `dmg_tank_hide` in `src/gbc/main.c` now draws that bridge tile from a scratch sprite tile (hardware ids 124..127) with the tank's opaque pixels cut out, no hole in the bridge; DMG only, GBC unchanged; waits for the user's playtest)


# FEATURES
- [~] **Tank shell destroys other objects, as in the Atari** (user wants it; `ent_shell_kills` in `src/entities.c`, `$A4C1-$A4D9`: the swelling fireball turns any helicopter, balloon, ship, bridge or fuel depot it touches into an explosion with the kill sound, no score; the enemy jet is spared)
- [x] **One ROM for both GBC and DMG** (user, 2026-11-02), `.\build_gbc.ps1` -> `roms/river_raid.gb`, header `$80`. Waiting for the user's playtest in both modes (checklist in `HANDOFF.md`). Decisions and numbers: `PLAN_DMG.md` "Dual ROM".
- [x] **Exact river on DMG** (`PLAN_DMG.md`). `roms/river_raid_dmg.gb`, delay queue and shared tiles, no art cut. The user's playtest, 2026-11-02: "absolutely flawless", every checklist item passes. Not merged; that is the user's call.
- [x] **DMG colours, user's request after that playtest:** FUEL in dark and light grey; balloon light grey with a dark grey stripe and basket. Enemy jets stay dark. Confirmed by the user, 2026-11-02.
- [x] **Sprite Y position** (carried over from the GBC list). Confirmed by the user, 2026-11-02.
- [x] **High-score save to SRAM** (`src/gbc/save_gbc.c`, carried over from the GBC list). Confirmed by the user, 2026-11-02.
- [x] Entity sprites invisible (only the jet shows) in the Stage 1b ROMs: wrong OAM byte order in `ent_emit_row`/`ent_emit_asm`. Fixed 2026-11-02; the user's later playtest of the slow exact ROM found it acceptable.
- [ ] Splashscreen ?

- [-] **2600 river mode** (stale, user, 2026-11-02; `PLAN_2600.md`, proposal `docs/PROPOSAL_2600_RIVER_MODE.md`).
- [-] **Smooth blocky edges (stale, user, 2026-11-02; idea, never started).** The 8 px grid river looks bad. Keep the generator and the collision edges as they are, and draw the 8 px jump of each band as a 1 px/row diagonal over the band's 8 rows (`x0 + k*J/8`), so a ramp becomes a straight 45° line. Atlas key stays (side, edge position, jump): est. 30-60 tiles, no extra CPU. **Catches:** (1) drawing differs from collision by up to 7 px, so the drawn river must never be wider than the collision river (a narrowing river ramps before the jump, a widening one after it, which needs the previous band's jump in the key); (2) island, replayed and narrow bands use the exact renderer and stay blocky unless it interpolates too (islands: 4 boundaries); (3) `host_river_fast.ps1` needs a new reference (replay with quantisation off); (4) bank 0 has ~350 bytes free. Do after the performance work in `PLAN_2600.md` "Stage 1b".