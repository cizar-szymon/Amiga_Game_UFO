#!/usr/bin/env python3
"""Globe pictures per second per test phase, from AmiXcom's "globe:" probe.

  globe: 10 calls in X ms over F frames: base B (cache c + ocean o + land l +
         shadow s + detail d ms each), radar R (r ms each), markers m ms total

Phases are the odd autoinput waits of perf-scenes.ps1 (11 idle, 12 rotate,
13 zoom). B is how many times the globe itself (ocean, land, shadow) was
redrawn - what the player sees move - against the frames shown."""
import re
import sys

PHASES = {"11": "time running", "12": "rotate", "13": "zoom"}
RX = re.compile(r"globe: 10 calls in (\d+) ms over (\d+) frames: base (\d+) \(cache (\d+) \+ ocean (\d+) \+ land (\d+) \+ shadow (\d+) \+ detail (\d+) ms each\)")


def main(path):
    phase, first, acc = None, False, {}
    for line in open(path, errors="replace"):
        m = re.search(r"autoinput: wait (\d+)\s*$", line)
        if m and (m.group(1) in PHASES or m.group(1) == "15"):
            phase = m.group(1) if m.group(1) in PHASES else None
            first = True
            continue
        m = RX.search(line)
        if m and phase:
            if first:          # straddles the change
                first = False
                continue
            ms, fr, base, cache, ocean, land, shadow, detail = map(int, m.groups())
            a = acc.setdefault(phase, [0, 0, 0, [0, 0, 0, 0, 0]])
            a[0] += ms; a[1] += fr; a[2] += base
            for i, v in enumerate((cache, ocean, land, shadow, detail)):
                a[3][i] += v * base
    print(f"== {path}")
    for ph in sorted(acc):
        ms, fr, base, parts = acc[ph]
        each = [p / base if base else 0 for p in parts]
        print(f"  {PHASES[ph]:<13} {fr * 1000 / ms if ms else 0:5.1f} frames/s, globe redrawn {base * 1000 / ms if ms else 0:5.1f}/s"
              f"  (one redraw: cache {each[0]:.0f} + ocean {each[1]:.0f} + land {each[2]:.0f} + shadow {each[3]:.0f} + detail {each[4]:.0f} ms)")


if __name__ == "__main__":
    for p in sys.argv[1:]:
        main(p)
