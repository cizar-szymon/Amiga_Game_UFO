"""Match the PC X-COM sound effects to the Amiga UFO's 8SVX files.

The Amiga release keeps its effects as named 8SVX files, the PC one as
numbered WAVs in SAMPLE.CAT (Geoscape) and SAMPLE2.CAT (battles), and the
games refer to them by number. Most Amiga files look like the PC recordings
at a lower rate, so each PC sound is compared with every Amiga file: both
resampled to 4 kHz, the normalised cross-correlation of the waveforms and of
their envelopes, best lag. Prints a table and the best match per PC index.

    python tools/match_amiga_sfx.py data/UFO-PC/SAMPLE2.CAT data/UFO-Amiga/Music/TacticalFX
"""
import glob
import os
import struct
import sys

import numpy as np
from scipy.signal import correlate, resample_poly

RATE = 4000


def cat_wavs(path):
    d = open(path, 'rb').read()
    n = struct.unpack('<I', d[0:4])[0] // 8
    out = []
    for i in range(n):
        off, ln = struct.unpack('<II', d[i * 8:i * 8 + 8])
        e = d[off:off + ln]
        if e.find(b'RIFF') < 0:          # a bare sample, no WAV header
            out.append((11025, np.frombuffer(e[1 + e[0]:], dtype=np.uint8).astype(np.float64) - 128))
            continue
        e = e[e.find(b'RIFF'):]          # past the name
        f = e.find(b'fmt ')
        rate = struct.unpack('<I', e[f + 12:f + 16])[0]
        p = e.find(b'data')
        size = struct.unpack('<I', e[p + 4:p + 8])[0]
        pcm = np.frombuffer(e[p + 8:p + 8 + size], dtype=np.uint8).astype(np.float64) - 128
        out.append((rate if 4000 <= rate <= 48000 else 11025, pcm))
    return out


def svx(path):
    d = open(path, 'rb').read()
    p = d.find(b'VHDR')
    rate = struct.unpack('>H', d[p + 8 + 12:p + 8 + 14])[0]
    p = d.find(b'BODY')
    size = struct.unpack('>I', d[p + 4:p + 8])[0]
    pcm = np.frombuffer(d[p + 8:p + 8 + size], dtype=np.int8).astype(np.float64)
    return rate, pcm


def norm(rate, pcm):
    x = resample_poly(pcm, RATE, rate)
    x = x - x.mean()
    return x


def env(x):
    k = RATE // 100
    e = np.sqrt(np.convolve(x * x, np.ones(k) / k, mode='same'))
    return e - e.mean()


def ncc(a, b):
    if len(a) < 8 or len(b) < 8:
        return 0.0
    c = correlate(a, b, mode='full', method='fft')
    return float(c.max() / (np.linalg.norm(a) * np.linalg.norm(b) + 1e-9))


def main():
    pcs = [norm(r, p) for r, p in cat_wavs(sys.argv[1])]
    files = sorted(glob.glob(os.path.join(sys.argv[2], '*.8svx')))
    am = [(os.path.basename(f)[:-5], norm(*svx(f))) for f in files]
    for i, x in enumerate(pcs):
        scores = []
        for name, y in am:
            w = ncc(x, y)
            e = ncc(env(x), env(y))
            lr = min(len(x), len(y)) / max(len(x), len(y))
            scores.append((0.6 * w + 0.4 * e, w, e, lr, name))
        scores.sort(reverse=True)
        best = scores[0]
        alt = ', '.join('%s %.2f' % (s[4], s[0]) for s in scores[1:3])
        print('%2d  %.2fs  %-9s score %.2f (wave %.2f env %.2f len %.2f)   next: %s' % (
            i, len(x) / RATE, best[4], best[0], best[1], best[2], best[3], alt))


if __name__ == '__main__':
    main()
