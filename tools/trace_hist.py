#!/usr/bin/env python3
"""Exclusive cost per function and per main-loop phase, from a tests/gb/trace_prof.mjs disk trace.

    python tools/trace_hist.py TRACE.log roms/X.noi [rows]

Disk traces carry no timing, so each instruction's cost comes from an SM83 M-cycle table (x4 =
profiler units at single speed; a taken branch is seen from the next PC; HALT counts as 0).
Symbols: the .noi plus the statics in build/gbc/*.sym (each area's base comes from a global that
is in both), so the last build must be the one traced.  Units are the trace's (70,224 per frame).
Each pass starts at _sound_update (the first call of the pass; vsync() may be skipped); a frame is an
entry to $0040.  The phase of an instruction is the last top-level function
entered from main; interrupt handlers (from the entry at $40/$48 until RETI) are their own phase.
"""
import bisect
import collections
import re
import sys
from pathlib import Path

PHASES = ["_sound_update", "_hud_update", "_river_update", "_entities_follow", "_player_update",
          "_entities_update", "_camera_follow", "_oam_camera", "_apply_camera"]

trace, noi = sys.argv[1], sys.argv[2]
rows = int(sys.argv[3]) if len(sys.argv) > 3 else 40
absol = {}
for line in Path(noi).read_text().splitlines():
    m = re.match(r"DEF (\S+) 0x([0-9A-Fa-f]+)", line)
    if m and not m.group(1).startswith(("b_", "l_", "s_", ".__")):
        absol[m.group(1)] = int(m.group(2), 16)
syms = {k: v for k, v in absol.items() if (v & 0xFFFF) < 0x8000}
for sym in Path("build/gbc").glob("*.sym"):
    areas = collections.defaultdict(list)
    for line in sym.read_text(errors="replace").splitlines():
        m = re.match(r"\s*(\d+) (_\S+)\s+([0-9A-Fa-f]{8}) (\S+)", line)
        if m:
            areas[m.group(1)].append((m.group(2), int(m.group(3), 16), "G" in m.group(4)))
    for lst in areas.values():
        base = next((syms[n] - o for n, o, g in lst if g and n in syms), None)
        if base is not None:
            for n, o, _ in lst:
                syms.setdefault(n, base + o)
table = sorted((v, k) for k, v in syms.items())
keys = [v for v, _ in table]


def sym_of(full):
    i = bisect.bisect_right(keys, full) - 1
    if i < 0 or (keys[i] >> 16) != (full >> 16):
        return f"?{full:06X}"
    return table[i][1]


BASE = [0] * 256
for i, c in enumerate(bytes.fromhex(
        "0103020201010201050202020101020101030202010102010302020201010201"
        "0203020201010201020202020101020102030202030303010202020201010201")):
    BASE[i] = c
for i in range(0x40, 0xC0):
    BASE[i] = 2 if (i & 7) == 6 or (0x70 <= i < 0x78) else 1
BASE[0x76] = 0
for i, c in enumerate(bytes.fromhex(
        "0203030403040204020403020306020402030300030402040204030003000204"
        "0303020000040204040104000002040403030201000402040302040100000204")):
    BASE[0xC0 + i] = c
TAKEN_EXTRA = {0x20: 1, 0x28: 1, 0x30: 1, 0x38: 1, 0xC0: 3, 0xC8: 3, 0xD0: 3, 0xD8: 3,
               0xC2: 1, 0xCA: 1, 0xD2: 1, 0xDA: 1, 0xC4: 3, 0xCC: 3, 0xD4: 3, 0xDC: 3}


def cost_of(b):
    if b[0] == 0xCB:
        lo = b[1] & 7
        if lo != 6:
            return 2
        return 3 if 0x40 <= b[1] < 0x80 else 4
    return BASE[b[0]]


entry = {syms[p]: p for p in PHASES if p in syms}
wait = syms.get("_wait_vbl_done")
first = syms.get("_sound_update")
rx = re.compile(r"^\S+\s+(?:\+\d+\s+)?([0-9A-Fa-f]{3}):([0-9A-Fa-f]{4})\s+A:.*?\s{2,}((?:[0-9A-Fa-f]{2} ?)+)\s*$")
excl = collections.Counter()
calls = collections.Counter()
passes = []                                 # one Counter of phase costs per pass
cur = collections.Counter()
cursym = collections.Counter()
passsyms = []
phase, irq_save = "start", None
frames = 0
total = 0
prev = None                                 # (pc, op bytes, symbol, phase)
with open(trace, errors="replace") as fh:
    for line in fh:
        m = rx.match(line)
        if not m:
            continue
        bank = int(m.group(1), 16)
        addr = int(m.group(2), 16)
        ops = bytes.fromhex(m.group(3).replace(" ", ""))
        full = (bank << 16) | addr if 0x4000 <= addr < 0x8000 else addr
        if prev is not None:
            c = cost_of(prev[1])
            if addr != (prev[0] + len(prev[1])) & 0xFFFF:
                c += TAKEN_EXTRA.get(prev[1][0], 0)
            c *= 4
            excl[prev[2]] += c
            cur[prev[3]] += c
            cursym[prev[2]] += c
            total += c
        s = sym_of(full) if addr < 0x8000 else "RAM/HRAM"
        if addr == 0x40:
            frames += 1
        if addr in (0x40, 0x48) and irq_save is None:
            irq_save, phase = phase, "irq"
        if irq_save is None:
            if full == wait:
                phase = "vsync"
            elif full in entry:
                if full == first:
                    passes.append(cur)
                    passsyms.append(cursym)
                    cur = collections.Counter()
                    cursym = collections.Counter()
                phase = entry[full]
            elif s == "_main":
                phase = "main"
        i = bisect.bisect_left(keys, full)
        if i < len(keys) and keys[i] == full:
            calls[s] += 1
        prev = (addr, ops, s, phase)
        if irq_save is not None and ops[0] == 0xD9:  # RETI
            phase, irq_save = irq_save, None

passes = passes[1:]                         # the first is partial
passsyms = passsyms[1:]
n = max(len(passes), 1)
print(f"{frames:.0f} frames, {n} passes = {n / frames:.2f} passes/frame")
names = sorted({k for p in passes for k in p}, key=lambda k: -sum(p[k] for p in passes))
print(f"\n{'phase':22} {'avg/pass':>9} {'max':>8}")
for k in names:
    print(f"{k:22} {sum(p[k] for p in passes) / n:9.0f} {max(p[k] for p in passes):8}")
work = [sum(v for k, v in p.items() if k != "vsync") for p in passes]
print(f"{'work (no vsync)':22} {sum(work) / n:9.0f} {max(work):8}")
w = sorted(work)
print(f"work percentiles: p50 {w[n // 2]}  p90 {w[n * 9 // 10]}  p99 {w[n * 99 // 100]}; "
      f"passes under one frame of work: {sum(1 for x in work if x < 70224) * 100 // n}%")
print("\nworst passes:")
for i in sorted(range(n), key=lambda i: -work[i])[:8]:
    top = ", ".join(f"{k} {v}" for k, v in passes[i].most_common(4) if k != "vsync")
    print(f"  #{i}: {work[i]:7}  {top}")
    print("         " + ", ".join(f"{k} {v}" for k, v in passsyms[i].most_common(7)))
print(f"\n{'function (exclusive)':30} {'entries':>7} {'avg/pass':>9} {'%':>5}")
for k, v in excl.most_common(rows):
    print(f"{k:30} {calls[k]:7} {v / n:9.0f} {100 * v / total:5.1f}")
