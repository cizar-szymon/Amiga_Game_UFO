#!/usr/bin/env python3
"""Average AmiXcom's perf probes per test phase.

perf-run.ps1 marks phases with odd autoinput waits ("autoinput: wait 11" ...).
Every probe line after a marker belongs to that phase; the first window after
a marker straddles the change and is dropped.

Probes used (all per 100 frames):
  us2:  think T | blit B = terr .. + scrblit big X/n small Y/n classify .. fill F + rest | flip L (us)
        battle, amigaPerfLog: the work per frame, in microseconds
  cache: hit H | partial P/R rects ... | full N ... | why: ... scroll S
        battle frame cache: frames served from cache / partial / full redraws
  geo:  100 frames in N ms: think T, blit B, flip F, events E/n, other O ms
        geoscape (and battle when no map was drawn), milliseconds
  perf: 100 frames in N ms: c2p C ms ...  (sdlmini, always; C = screen copy)
The game caps at 50 fps, so fps saturates in a fast emulator: compare BUSY
(the work per frame), not fps.
"""
import re
import sys

PHASES = {"11": "idle", "12": "cursor/rotate", "13": "scroll+/zoom",
          "14": "scroll-", "15": "end"}
RX = {
    "us2":   re.compile(r"us2: 100 fr: think (\d+) \| blit (\d+) = terr (\d+) \+ scrblit big (\d+)/\d+ small (\d+)/\d+ classify \d+ fill (\d+) .*?\| flip (\d+)"),
    "cache": re.compile(r"cache: hit (\d+) \| partial (\d+)/\d+ rects .*?\| full (\d+) .*?scroll (\d+)"),
    "geo":   re.compile(r"geo: 100 frames in (\d+) ms: think (\d+), blit (\d+), flip (\d+), events (\d+)/\d+, other (-?\d+)"),
    "perf":  re.compile(r"perf: 100 frames in (\d+) ms: c2p (\d+) ms"),
    "fskip": re.compile(r"perf: .*fskip (\d+) verr (\d+)"),
    "fpart": re.compile(r"perf: .*fpart (\d+) \((\d+) px\)"),
}


def avg(rows):
    return [sum(r[i] for r in rows) / len(rows) for i in range(len(rows[0]))]


def main(path):
    phase, skip, data = None, {}, {}
    for line in open(path, errors="replace"):
        m = re.search(r"autoinput: wait (\d+)\s*$", line)
        if m and m.group(1) in PHASES:
            phase, skip = m.group(1), {k: True for k in RX}
            continue
        if phase in (None, "15"):
            continue
        for kind, rx in RX.items():
            m = rx.search(line)
            if m:
                if skip.get(kind):
                    skip[kind] = False
                else:
                    data.setdefault((phase, kind), []).append([int(g) for g in m.groups()])
    print(f"== {path}")
    for ph in sorted(PHASES):
        out = []
        if (ph, "us2") in data:
            t, b, terr, big, small, fill, fl = [v / 100000 for v in avg(data[(ph, "us2")])]
            out.append(f"busy {t + b + fl:5.1f} ms/fr = think {t:4.1f} + blit {b:4.1f} (terr {terr:.1f}, scrblit {big + small:.1f}, fill {fill:.1f}) + flip {fl:4.1f}")
        if (ph, "geo") in data and (ph, "us2") not in data:
            w, t, b, fl, ev, oth = [v / 100 for v in avg(data[(ph, "geo")])]
            out.append(f"busy {t + b + fl + ev:5.1f} ms/fr = think {t:4.1f} + blit {b:4.1f} + flip {fl:4.1f} + events {ev:4.1f} (frame {w:.1f}, {1000 / w if w else 0:.0f} fps)")
        if (ph, "cache") in data:
            h, p, f, sc = avg(data[(ph, "cache")])
            out.append(f"cache/100fr: hit {h:.0f} partial {p:.0f} full {f:.0f} scroll {sc:.0f}")
        if (ph, "perf") in data:
            w, c = avg(data[(ph, "perf")])
            out.append(f"screen copy {c / 100:.2f} ms/fr, {100000 / w if w else 0:.0f} fps")
        if (ph, "fskip") in data:
            fs, ve = avg(data[(ph, "fskip")])
            vmax = max(r[1] for r in data[(ph, "fskip")])
            out.append(f"unchanged frames: {fs:.0f}/100 skipped (or verified); verify mismatches so far: {vmax}")
        if (ph, "fpart") in data:
            fp, px = avg(data[(ph, "fpart")])
            out.append(f"partial frames: {fp:.0f}/100, {px / fp if fp else 0:.0f} px redrawn per partial frame")
        if out:
            print(f"  {PHASES[ph]:<14} " + "\n  " .join([""] + out).lstrip("\n").replace("\n  ", "\n                 "))


if __name__ == "__main__":
    main(sys.argv[1])
