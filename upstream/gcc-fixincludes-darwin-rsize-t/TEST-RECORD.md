# Test record: `__need_rsize_t` [PR target/126782]

Base: gcc-mirror `master` `c5d147d7370fb36834c9348c5d3bab229d89fb3e`
Date: 2026-08-13 UTC
GCC development stage: 17 Stage 1. Stage 3 starts 2026-11-16.

The policy test is a 3-stage bootstrap plus `make -k check` on Finch.
That job still runs. Do not call the packet bootstrapped until that job
prints `bootstrap finished`.

Current source-diff SHA-256:
`1f154efcfcba938e938b3c06256bf9cbc2893209d1e54ccd329a251d4a09f621`.

## Already done

Style:

- `git diff --check`: clean
- `contrib/check_GNU_style.sh`: see `evidence/logs/style-notes.txt`
- `contrib/mklog.py`: ran. Keep the handwritten `stddef.h` ChangeLog line.
- `git apply --check`: clean on HEAD `c5d147d7370fb36834c9348c5d3bab229d89fb3e`

Darwin protocol probes (`evidence/logs/darwin-probes.log`):

- Compiler: gcc 17.0.0 20260810, `aarch64-apple-darwin24`
- Explicit `__need_rsize_t`: fail before the patch, pass after
- Full `<stddef.h>`: no `rsize_t` before or after
- Full include plus `__STDC_WANT_LIB_EXT1__`: no `rsize_t` before or after
- Full include, then `__need_rsize_t`: fail before, pass after
- `rsize.cc` with `g++-17 -std=c++26 -fmodules`: pass
- `__has_feature(modules)` with `-fmodules`: 0

Unpatched probes used `git show HEAD:gcc/ginclude/stddef.h` with `-I`.

The `rsize.cc -fmodules` command is history. It is not the regression test.

## Finch job (still runs)

- Finch v1.14.1, image `gcc:16.1.0-trixie`, 10 CPUs, 48 GiB
- Container: `gcc-rsize-bootstrap` (PID 1 is `sleep infinity`)
- Configure: default languages, no `--disable-bootstrap`,
  `--enable-checking=yes --disable-nls --disable-multilib --with-system-zlib`
- Languages: `c,c++,fortran,lto,objc`
- `stage_final`: stage3
- Host, build, and target: `aarch64-unknown-linux-gnu`

## Not requested

- GCC 16 backport. This patch is for trunk only.
- Darwin rebuild of this exact trunk revision. Darwin probes used a
  header override.

## Older Linux run (not the policy test)

An earlier `--disable-bootstrap` C/C++ build exists under
`~/.gcc/rsize-linux/`. Do not describe that run as a bootstrap.
