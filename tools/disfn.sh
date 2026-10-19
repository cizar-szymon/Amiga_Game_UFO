#!/bin/sh
# disfn.sh <binary> <function-name-regex> [max lines]: disassemble one function (68060 decoding)
m68k-amigaos-objdump -m m68k:68060 -d "$1" > /tmp/dis.txt
awk -v re="$2" '/^[0-9a-f]+ <.*>:$/ { show = ($2 ~ re) } show' /tmp/dis.txt | head -${3:-400}
