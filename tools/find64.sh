#!/bin/sh
# Count 68020+ instructions the 68060 does NOT implement in hardware (trapped
# and emulated by 68060.library): 64-bit MULS.L/MULU.L (Dh:Dl result) and
# 64/32 DIVS.L/DIVU.L (Dr:Dq). gcc -mcpu=68020 uses the 64-bit multiply for
# every division by a constant (multiply by the reciprocal).
# objdump must be told the CPU, or it decodes these as ".short".
# usage: find64.sh <binary> [function-regex]
BIN=$1
FN=${2:-}
DIS=/tmp/dis.txt
m68k-amigaos-objdump -m m68k:68060 -d "$BIN" > $DIS
echo "== $BIN"
# objdump spells the 64-bit forms with THREE operands: "mulsl #k,d0,d2"
# (Dh,Dl) and "divsl d2,d1,d0" (64/32: Dr,Dq). "divsll" is the 32-bit
# DIVSL.L with remainder, which the 060 does implement.
R64='	(mul[su]l|div[su]l)[ 	]+[^ ]*,%?d[0-7],%?d[0-7]$'
echo "64-bit mul/div (TRAP on 060):  $(grep -c -E "$R64" $DIS)"
echo "== sample"
grep -E "$R64" $DIS | head -5
echo "== functions with the most 64-bit forms"
awk -v re="$R64" '/^[0-9a-f]+ <.*>:$/ { fn=$2 } $0 ~ re { n[fn]++ } END { for (f in n) print n[f], f }' $DIS | sort -rn | head -25
if [ -n "$FN" ]; then
	echo "== disassembly of functions matching $FN"
	awk -v re="$FN" '/^[0-9a-f]+ <.*>:$/ { show = ($2 ~ re) } show' $DIS | head -250
fi
