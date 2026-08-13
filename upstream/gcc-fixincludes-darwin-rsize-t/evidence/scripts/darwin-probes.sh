#!/bin/bash
# Host Darwin protocol probes against unpatched HEAD stddef.h vs the patched header.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EV="$ROOT/evidence"
GCC_SRC="${GCC_SRC:-/Users/bjorn/.gcc/src/gcc}"
GXX="${GXX:-/Users/bjorn/.gcc/versions/17.0.0/bin/g++-17}"
GCC="${GCC:-/Users/bjorn/.gcc/versions/17.0.0/bin/gcc-17}"
LOG="$EV/logs/darwin-probes.log"
mkdir -p "$EV/logs" "$EV/headers/unpatched" "$EV/headers/patched"
exec > >(tee "$LOG") 2>&1

echo "==== start $(date -u +%Y-%m-%dT%H:%M:%SZ) ===="
"$GXX" -v
git -C "$GCC_SRC" rev-parse HEAD
git -C "$GCC_SRC" show HEAD:gcc/ginclude/stddef.h > "$EV/headers/unpatched/stddef.h"
cp "$GCC_SRC/gcc/ginclude/stddef.h" "$EV/headers/patched/stddef.h"

probe() {
  local tag="$1"; shift
  echo "----- $tag -----"
  echo "+ $*"
  local rc=0
  "$@" || rc=$?
  echo "exit $rc"
  echo "$tag rc=$rc"
}

for hdr in unpatched patched; do
  INC="$EV/headers/$hdr"
  for src in need-rsize.c full-include.c full-want.c full-then-need.c; do
    probe "darwin-$hdr-$src" "$GCC" -fsyntax-only -I "$INC" "$EV/probes/$src"
  done
  probe "darwin-$hdr-need-rsize.cc" "$GXX" -fsyntax-only -I "$INC" "$EV/probes/need-rsize.cc"
done

echo "==== original rsize.cc -fmodules (historical trigger) ===="
probe "darwin-rsize-cc-fmodules" "$GXX" -std=c++26 -fmodules -c "$EV/probes/rsize.cc" -o /tmp/rsize-cc-probe.o
echo "==== __has_feature(modules) ===="
printf '%s\n' '#if defined(__has_feature) && __has_feature(modules)' 'int yes;' '#else' 'int no;' '#endif' > /tmp/has-feature-modules.c
"$GXX" -std=c++26 -fmodules -dM -E /tmp/has-feature-modules.c | grep -E 'has_feature|modules' || true
"$GXX" -std=c++26 -fmodules -E /tmp/has-feature-modules.c | grep -E 'int (yes|no)'

echo "==== done $(date -u +%Y-%m-%dT%H:%M:%SZ) ===="
