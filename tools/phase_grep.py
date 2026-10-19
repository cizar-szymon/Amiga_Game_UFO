#!/usr/bin/env python3
"""Print log lines matching a regex, prefixed with the perf-run phase they fall in.
   python phase_grep.py LOG REGEX"""
import re
import sys

PHASES = {"11": "idle", "12": "cursor/rotate", "13": "scroll+/zoom", "14": "scroll-", "15": "end"}
phase = "-"
rx = re.compile(sys.argv[2])
for line in open(sys.argv[1], errors="replace"):
    m = re.search(r"autoinput: wait (\d+)\s*$", line)
    if m and m.group(1) in PHASES:
        phase = PHASES[m.group(1)]
        continue
    if rx.search(line):
        print(f"{phase:<14} {line.rstrip()}")
