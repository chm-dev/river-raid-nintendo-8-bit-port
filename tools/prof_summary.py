#!/usr/bin/env python3
"""Rank a profile_play.mjs JSON dump by exclusive cost per pass, with banked SUB_ entries named.

    $env:GB_PROF_JSON='prof.json'; node tests/gb/profile_play.mjs fast 300
    python tools/prof_summary.py prof.json roms/<rom>.noi [rows]

Units are the profiler's (70,224 per frame).  "excl/pass" is the function's own time, "incl/call"
includes callees.  Bank 0 statics stay SUB_00_xxxx (not in the .noi): see tools/resolve_sub.py.
"""
import json
import re
import sys
from pathlib import Path

d = json.loads(Path(sys.argv[1]).read_text())
rows = int(sys.argv[3]) if len(sys.argv) > 3 else 40
syms = []
for line in Path(sys.argv[2]).read_text().splitlines():
    m = re.match(r"DEF (\S+) 0x([0-9A-Fa-f]+)", line)
    if m and not m.group(1).startswith(("b_", "l_", "s_")):
        syms.append((int(m.group(2), 16), m.group(1)))
syms.sort()


def name(f):
    m = re.match(r"SUB_(\d\d)_([0-9A-F]{4})$", f["name"])
    if not m or m.group(1) == "00":
        return f["name"]
    full = (int(m.group(1)) << 16) | int(m.group(2), 16)
    near = [s for s in syms if s[0] <= full and (s[0] >> 16) == int(m.group(1))]
    return f"{near[-1][1]}+{full - near[-1][0]}" if near else f["name"]


fs = d["functions"]
passes = next((f["calls"] for f in fs if f["name"] == "_river_update"), 1)
total = sum(f["exclusive_cycles"] for f in fs)
idle = sum(f["exclusive_cycles"] for f in fs if f["name"] in ("[HALT]",))
print(f"frames {d['frames']} passes {passes} ({passes / d['frames']:.2f}/frame); "
      f"{total / passes:.0f} units/pass, idle(HALT) {idle / passes:.0f}, work {(total - idle) / passes:.0f}")
print(f"{'function':34} {'calls':>6} {'excl/pass':>9} {'excl%':>6} {'incl/call':>9} {'max':>7}")
for f in sorted(fs, key=lambda f: -f["exclusive_cycles"])[:rows]:
    print(f"{name(f):34} {f.get('calls', 0):6} {f['exclusive_cycles'] / passes:9.0f} "
          f"{100 * f['exclusive_cycles'] / total:6.1f} {f.get('average_cycles', 0):9} {f.get('max_cycles', 0):7}")
