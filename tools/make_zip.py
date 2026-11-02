#!/usr/bin/env python3
"""A release ZIP for the Amiga (after OpenDUNE's tools/make_zip.py).

  python tools/make_zip.py OUT.zip BASE NAME... [--add DIR ARCDIR]...

NAME: files or directories under BASE, stored with their path from BASE.
--add DIR ARCDIR: everything under DIR (recursive) stored under ARCDIR - the
private full archive's game data and converted music, never a public one.
Directories get their own entries, so empty ones (data/UFO) unpack too. Every
entry carries Unix permissions: Amiga UnZip turns them into the protection
bits, and without 755 a program unpacks without its E bit and cannot be
started. The converted music (*.raw, 8-bit samples) is stored, not deflated.
"""
import argparse
import os
import zipfile

EXECUTABLES = {"UFO", "UFO-noFPU", "MakeMusic"}


def put(z, path, arcname):
    info = zipfile.ZipInfo.from_file(path, arcname)
    info.create_system = 3
    mode = 0o755 if os.path.basename(arcname) in EXECUTABLES else 0o644
    info.external_attr = (0o100000 | mode) << 16
    info.compress_type = zipfile.ZIP_STORED if arcname.lower().endswith(".raw") else zipfile.ZIP_DEFLATED
    with open(path, "rb") as f:
        z.writestr(info, f.read())


def put_dir(z, arcname):
    info = zipfile.ZipInfo(arcname.rstrip("/") + "/")
    info.create_system = 3
    info.external_attr = (0o040000 | 0o755) << 16
    z.writestr(info, b"")


def walk(z, path, arcname):
    if os.path.isdir(path):
        put_dir(z, arcname)
        for name in sorted(os.listdir(path)):
            walk(z, os.path.join(path, name), arcname + "/" + name)
    else:
        put(z, path, arcname)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("base")
    ap.add_argument("names", nargs="+")
    ap.add_argument("--add", nargs=2, action="append", default=[], metavar=("DIR", "ARCDIR"))
    a = ap.parse_args()
    if os.path.exists(a.out):
        os.remove(a.out)
    with zipfile.ZipFile(a.out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for n in a.names:
            walk(z, os.path.join(a.base, n), n.replace("\\", "/"))
        for d, arc in a.add:
            for name in sorted(os.listdir(d)):
                walk(z, os.path.join(d, name), arc.rstrip("/") + "/" + name)
    print("%s: %d bytes" % (a.out, os.path.getsize(a.out)))


if __name__ == "__main__":
    main()
