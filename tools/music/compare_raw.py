#!/usr/bin/env python3
"""Compares two directories of AmiXcom music (*.raw: 8-bit signed mono,
22050 Hz): identical files, else how alike they sound - the length, the share
of equal bytes, and the correlation of the loudness envelope (RMS over 50 ms),
1.0 = the same music at the same moments.

  python tools/music/compare_raw.py DIR_A DIR_B
"""
import glob
import os
import sys

import numpy as np

WIN = 1102   # 50 ms at 22050 Hz


def env(x):
    n = len(x) // WIN
    x = x[:n * WIN].astype(np.float64).reshape(n, WIN)
    return np.sqrt((x * x).mean(axis=1))


def main(a, b):
    for f in sorted(glob.glob(os.path.join(a, '*.raw'))):
        name = os.path.basename(f)
        g = os.path.join(b, name)
        if not os.path.exists(g):
            print('%-13s missing in %s' % (name, b))
            continue
        x = np.fromfile(f, dtype=np.int8)
        y = np.fromfile(g, dtype=np.int8)
        if len(x) == len(y) and np.array_equal(x, y):
            print('%-13s identical (%d s)' % (name, len(x) // 22050))
            continue
        n = min(len(x), len(y))
        same = float(np.mean(x[:n] == y[:n]))
        ex, ey = env(x[:n]), env(y[:n])
        c = float(np.corrcoef(ex, ey)[0, 1]) if len(ex) > 2 and ex.std() > 0 and ey.std() > 0 else float('nan')
        print('%-13s length %d / %d s, equal bytes %.0f%%, envelope %.3f, RMS %.1f / %.1f'
              % (name, len(x) // 22050, len(y) // 22050, same * 100, c,
                 float(np.sqrt((x[:n].astype(float) ** 2).mean())), float(np.sqrt((y[:n].astype(float) ** 2).mean()))))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
