# Test record: `__need_rsize_t` [PR target/126782]

Base revision: `c5d147d7370fb36834c9348c5d3bab229d89fb3e` (gcc-mirror master)
Recorded: 2026-08-13 UTC
GCC development stage: GCC 17 Stage 1 (Stage 3 starts 2026-11-16)

This is a `--disable-bootstrap` C/C++ build of patched trunk on Linux,
plus header-override protocol probes on Darwin and Linux.  It is not a
3-stage bootstrap.  Do not describe it as a bootstrap.

## Patch bytes

- File: `need-rsize-tested.diff`
- SHA-256: `b56ad4476a7587e789c1276f915041c2690be3ae0afb3b8dbad95b739c362536`

Regenerate the hash if `stddef.h` or the tests change after this line.

## Style checks

- `git diff --check`: clean
- `contrib/check_GNU_style.sh`: two classes of noise
  1. `defined(__need_rsize_t)` on the existing `__need_*` gate lines.
     Those lines already use `defined(__need_wint_t)` with no space.
     Matching the adjacent token is required.
  2. `#endif /* _RSIZE_T */` lacks the two-space sentence terminator.
     That matches `#endif /* _SIZE_T */` in the same file.
- `contrib/mklog.py`: not run.  Host Python lacks `requests` and `unidiff`.
  ChangeLog text was written by hand in `COMMIT-MESSAGE.txt`.

## Protocol matrix

Expected: explicit `__need_rsize_t` fails before the patch and passes
after it.  Ordinary `<stddef.h>` inclusion, with or without
`__STDC_WANT_LIB_EXT1__`, must not expose `rsize_t` either before or
after.

| Probe | Unpatched | Patched |
|---|---|---|
| Linux gcc 16.1.0, `__need_rsize_t` (C) | fail | pass |
| Linux gcc 16.1.0, `__need_rsize_t` (C++) | fail | pass |
| Linux gcc 16.1.0, full include | fail (no type) | fail (no type) |
| Linux gcc 16.1.0, full include + `__STDC_WANT_LIB_EXT1__` | fail (no type) | fail (no type) |
| Linux gcc 16.1.0, full then `__need_rsize_t` | fail | pass |
| Darwin gcc 17.0.0 20260810, `__need_rsize_t` (C) | fail | pass |
| Darwin gcc 17.0.0 20260810, `__need_rsize_t` (C++) | fail | pass |
| Darwin gcc 17.0.0, full include | fail (no type) | fail (no type) |
| Darwin gcc 17.0.0, full include + `__STDC_WANT_LIB_EXT1__` | fail (no type) | fail (no type) |
| Darwin gcc 17.0.0, full then `__need_rsize_t` | fail | pass |

Unpatched probes used `git show HEAD:gcc/ginclude/stddef.h` via `-I`.
Patched probes used the tested `stddef.h` via `-I`.

## Original Darwin source

`rsize.cc` with `g++-17 -std=c++26 -fmodules` passes on this Darwin GCC
17, patched or unpatched.  `__has_feature(modules)` is 0, so the SDK
takes the plain typedef branch.  That command is historical motivation,
not the regression test.

## New test files compiled against the patched header (Darwin gcc-17)

- `stddef-need-rsize-1.c`: exit 1, `unknown type name 'size_t'` on the
  intended `dg-error` line.  `rsize_t` uses compiled.
- `stddef-need-rsize-2.c`: exit 0
- `stddef-need-rsize-3.c`: exit 1, `unknown type name 'rsize_t'`
- `stddef-need-rsize-4.c`: exit 1, `unknown type name 'rsize_t'`

## Linux patched trunk build

- Image: `gcc:16.1.0-trixie` via Finch v1.14.1
- Host bind: `/Users/bjorn/.gcc/src/gcc` -> `/src`
- Build dir: `/Users/bjorn/.gcc/rsize-linux/build`
- Configure: `--enable-languages=c,c++ --disable-nls --enable-checking=release --disable-bootstrap --disable-multilib --with-system-zlib`
- Log: `/Users/bjorn/.gcc/rsize-linux/logs/linux-build.log`

Results for xgcc / installed gcc / `make check-gcc` are filled in after
that build finishes.
