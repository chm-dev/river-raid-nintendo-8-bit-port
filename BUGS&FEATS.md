# BUGS & FEATS

Status: `[ ]` open · `[~]` fixed, waiting for the user's playtest · `[x]` confirmed in play by the user · `[-]` stale: not being pursued, kept for reference.
History: `archive/nes/BUGS&FEATS.md`, `archive/gbc/BUGS&FEATS.md`, `archive/dmg/BUGS&FEATS.md`.

# BUGS

- [~] **DMG bridge turned white, and the tank-under-bridge bug returned** (user, 2026-11-07).
  Both came from the first palette pass. The bridge's deck is sprite colour 2, which that pass made
  white; the bridge tiles are now remapped 2 -> 3 (`entities_init_graphics` and `entities_dmg_art`)
  so the deck is light grey again, and the scratch copy matches. The tank fix silently switched
  itself off because `dmg_spr_used` had grown from 124 to 125 (drawing the jet in sprite colour 2
  costs one deduped tile), and `dmg_tank_hide` needs hardware ids 124..127 free
  (`dmg_spr_used <= DMG_SCR_HW`, 124). The jet is back on colour 1 (white via `DMG_OBP0`);
  `dmg_check.mjs` now fails if the allocation goes over 124.

# FEATURES

- [~] **Two DMG palettes, SELECT in any state** (user, 2026-11-07; waiting for the playtest).
  `dmg_pal` (0 = palette A, the new one; 1 = the original DMG palette) is toggled by SELECT in
  every state — attract, play, pause, dead (`player_update` -> `dmg_palette_toggle`,
  `src/gbc/main.c`) — and applies immediately: BGP/OBP0/OBP1, the HUD tiles (`hud_dmg_palette`)
  and, for new rows, the river encoding.  Rows already on screen keep the old hills/road encoding
  until they scroll out (a few seconds).  The pause menu no longer uses SELECT for its cursor
  (Up/Down still do; the NES keeps SELECT).
  Palette A is the previous entry; palette B restores the original shades: BGP entry 3 white water
  and white HUD/gauge/road line (ink colour 3), entry 2 dark grey hills/road, hills not remapped,
  OBP black/light grey/dark grey (black player, light grey FUEL/balloon, dark grey helicopters).
  The bridge art stays remapped in both, so in B its deck is dark grey, not the old light grey.
  GBC unchanged (SELECT is a no-op there).
- [~] **Palette A: white bridge flash and white enemy planes** (user, 2026-11-07).
  `DMG_BGP_FLASH_A` entry 3 is white now, so a bridge hit floods the water/hills/road white.  The
  enemy jets (the vertical planes) are white on OBP0 colour 3 (`DMG_OBP0_A` = all white).  The FUEL
  depot moved to OBP1 (`tools/gen_entity_gfx.py`, `dmg_pal[FUEL] = 1`) to free that colour; in
  palette B its colours are unchanged (the B registers are the same), and B's enemy jets stay dark
  grey.
- [~] **Collisionless debug start moved from SELECT to A + B + START** (user, 2026-11-07).
  Universal, all platforms: START with A and B held sets `pl_nocol` (`player.c`). SELECT is the DMG
  palette switch (every state) and does nothing else in attract. `tests/nes_menu.mjs` now uses
  A+B+START; the NES ROM changes (see the handover for its hash).

- [~] **DMG palette 2: land light grey, water dark grey, white HUD** (user, 2026-11-07; waiting for
  the playtest). Same method as palette 1, one shade down: `DMG_BGP` = black outline, light grey
  land, white entry 2, dark grey water. A DMG has only four shades, and this layout gives water and
  hills the same shade, so `river_dmg_hills` maps the hills' raw value 2 to the water's 3; entry 2
  is then white and belongs to the HUD, the gauge's filled part and the road centre line
  (`river_road_paint` writes the line as value 2). Land, the strip below the road and the river all
  match the mockup; the bridge-flash `DMG_BGP_FLASH` darkens the shared water/hills/road entry one
  step to black.
  Sprites: `DMG_OBP0` colours 1 and 2 white, 3 black at first (player, pause letters, cursor and
  shot white; enemy jet dark; FUEL later moved to OBP1 and the enemy jet made white, see the
  palette-A fixes entry); `DMG_OBP1` 1 black, 2 white, 3 light grey (helicopter and ship bodies
  light grey, bridge deck light grey after its remap). No sprite uses the river's dark grey.
  GBC unchanged. Checked: `tests/host_river.ps1` (19,999 rows), `dmg_check.mjs` (dual), `gbc_check.mjs`
  (80 samples, no colour change), screenshots against the mockup and of a bridge.

- [x] **DMG palette 1: land white, water light grey** (user, 2026-11-07; confirmed, "you did great
  job"). Land took the lightest shade and the water the next one; the HUD, road line and jet stayed
  white. Superseded by palette 2; its two regressions are the BUGS entry above.
  `DMG_BGP` land white / water light grey, HUD ink and gauge filled white (colour 1), road line
  white. Sprites: colour 2 white so the rotors, ships, FUEL and balloon read against the water.
  GBC unchanged. Checked: `host_river.ps1`, `dmg_check.mjs`, `gbc_check.mjs`, screenshot against
  the mockup.
