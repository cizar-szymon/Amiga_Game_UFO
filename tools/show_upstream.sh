#!/bin/sh
# show_upstream.sh <path under src/> <from line> <to line>  (pristine tarball)
mkdir -p /tmp/up && cd /tmp/up
tar xzf /repo/upstream/openxcom-00fbacde.tar.gz --wildcards "*/src/$1"
sed -n "$2,$3p" */src/$1
