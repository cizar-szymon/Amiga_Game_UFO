#!/usr/bin/env python3
"""Map a hang.log / CPU TRAP stack dump onto function names.
   python3 hangmap.py <hang.log> <nm output file>
The nm file: m68k-amigaos-nm -n <unstripped binary> (text symbols, sorted)."""
import bisect, re, sys

log, nmf = sys.argv[1], sys.argv[2]
syms = []
for l in open(nmf, errors="replace"):
    p = l.split()
    if len(p) >= 3 and p[1] in "tT":
        syms.append((int(p[0], 16), p[2]))
syms.sort()
addrs = [a for a, _ in syms]
anchor = dict((n, a) for a, n in syms).get("_amiga_trap_land")
text = open(log, errors="replace").read()
m = re.search(r"amiga_trap_land is at 0x([0-9a-f]+)", text)
base = int(m.group(1), 16) - anchor
end = addrs[-1] + 4096
seen = 0
for l in text.splitlines():
    if not l.strip().startswith("usp+"):
        continue
    off = int(l.split(":")[0].split("+")[1], 16)
    for k, w in enumerate(l.split(":")[1].split()):
        v = int(w, 16) - base
        if 0 <= v < end:
            i = bisect.bisect_right(addrs, v) - 1
            if i >= 0:
                print(f"usp+{off + 4 * k:03x}: {syms[i][1]}+0x{v - syms[i][0]:x}")
                seen += 1
    if seen > 60:
        break
