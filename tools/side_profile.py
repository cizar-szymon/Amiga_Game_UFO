#!/usr/bin/env python3
"""Where the time of each side's turn goes, from an AmiXcom oxc.log
(amigaPerfLog: 1, build with the 'endturn:' probe).

For every stretch between two 'endturn:' lines it sums the 'prof:' windows
(100 frames each: think / map / ui / flip / ev, the rest is waiting for the
frame or animation timer), the 'slow frame' lines (frames over 250 ms) and the
'ai think:' lines, and counts frames per screen (state name).

    python tools/side_profile.py oxc.log
"""
import re
import sys
from collections import Counter, defaultdict

PROF = re.compile(r"prof: 100 frames in (\d+) ms \| think (\d+) .*?\| map (\d+) .*?\| ui (\d+) .*?\| flip (\d+) .*?\| ev (\d+) .*\| (\S+)$")
GEO = re.compile(r"geo: 100 frames in (\d+) ms: think (\d+), blit (\d+), flip (\d+), events (\d+)/\d+, other (-?\d+) ms.* in (\S+)$")
SLOW = re.compile(r"slow frame: think (\d+) blit (\d+) flip (\d+) ms in (\S+)")
END = re.compile(r"endturn: side (\d+) took (\d+) ms(?:, burst (\d+) ticks in (\d+) frames)?")
AIT = re.compile(r"ai think: unit (\d+) (\d+) ms")
NAMES = {"0": "X-COM", "1": "aliens", "2": "civilians"}


def main(path):
    acc = defaultdict(int)
    states = Counter()
    slow = []
    thinks = []
    for line in open(path, errors="replace"):
        m = PROF.search(line)
        if m:
            t, th, mp, ui, fl, ev, st = m.groups()
            for k, v in zip(("wall", "think", "map", "ui", "flip", "ev"), (t, th, mp, ui, fl, ev)):
                acc[k] += int(v)
            acc["frames"] += 100
            states[st.replace("N8OpenXcom", "")] += 100
            continue
        m = GEO.search(line)
        if m and "Battlescape" in m.group(7):
            # a battle window in which no map was drawn: the hidden-movement screen
            t, th, bl, fl, ev, oth, st = m.groups()
            for k, v in (("wall", t), ("think", th), ("map", bl), ("flip", fl), ("ev", ev)):
                acc[k] += int(v)
            acc["frames"] += 100
            states["hidden movement"] += 100
            continue
        m = SLOW.search(line)
        if m:
            slow.append(int(m.group(1)) + int(m.group(2)) + int(m.group(3)))
            continue
        m = AIT.search(line)
        if m:
            thinks.append(int(m.group(2)))
            continue
        m = END.search(line)
        if m:
            side, took, bt, bf = m.groups()
            print("== side %s (%s) took %.1f s" % (side, NAMES.get(side, "?"), int(took) / 1000.0)
                  + (", burst %s ticks in %s frames" % (bt, bf) if bt else ""))
            if acc["frames"]:
                w = acc["wall"] or 1
                rest = acc["wall"] - acc["think"] - acc["map"] - acc["ui"] - acc["flip"] - acc["ev"]
                print("   prof windows: %d frames in %.1f s = %.1f fps" % (acc["frames"], w / 1000.0, acc["frames"] * 1000.0 / w))
                for k in ("think", "map", "ui", "flip", "ev"):
                    print("     %-6s %6.1f s  %3d%%" % (k, acc[k] / 1000.0, acc[k] * 100 // w))
                print("     %-6s %6.1f s  %3d%%  (waiting: frame cap / timers)" % ("rest", rest / 1000.0, rest * 100 // w))
                print("   screens: " + ", ".join("%s %d" % kv for kv in states.most_common(4)))
            if slow:
                print("   slow frames: %d, %.1f s, longest %s ms" % (len(slow), sum(slow) / 1000.0, sorted(slow)[-3:]))
            if thinks:
                print("   ai think lines: %d, %.1f s, longest %s ms" % (len(thinks), sum(thinks) / 1000.0, sorted(thinks)[-3:]))
            acc.clear(); states.clear(); slow = []; thinks = []


if __name__ == "__main__":
    main(sys.argv[1])
