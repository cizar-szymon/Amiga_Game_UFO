#!/bin/sh
# One-time setup of $HOME/build that build.sh takes for granted on the
# author's machine: yaml-cpp 0.6.3, unpacked as yamlcpp063, with the pristine
# tarball beside it (build.sh restores patched yaml files from it).
set -e
WORK=$HOME/build
mkdir -p "$WORK"
if [ ! -f "$WORK/yaml-cpp-0.6.3.tar.gz" ]; then
	cp /cache/yaml-cpp-0.6.3.tar.gz "$WORK/"
fi
if [ ! -d "$WORK/yamlcpp063" ]; then
	top=$(tar tzf "$WORK/yaml-cpp-0.6.3.tar.gz" | head -1 | cut -d/ -f1)
	tar xzf "$WORK/yaml-cpp-0.6.3.tar.gz" -C "$WORK"
	mv "$WORK/$top" "$WORK/yamlcpp063"
	echo "== prepare: yaml-cpp 0.6.3 unpacked to $WORK/yamlcpp063"
fi
exec sh "$@"
