# 2600 river mod — scope (Stage 1 task 1, part 1)

Method: the XEX is packed, so a static byte diff is useless (see below). Booted `xex/River Raid 2600.xex` in AltirraBridge, ran 300 frames, dumped RAM, diffed `$A000-$BFFF` against the original cart. Image saved: `assets/atari/river2600/mod_cart_a000_bfff.bin`.

## Container
- Original XEX: one segment `$9FF0-$BFFF` (the cart image) + RUNAD.
- Mod XEX (10,754 bytes): a 66-byte stub at `$0600`, a small helper at `$06A0` (disables BASIC/OS ROM via PORTB), and a **packed payload at `$2000-$4976`** (10.6 KB, only 110 zero bytes, not readable code). The stub runs `$2000`, then ends with `JSR $9FF6`, the original cart entry. So the payload unpacks/relocates the **original cart into `$9FF0-$BFFF`, patched**. The extra 2.5 KB is the unpacker plus its data.
- "v1.2 MDA" appears in the XEX header at file offset 2 (a loader tag; meaning not yet identified).

## Result: it is a patched original, not a rewrite
After unpacking, 961 of 8192 cart bytes differ, in 38 runs. The code layout is intact, so `docs/atari_disasm_*.txt` still maps.

| Range | Bytes | Looks like |
|---|---|---|
| A06B, A2A9, A6FC, A89F | 1 each | `$EA` NOP patches (disabled instructions) |
| A2B5, A616, A6A1, A976, ADAB | 1 each | Immediate operand changes (constants) |
| B42B, B48D-B493, B4E8, B4F0, B6A9, B6BB, B735 | 1-7 | Constant / table tweaks (unidentified) |
| B798-B7F3 (92), B838-B84A, B865-B895, B89F-B8D7 | ~200 | Tables, probably text/score/screen layout |
| B965-BA1D (~140) | | Character/graphic data (glyph bytes) |
| BA90-BAAD (30) | | Table zeroed out |
| **BC06-BDB8 (435), BDC7-BED7 (273), BEDE-BF04** | **~730** | **Large data tables: probably the river/playfield shape data, the 2600-style edges.** Most of the mod |
| BFA0-BFE4 | ~50 | Charset/glyph data |

## Open items
- Disassemble the code-side patches (A06B, A2A9, A2B5, A616, A6A1, A6FC, A89F, A976, ADAB, B42B..) and name each.
- Identify the `BC06-BF04` tables with `docs/ATARI_RE_SPEC.md` / the original disasm.
- Confirm that nothing in the dump was changed by runtime writes (all diffs are in cart RAM, which the game should not write, but check by dumping at two different frame counts).
- Non-river changes (glyphs, text, `$EA` patches) look small; the river looks like a data-table swap plus a few constants. Risk 1 (pollution) looks low. Confirm after the disassembly.
