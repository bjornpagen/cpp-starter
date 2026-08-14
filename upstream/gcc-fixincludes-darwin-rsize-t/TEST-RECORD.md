# Test record: `__need_rsize_t` [PR target/126782]

Base: gcc-mirror `master` `c5d147d7370fb36834c9348c5d3bab229d89fb3e`
Local commit (do not push): `63e3cdeb413866b501b102e5e37afcd6a1f510d7`
Date: 2026-08-13 UTC
Live develop.html as of 2026-08-13 (page last modified 2026-08-07):
GCC 17 Stage 1 started 2026-04-23. Stage 3 starts 2026-11-16.
Do not copy those dates into public mail. Recheck before send.

Post-patch test: the 3-stage bootstrap compared equal. `make -k check`
wrote `evidence/logs/check-summary.txt`.

No pre-patch full testsuite or suitable `gcc-testresults` comparison is
recorded. The public message discloses this deviation.

Patch SHA-256:
`51b34741bdbffdb7c9e81cae509d01461ed9f6982f7e3d0acf9a580f067d05`
(7990 bytes, us-ascii). If this hash changes, request approval again.

## Style (already done)

- `git diff --check`: clean
- `contrib/check_GNU_style.sh`: see `evidence/logs/style-notes.txt`
- `contrib/mklog.py`: ran. Keep the handwritten `stddef.h` ChangeLog line.
- `contrib/gcc-changelog/git_check_commit.py`: OK
- `git apply --check`: clean on parent `c5d147d7370fb36834c9348c5d3bab229d89fb3e`
- Applied tree equals the local commit tree

## Darwin protocol probes

Compiler: gcc 17.0.0 20260810, `aarch64-apple-darwin24`
Log: `evidence/logs/darwin-probes.log`

- Explicit `__need_rsize_t`: fail before the patch, pass after
- Full `<stddef.h>`: no `rsize_t` before or after
- Full include plus `__STDC_WANT_LIB_EXT1__`: no `rsize_t` before or after
- Full include, then `__need_rsize_t`: fail before, pass after
- `rsize.cc` with `g++-17 -std=c++26 -fmodules`: pass
- `__has_feature(modules)` with `-fmodules`: 0

Unpatched probes used `git show HEAD:gcc/ginclude/stddef.h` with `-I`.
The `rsize.cc -fmodules` command is history. It is not the regression test.

## Finch 3-stage bootstrap and `make -k check`

- Finch v1.14.1, image `gcc:16.1.0-trixie`, 10 CPUs, 48 GiB
- Container: `gcc-rsize-bootstrap` (PID 1 is `sleep infinity`)
- Configure: default languages, no `--disable-bootstrap`,
  `--enable-checking=yes --disable-nls --disable-multilib --with-system-zlib`
- Languages: `c,c++,fortran,lto,objc`
- `stage_final`: stage3
- Host, build, and target: `aarch64-unknown-linux-gnu`
- Compiler: gcc 17.0.0 20260813 (experimental), see `evidence/logs/stage3-xgcc-v.txt`
- Stages 2 and 3: `Comparison successful.` (`evidence/logs/bootstrap-markers.txt`)
- `make -k check` started 2026-08-13T14:23:51Z, finished 2026-08-13T19:59:25Z
- Exit status 2 (`evidence/logs/check-exit.txt`), expected with FAILs under `-k`
- Totals: PASS=997628 FAIL=57 XFAIL=5623 XPASS=0 UNSUPPORTED=11149
  UNRESOLVED=7 ERROR=0 (`evidence/logs/check-summary.txt`)

New tests, all PASS:

- `gcc.dg/stddef-need-rsize-1.c`
- `gcc.dg/stddef-need-rsize-2.c`
- `gcc.dg/stddef-need-rsize-3.c`
- `gcc.dg/stddef-need-rsize-4.c`
- `g++.dg/stddef-need-rsize-1.C` (c++98, c++20, c++29)
- existing `gcc.dg/c23-stddef-1.c` and `c23-stddef-2.c`

The 57 DejaGnu FAILs are aarch64 SME/SVE/advsimd, two libgomp C++ compiles,
and libstdc++ filesystem copy tests. None mention stddef or rsize_t.

Two in-tree MPFR long-double tests hung with empty logs at 100% CPU:
`tget_ld_2exp` (~4h54m) and `tset_ld` (~1h). They were SIGTERM'd (exit 143).
See `evidence/logs/mpfr-killed.txt`. They are not GCC DejaGnu tests.

## Checks that did not run

- Pre-patch full testsuite on this host
- Comparison with recent `gcc-testresults` results
- Literal target name `make bootstrap` (used `make -j10` with bootstrap
  enabled; stages 2 and 3 compared equal)
- `make -C gcc -k check-c++-all` (not a C++ front-end change)
- Live Bugzilla re-read this pass (Anubis). Last browser read: 2026-08-13,
  comments 0-5

## Not requested

- GCC 16 backport. This patch is for trunk only. The public mail does
  not ask for a backport.
- Darwin rebuild of this exact trunk revision. Darwin probes used a
  header override.

## Older Linux run (not the policy test)

An earlier `--disable-bootstrap` C/C++ build exists under
`~/.gcc/rsize-linux/`. Do not describe that run as a bootstrap.
