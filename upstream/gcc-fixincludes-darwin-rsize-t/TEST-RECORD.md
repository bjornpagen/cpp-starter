# Test record: `__need_rsize_t` [PR target/126782]

Base: gcc-mirror `master` `c5d147d7370fb36834c9348c5d3bab229d89fb3e`
Date: 2026-08-13 UTC
GCC stage: 17 Stage 1 (Stage 3 starts 2026-11-16)

This is a `--disable-bootstrap` C/C++ build of patched trunk on Linux,
plus header-override protocol probes on Darwin and Linux.  It is not a
3-stage bootstrap.  Do not describe it as a bootstrap.

Patch SHA-256: `b56ad4476a7587e789c1276f915041c2690be3ae0afb3b8dbad95b739c362536`

## Style

- `git diff --check`: clean
- `contrib/check_GNU_style.sh`: flags `defined(__need_rsize_t)` on the
  existing `__need_*` gate lines (those lines already use
  `defined(__need_wint_t)` with no space) and `#endif /* _RSIZE_T */`
  (matches `#endif /* _SIZE_T */` in the same file)
- `contrib/mklog.py`: not run (host Python lacks `requests` / `unidiff`).
  ChangeLog text is in `COMMIT-MESSAGE.txt`

## Protocol matrix

Explicit `__need_rsize_t` fails before the patch and passes after.
Ordinary `<stddef.h>`, with or without `__STDC_WANT_LIB_EXT1__`, does
not expose `rsize_t` either before or after.

| Probe | Unpatched | Patched |
|---|---|---|
| Linux gcc 16.1.0, `__need_rsize_t` (C and C++) | fail | pass |
| Linux gcc 16.1.0, full include | no type | no type |
| Linux gcc 16.1.0, full include + `__STDC_WANT_LIB_EXT1__` | no type | no type |
| Linux gcc 16.1.0, full then `__need_rsize_t` | fail | pass |
| Darwin gcc 17.0.0 20260810, same four rows | same | same |
| Linux trunk `xgcc` 17.0.0 20260813, same four rows | — | same as patched |

Unpatched probes used `git show HEAD:gcc/ginclude/stddef.h` via `-I`.

`rsize.cc` with `g++-17 -std=c++26 -fmodules` already passes on Darwin
GCC 17, patched or unpatched: `__has_feature(modules)` is 0.  That
command is historical motivation, not the regression test.

## Linux trunk build

- Finch v1.14.1, image `gcc:16.1.0-trixie`, aarch64
- Configure: `--enable-languages=c,c++ --disable-nls --enable-checking=release --disable-bootstrap --disable-multilib --with-system-zlib`
- `xgcc` 17.0.0 20260813 (experimental), `aarch64-unknown-linux-gnu`

DejaGNU (`runtest --tool gcc dg.exp=stddef-need-rsize-N.c`):

- `stddef-need-rsize-1.c` PASS (error line 35) + PASS (excess errors)
- `stddef-need-rsize-2.c` PASS
- `stddef-need-rsize-3.c` PASS (error line 7) + PASS (excess errors)
- `stddef-need-rsize-4.c` PASS (error line 9) + PASS (excess errors)
- `c23-stddef-1.c` PASS
- `c23-stddef-2.c` PASS

## Unrun

- 3-stage bootstrap
- Full `make -k check`
- GCC 16 backport
- Darwin rebuild of this exact trunk revision
