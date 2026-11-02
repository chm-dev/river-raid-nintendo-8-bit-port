#!/usr/bin/env python3
"""Name the profiler's SUB_<bank>_<addr> entries (banked or unexported functions).

    python tools/resolve_sub.py 01_576D 00_0543
    python tools/resolve_sub.py --noi roms/river_raid_2600.noi 01_576D

Bank >= 1: looks up roms/river_raid.noi (or --noi FILE; the NES build overwrites it, rebuild the GBC ROM last) (symbols there are bank<<16 | addr) and prints the
nearest symbol at or below.  Bank 0 statics are not in the .noi: use the per-file listing
build/gbc/<file>.sym (offsets relative to the module, which the .map places; main.c is linked
first at $0200, so _oam_camera = $0200 + its .sym offset).  Build with -ProfExport first.
"""
import re
import sys
from pathlib import Path

args = sys.argv[1:]
noi = Path(__file__).resolve().parent.parent / "roms" / "river_raid.noi"
if args[:1] == ["--noi"]:
    noi, args = Path(args[1]), args[2:]
syms = []
for line in noi.read_text().splitlines():
    m = re.match(r"DEF (\S+) 0x([0-9A-Fa-f]+)", line)
    if m and not m.group(1).startswith(("b_", "l_", "s_")):
        syms.append((int(m.group(2), 16), m.group(1)))
syms.sort()
for q in args:
    bank, addr = int(q[:2]), int(q[3:], 16)
    full = (bank << 16) | addr if bank else addr
    near = [s for s in syms if s[0] <= full and (s[0] >> 16) == bank]
    print(q, f"{near[-1][1]}+{full - near[-1][0]}" if near else "?")

