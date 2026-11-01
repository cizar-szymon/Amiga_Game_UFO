#!/usr/bin/env python3
"""Make a user/xcom1/battle.cfg for a late-game test battle (New Battle /
amigaAutoBattle) from an existing one: the alien race, mission and tech level,
X-COM soldiers in Power / Flying Suits and tanks in the craft.

    python tools/make_battlecfg.py IN OUT --mission 8 --race 4 [--tech 9]
           [--darkness 0] [--soldiers 6] [--tanks STR_HOVERTANK_PLASMA,STR_TANK_LASER_CANNON]
           [--armor STR_POWER_SUIT_UC,STR_FLYING_SUIT_UC]

Indices are the order of the rulesets (standard/xcom1):
  mission: 0 small scout .. 5 terror ship, 6 battleship, 7 supply ship,
           8 terror mission, 9 alien base, 10 base defense, 11 Cydonia
  race:    0 Sectoid, 1 Snakeman, 2 Ethereal, 3 Muton, 4 Floater, 5 mixed
"""
import argparse
import re

ap = argparse.ArgumentParser()
ap.add_argument("inp")
ap.add_argument("out")
ap.add_argument("--mission", type=int, required=True)
ap.add_argument("--race", type=int, required=True)
ap.add_argument("--tech", type=int, default=9)
ap.add_argument("--darkness", type=int, default=0)
ap.add_argument("--terrain", type=int, default=None)
ap.add_argument("--soldiers", type=int, default=6, help="soldiers left in the craft")
ap.add_argument("--tanks", default="STR_HOVERTANK_PLASMA,STR_TANK_LASER_CANNON")
ap.add_argument("--armor", default="STR_POWER_SUIT_UC,STR_FLYING_SUIT_UC")
a = ap.parse_args()

text = open(a.inp, "r", encoding="latin-1", newline="").read().replace("\r\n", "\n")

head = {"mission": a.mission, "alienRace": a.race, "alienTech": a.tech, "darkness": a.darkness}
if a.terrain is not None:
    head["terrain"] = a.terrain
for k, v in head.items():
    text, n = re.subn(r"(?m)^%s: .*$" % k, "%s: %d" % (k, v), text, count=1)
    assert n == 1, k

armors = [x for x in a.armor.split(",") if x]
CRAFT = "      craft:\n        lon: 0\n        lat: 0\n        type: STR_SKYRANGER\n        id: 1\n"
parts = text.split("\n    -\n      type: STR_SOLDIER\n")
out = [parts[0]]
kept = 0
for i, p in enumerate(parts[1:]):
    if CRAFT in p:
        if kept < a.soldiers:
            kept += 1
        else:
            p = p.replace(CRAFT, "", 1)
    if armors:
        p = re.sub(r"(?m)^      armor: .*$", "      armor: " + armors[i % len(armors)], p, count=1)
    out.append(p)
text = "\n    -\n      type: STR_SOLDIER\n".join(out)

tanks = [x for x in a.tanks.split(",") if x]
if tanks:
    v = "      vehicles:\n" + "".join(
        "        - type: %s\n          ammo: 255\n          size: 4\n" % t for t in tanks)
    text, n = re.subn(r"(?m)^      status: STR_READY\n", v + "      status: STR_READY\n", text, count=1)
    assert n == 1, "craft status line"

open(a.out, "w", encoding="latin-1", newline="\n").write(text)
print("soldiers in craft %d, tanks %d, armor %s" % (kept, len(tanks), armors))
