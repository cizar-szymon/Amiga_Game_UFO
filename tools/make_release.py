"""Build the AmiXcom release archive (LHA) from work/ and AmiXcom/release/.

  python tools/make_release.py 0.86

-> releases/AmiXcom-<ver>.lha, top folder AmiXcom/. Contains the two RTG
binaries with the fast allocator (UFO = FPU, UFO-noFPU = soft float), their
icons (1 MB stack), OpenXcom's data (common/, standard/, translations) and the
release notes. Never any X-COM data: data/UFO, data/TFTD and data/UFO_AMIGA
hold only their README files. No .ybc caches, no user/ folder (the first start
writes the options).

  python tools/make_release.py 0.86 --full

-> releases/AmiXcom-<ver>-full-PRIVATE.lha: the same plus the player's own
game data (work/data/UFO, the Amiga music in data/UFO-Amiga/Music) and the
pre-rendered music (work/user/music/*.raw + source.txt), so the first start
on another Amiga needs no copying and no conversion. For the owner's own
machines only - never published.
"""
import os
import shutil
import subprocess
import sys

UFO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORK = os.path.join(UFO, 'work')
REL = os.path.join(UFO, 'AmiXcom', 'release')
OUT = os.path.join(UFO, 'releases')


def text_for_amiga(src, dst, encoding):
    t = open(src, encoding='utf-8').read().replace('\r\n', '\n')
    open(dst, 'wb').write(t.encode(encoding))


def add_full(stage, data):
    """The owner's game data and the converted music (private archive)."""
    shutil.rmtree(os.path.join(data, 'UFO'))
    shutil.copytree(os.path.join(WORK, 'data', 'UFO'), os.path.join(data, 'UFO'))
    amiga = os.path.join(os.path.dirname(WORK), 'data', 'UFO-Amiga', 'Music')
    dst = os.path.join(data, 'UFO_AMIGA')
    for f in os.listdir(amiga):
        s = os.path.join(amiga, f)
        if os.path.isdir(s):
            shutil.copytree(s, os.path.join(dst, f))
        else:
            shutil.copyfile(s, os.path.join(dst, f))
    music = os.path.join(WORK, 'user', 'music')
    out = os.path.join(stage, 'user', 'music')
    os.makedirs(out)
    tunes = [f for f in os.listdir(music) if f.endswith('.raw')]
    if len(tunes) < 14:
        raise SystemExit('only %d tunes in %s - run the game once to render them' % (len(tunes), music))
    for f in tunes + ['source.txt']:
        shutil.copyfile(os.path.join(music, f), os.path.join(out, f))


def main():
    ver = sys.argv[1]
    full = '--full' in sys.argv[2:]
    stage_root = os.path.join(OUT, 'stage')
    stage = os.path.join(stage_root, 'AmiXcom')
    shutil.rmtree(stage_root, ignore_errors=True)
    os.makedirs(stage)

    for src, dst in (('openxcom-rtg-fpu-fm', 'UFO'), ('openxcom-rtg-fm', 'UFO-noFPU')):
        shutil.copyfile(os.path.join(WORK, src), os.path.join(stage, dst))
    icon = os.path.join(OUT, 'UFO.info')
    subprocess.run([sys.executable, os.path.join(UFO, 'tools', 'make_icon.py'), icon,
                    '--frame', os.path.join(UFO, 'data', 'ICON_bmp.bmp'), '--crop', '0', '0', '461', '410',
                    '--size', '72', '64', '--colors', '64', '--stack', '1000000'], check=True)
    for dst in ('UFO.info', 'UFO-noFPU.info'):
        shutil.copyfile(icon, os.path.join(stage, dst))

    data = os.path.join(stage, 'data')
    ign = shutil.ignore_patterns('*.ybc')
    for d in ('common', 'standard'):
        shutil.copytree(os.path.join(WORK, 'data', d), os.path.join(data, d), ignore=ign)
    for d in ('UFO', 'TFTD'):                     # README only - never game data
        os.makedirs(os.path.join(data, d))
        readme = os.path.join(WORK, 'data', d, 'README.txt')
        if os.path.exists(readme):
            shutil.copyfile(readme, os.path.join(data, d, 'README.txt'))
    os.makedirs(os.path.join(data, 'UFO_AMIGA'))
    text_for_amiga(os.path.join(REL, 'UFO_AMIGA_README.txt'), os.path.join(data, 'UFO_AMIGA', 'README.txt'), 'ascii')

    text_for_amiga(os.path.join(REL, 'AmiXcom_release.txt'), os.path.join(stage, 'AmiXcom_release.txt'), 'iso-8859-2')
    text_for_amiga(os.path.join(REL, 'AmiXcom_release_en.txt'), os.path.join(stage, 'AmiXcom_release_en.txt'), 'ascii')

    if full:
        add_full(stage, data)
    # nothing of the player's may slip in
    for root, _dirs, files in os.walk(data):
        for f in files:
            p = os.path.relpath(os.path.join(root, f), data).replace('\\', '/')
            top = p.split('/')[0]
            if not full and top in ('UFO', 'TFTD', 'UFO_AMIGA') and not p.endswith('README.txt'):
                raise SystemExit('game data in the stage: ' + p)

    arc = 'AmiXcom-%s%s.lha' % (ver, '-full-PRIVATE' if full else '')
    if os.path.exists(os.path.join(OUT, arc)):
        os.remove(os.path.join(OUT, arc))
    subprocess.run(['docker', 'run', '--rm', '-v', '%s:/o' % OUT, 'amixcom-build', 'sh', '-c',
                    'cd /o/stage && lha aq /o/%s AmiXcom' % arc], check=True)
    size = sum(os.path.getsize(os.path.join(r, f)) for r, _d, fs in os.walk(stage) for f in fs)
    print('%s: %d bytes, unpacked %.1f MB' % (os.path.join(OUT, arc), os.path.getsize(os.path.join(OUT, arc)), size / 1e6))


if __name__ == '__main__':
    main()
