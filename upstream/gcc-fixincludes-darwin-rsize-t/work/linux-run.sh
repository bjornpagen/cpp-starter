#!/bin/bash
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
LOG=/work/logs
mkdir -p "$LOG" /work/build /work/install
: > "$LOG/linux-build.log"
exec > >(tee -a "$LOG/linux-build.log") 2>&1

echo "==== start $(date -u +%Y-%m-%dT%H:%M:%SZ) ===="
uname -a
head -6 /etc/os-release
nproc
free -h | head -2
gcc -v
g++ -v

git config --global --add safe.directory /src
echo "SRC HEAD:"
git -C /src rev-parse HEAD
git -C /src log -1 --oneline
git -C /src status --short -- gcc/ginclude/stddef.h gcc/testsuite/gcc.dg/stddef-need-rsize-*.c || true

echo "==== apt deps ===="
apt-get update
apt-get install -y --no-install-recommends \
  flex bison gawk dejagnu expect tcl \
  libmpfr-dev libmpc-dev libisl-dev \
  patch python3

echo "==== unpatched/patched probes with image gcc 16.1.0 ===="
probe() {
  local tag="$1"; shift
  echo "----- $tag -----"
  echo "+ $*"
  local rc=0
  "$@" 2>&1 || rc=$?
  echo "exit $rc"
  echo "$tag rc=$rc" >> "$LOG/linux-probe-summary.txt"
}

: > "$LOG/linux-probe-summary.txt"
for hdr in unpatched patched; do
  INC=/probes/${hdr}-include
  for src in need-rsize.c need-rsize.cc full-include.c full-want.c full-then-need.c; do
    if [[ "$src" == *.cc ]]; then CC=g++; else CC=gcc; fi
    probe "image16-$hdr-$src" "$CC" -fsyntax-only -I "$INC" "/probes/probes/$src"
  done
done

echo "==== configure patched trunk ===="
cd /work/build
/src/configure \
  --prefix=/work/install \
  --enable-languages=c,c++ \
  --disable-nls \
  --enable-checking=release \
  --disable-bootstrap \
  --disable-multilib \
  --with-system-zlib
echo "==== make -j$(nproc) ===="
make -j"$(nproc)"
echo "==== make install ===="
make install
echo "==== patched xgcc identity ===="
/work/build/gcc/xgcc -v
/work/install/bin/gcc -v
/work/install/bin/g++ -v

echo "==== probes with patched trunk xgcc ===="
XGCC=/work/build/gcc/xgcc
XB="-B/work/build/gcc"
probe "xgcc-need-rsize.c" "$XGCC" $XB -fsyntax-only /probes/probes/need-rsize.c
probe "xgcc-need-rsize.cc" "$XGCC" $XB -x c++ -fsyntax-only /probes/probes/need-rsize.cc
probe "xgcc-full-include.c" "$XGCC" $XB -fsyntax-only /probes/probes/full-include.c
probe "xgcc-full-want.c" "$XGCC" $XB -fsyntax-only /probes/probes/full-want.c
probe "xgcc-full-then-need.c" "$XGCC" $XB -fsyntax-only /probes/probes/full-then-need.c

echo "==== installed gcc probes ===="
export PATH=/work/install/bin:$PATH
probe "inst-need-rsize.c" gcc -fsyntax-only /probes/probes/need-rsize.c
probe "inst-need-rsize.cc" g++ -fsyntax-only /probes/probes/need-rsize.cc
probe "inst-full-include.c" gcc -fsyntax-only /probes/probes/full-include.c
probe "inst-full-want.c" gcc -fsyntax-only /probes/probes/full-want.c
probe "inst-full-then-need.c" gcc -fsyntax-only /probes/probes/full-then-need.c

echo "==== testsuite: new tests + nearest stddef tests ===="
cd /work/build
make -j"$(nproc)" check-gcc RUNTESTFLAGS="dg.exp=stddef-need-rsize*.c"
make -j"$(nproc)" check-gcc RUNTESTFLAGS="dg.exp=c23-stddef*.c"
echo "==== .sum extracts ===="
find /work/build -name '*.sum' | head
if [ -f /work/build/gcc/testsuite/gcc/gcc.sum ]; then
  grep -E 'stddef-need-rsize|c23-stddef' /work/build/gcc/testsuite/gcc/gcc.sum || true
fi
echo "==== done $(date -u +%Y-%m-%dT%H:%M:%SZ) ===="
