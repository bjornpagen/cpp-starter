#!/bin/bash
# 3-stage bootstrap of default languages, then make -k check.
# PID 1 of the container must NOT be this script. Run it via finch exec.
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
LOG=/work/logs
mkdir -p "$LOG" /work/build /work/install
exec > >(tee -a "$LOG/bootstrap.log") 2>&1

echo "==== start $(date -u +%Y-%m-%dT%H:%M:%SZ) ===="
uname -a
cat /etc/os-release | head -8
echo "nproc=$(nproc)"
free -h | head -3
df -h /work /src | head -10
echo "==== host gcc (image) ===="
gcc -v
echo "==== src identity ===="
git -C /src rev-parse HEAD
git -C /src log -1 --format='%H %ci %s'
git -C /src status --short -- \
  gcc/ginclude/stddef.h \
  gcc/testsuite/gcc.dg/stddef-need-rsize-1.c \
  gcc/testsuite/gcc.dg/stddef-need-rsize-2.c \
  gcc/testsuite/gcc.dg/stddef-need-rsize-3.c \
  gcc/testsuite/gcc.dg/stddef-need-rsize-4.c \
  gcc/testsuite/g++.dg/stddef-need-rsize-1.C || true
git -C /src diff --stat HEAD -- gcc/ginclude/stddef.h gcc/testsuite/gcc.dg/stddef-need-rsize-*.c gcc/testsuite/g++.dg/stddef-need-rsize-1.C || true

echo "==== apt deps ===="
apt-get update
apt-get install -y --no-install-recommends \
  flex bison gawk dejagnu expect tcl \
  libgmp-dev libmpfr-dev libmpc-dev libisl-dev \
  patch python3 git

echo "==== configure (default languages, 3-stage bootstrap) ===="
cd /work/build
if [[ ! -f Makefile ]]; then
  /src/configure \
    --prefix=/work/install \
    --disable-nls \
    --disable-multilib \
    --with-system-zlib \
    --enable-checking=yes
else
  echo "Makefile already present; not reconfiguring"
fi

echo "==== configured languages ===="
grep -E "enable_languages|CHECKING|disable_bootstrap|bootstrap" /work/build/config.status | head -40 || true
sed -n '1,80p' /work/build/gcc/config.log > "$LOG/gcc-config-head.txt" || true
grep -E 'checking .* language|The following languages|enable_languages=' /work/build/config.log | tee "$LOG/languages.txt" || true

echo "==== make -j$(nproc) (3-stage bootstrap) ===="
# Top-level 'all' bootstraps when --disable-bootstrap was not given.
make -j"$(nproc)"
echo "==== bootstrap finished $(date -u +%Y-%m-%dT%H:%M:%SZ) ===="
/work/build/prev-gcc/xgcc -v > "$LOG/stage1-xgcc-v.txt" 2>&1 || true
/work/build/gcc/xgcc -v > "$LOG/stage3-xgcc-v.txt" 2>&1
/work/build/gcc/xg++ -v > "$LOG/stage3-xgxx-v.txt" 2>&1 || true
cat "$LOG/stage3-xgcc-v.txt"

echo "==== make install ===="
make install
/work/install/bin/gcc -v > "$LOG/installed-gcc-v.txt" 2>&1
cat "$LOG/installed-gcc-v.txt"

echo "==== make -k check $(date -u +%Y-%m-%dT%H:%M:%SZ) ===="
# Do not fail the script on individual test FAILs; -k continues.
set +e
make -k -j"$(nproc)" check
CHECK_RC=$?
set -e
echo "make -k check exit $CHECK_RC" | tee "$LOG/check-exit.txt"
echo "==== check finished $(date -u +%Y-%m-%dT%H:%M:%SZ) ===="

echo "==== sum files ===="
find /work/build -name '*.sum' | sort | tee "$LOG/sum-files.txt"
python3 - << 'PY'
import pathlib, re, collections
root = pathlib.Path("/work/build")
out = pathlib.Path("/work/logs/check-summary.txt")
counts = collections.Counter()
fails = []
unresolved = []
for p in sorted(root.rglob("*.sum")):
    text = p.read_text(errors="replace")
    local = collections.Counter()
    for line in text.splitlines():
        m = re.match(r"^(PASS|FAIL|XFAIL|XPASS|UNSUPPORTED|UNRESOLVED|ERROR|WARNING):\s+(.*)$", line)
        if m:
            kind, name = m.group(1), m.group(2)
            local[kind] += 1
            counts[kind] += 1
            if kind in ("FAIL", "XPASS", "ERROR"):
                fails.append(f"{p}:{line}")
            if kind == "UNRESOLVED":
                unresolved.append(f"{p}:{line}")
    print(f"{p.relative_to(root)} " + " ".join(f"{k}={local[k]}" for k in ("PASS","FAIL","XFAIL","XPASS","UNSUPPORTED","UNRESOLVED","ERROR") if local[k]))
lines = ["# make -k check summary", ""]
lines.append("totals: " + " ".join(f"{k}={counts[k]}" for k in ("PASS","FAIL","XFAIL","XPASS","UNSUPPORTED","UNRESOLVED","ERROR")))
lines.append("")
lines.append("## unexpected (FAIL/XPASS/ERROR)")
lines.extend(fails or ["(none)"])
lines.append("")
lines.append("## UNRESOLVED")
lines.extend(unresolved or ["(none)"])
out.write_text("\n".join(lines) + "\n")
print(out.read_text())
PY

echo "==== new stddef tests in gcc.sum ===="
grep -E "stddef-need-rsize|c23-stddef" /work/build/gcc/testsuite/gcc/gcc.sum /work/build/gcc/testsuite/g++/g++.sum 2>/dev/null || true

echo "==== done $(date -u +%Y-%m-%dT%H:%M:%SZ) ===="
