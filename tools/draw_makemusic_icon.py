#!/usr/bin/env python3
"""The picture of MakeMusic's Workbench icon: a loudspeaker with sound waves
and two notes, drawn here (no game graphics), on the Workbench grey - after
OpenDUNE's tools/icon/draw_icons.py, in the X-COM interface's green and blue.

  python tools/draw_makemusic_icon.py OUT.png
then tools/make_icon.py OUT.info --frame OUT.png --crop 0 0 54 44 (make_release.py does both).
"""
import sys

from PIL import Image, ImageDraw

S = 8                       # drawn 8 times larger, then scaled down (smooth edges)
BACK = (0xAA, 0xAA, 0xAA)   # the Workbench grey


def main(out):
    w, h = 54, 44
    im = Image.new('RGB', (w * S, h * S), BACK)
    d = ImageDraw.Draw(im)
    k = S
    dark = (20, 24, 40)
    # the loudspeaker: the magnet box and the cone
    d.rectangle([3 * k, 17 * k, 11 * k, 29 * k], fill=(60, 80, 120), outline=dark, width=k)
    d.polygon([(11 * k, 17 * k), (21 * k, 8 * k), (21 * k, 38 * k), (11 * k, 29 * k)], fill=(110, 140, 190), outline=dark)
    d.line([(11 * k, 17 * k), (21 * k, 8 * k), (21 * k, 38 * k), (11 * k, 29 * k)], fill=dark, width=k)
    # sound waves
    for r in (6, 10):
        d.arc([(21 - r) * k, (23 - r) * k, (21 + r) * k, (23 + r) * k], -45, 45, fill=(250, 250, 255), width=int(1.6 * k))
    # two beamed eighth notes, X-COM green
    note, edge = (90, 210, 90), (20, 70, 20)
    for cx, cy in ((36, 31), (47, 27)):
        d.ellipse([(cx - 4) * k, (cy - 3) * k, (cx + 3) * k, (cy + 3) * k], fill=note, outline=edge, width=k)
    d.line([(39 * k, 31 * k), (39 * k, 9 * k)], fill=edge, width=2 * k)
    d.line([(50 * k, 27 * k), (50 * k, 5 * k)], fill=edge, width=2 * k)
    d.polygon([(38 * k, 9 * k), (51 * k, 4 * k), (51 * k, 9 * k), (38 * k, 14 * k)], fill=note, outline=edge)
    im.resize((w, h), Image.LANCZOS).save(out)


if __name__ == '__main__':
    main(sys.argv[1])
