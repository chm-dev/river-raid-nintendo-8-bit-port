# river2600 — captures of xex/River Raid 2600.xex

- `mod_cart_a000_bfff.bin`: the mod's cart image after unpacking (Altirra, 300 frames after boot).
- `mod_trace_400.jsonl`, `orig_trace_400.jsonl`: 400 river rows from the start of game 1 for the mod and the original (`tools/atari_river_trace.py`, break at `$AB80`; fields n, step, row_addr, zp, obj, row). Mode-E pixels, 4 per byte: 1 = land, 2 = shore, 3 = water.
- Validate the model against the mod: `$env:RIVER_CART='assets/atari/river2600/mod_cart_a000_bfff.bin'; python tools/river_validate.py assets/atari/river2600/mod_trace_400.jsonl`. Only the feature select differs (6 of 397 transitions); see `docs/MOD_2600_SPEC.md`.
