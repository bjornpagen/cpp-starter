# Test record: `__need_rsize_t` [PR target/126782]

Base: gcc-mirror `master` `c5d147d7370fb36834c9348c5d3bab229d89fb3e`
Local commit (do not push): `63e3cdeb413866b501b102e5e37afcd6a1f510d7`
Date: 2026-08-13 UTC

Submitted 2026-08-14:
https://inbox.sourceware.org/gcc-patches/20260814032423.29082-1-hello@bjornpagen.com/

Post-patch test: the 3-stage bootstrap compared equal. `make -k check`
wrote `evidence/logs/check-summary.txt`.

No pre-patch full testsuite or suitable `gcc-testresults` comparison is
recorded. The public message discloses this deviation.

Patch SHA-256:
`5d8a6a60cec5186c740246b94bc4fcd48e4d227b2b8bce603388b917457fa4a0`
(7861 bytes, us-ascii).

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

## Checks that did not run

- Pre-patch full testsuite on this host
- Comparison with recent `gcc-testresults` results
- Literal target name `make bootstrap` (used `make -j10` with bootstrap
  enabled; stages 2 and 3 compared equal)
- `make -C gcc -k check-c++-all` (not a C++ front-end change)

## Not requested

- GCC 16 backport. This patch is for trunk only. The public mail does
  not ask for a backport.
- Darwin rebuild of this exact trunk revision. Darwin probes used a
  header override.

## Older Linux run (not the policy test)

An earlier `--disable-bootstrap` C/C++ build exists under
`~/.gcc/rsize-linux/`. Do not describe that run as a bootstrap.
