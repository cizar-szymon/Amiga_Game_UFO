#!/usr/bin/env python3
"""Put tools/shadowtest/shadow_new.inc (the version the test proved bit-exact)
into build/apply-amiga-patches.py, in place of the table + drawShadowHalfFix
block of patch 5u, as Python string literals in the patcher's style.
usage: sync_patch.py <shadow_new.inc> <apply-amiga-patches.py>"""
import sys

inc, patcher = sys.argv[1], sys.argv[2]
START = '        "/* AMIGA-PORT: shadowApply(dest, v) as a table.'
END = '        "/* double unit vector -> Q1.14, rounded */\\n"\n'

code = open(inc, encoding="utf-8").read().rstrip("\n") + "\n\n"
lit = []
for line in code.split("\n")[:-1]:
    s = line.replace("\\", "\\\\").replace('"', '\\"').replace("\t", "\\t")
    lit.append('        "%s\\n"\n' % s)

text = open(patcher, encoding="utf-8").read()
a = text.find(START)
b = text.find(END, a)
if a < 0 or b < 0 or text.find(START, a + 1) >= 0:
    sys.exit("markers not found exactly once")
text = text[:a] + "".join(lit) + text[b:]
open(patcher, "w", encoding="utf-8", newline="\n").write(text)
print("replaced %d bytes with %d lines" % (b - a, len(lit)))
