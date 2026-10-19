#!/bin/sh
# 64-bit mul/div (trap on the 68060) per function whose name matches $2.
m68k-amigaos-objdump -m m68k:68060 -d "$1" > /tmp/dis.txt
R64='	(mul[su]l|div[su]l)[ 	]+[^ ]*,%?d[0-7],%?d[0-7]$'
awk -v re="$R64" -v fre="$2" '/^[0-9a-f]+ <.*>:$/ { fn=$2 } fn ~ fre && $0 ~ re { n[fn]++; print fn ": " $0 } ' /tmp/dis.txt | head -40
