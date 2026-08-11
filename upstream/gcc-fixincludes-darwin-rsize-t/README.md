# gcc-fixincludes-darwin-rsize-t — filed as PR 126782; patch strategy changed

## Status

- We filed the Bugzilla report as [PR target/126782](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126782) on 2026-08-10. The "See Also" field points to PR 116827, which reports the same SDK assumption for `ptrdiff_t` and `size_t`.
- Maintainer feedback from Andrew Pinski (comments 3–4) changed the plan:
  1. The maintainer discourages fixincludes rules on Darwin. A rule binds to one SDK, but users select a different SDK at build time.
  2. The maintainer prefers support for `__need_rsize_t` in GCC's own `<stddef.h>`.
- The reporter agreed in comment 5. GCC's `<stddef.h>` already supports the selective-inclusion protocol for `size_t`, `ptrdiff_t`, and `wchar_t`. The header must also answer `__need_rsize_t` with `typedef __SIZE_TYPE__ rsize_t;`. A full include of `<stddef.h>` can gate `rsize_t` on `__STDC_WANT_LIB_EXT1__ >= 1`. This gate matches Clang.
- The fixincludes patch in this directory does not go to gcc-patches in its current form. The patch stays here as a reference and as the source of the local workaround header.

## Next work

1. Write a new patch against `gcc/ginclude/stddef.h`. The patch must recognize `__need_rsize_t`, define `rsize_t`, and keep the `__STDC_WANT_LIB_EXT1__` gate for full inclusion.
2. Add a testcase that compiles the minimal source from the PR with `-fmodules` against a Darwin SDK shape.
3. Send the patch to gcc-patches with `PR target/126782` in the subject line and in the ChangeLog.
4. File the Apple report (`APPLE-FEEDBACK.md`) after the fix direction for PR 126782 is settled. The report says that the SDK guard assumes that `__has_feature(modules)` implies Clang.

## Files

- `fixed-header.h` — the live local workaround. Every toolchain rebuild copies this file into GCC's `include-fixed/sys/_types/`. See the `macos-rsize-t-fixinclude` entry in `PINS.md`. This file stays until the upstream fix is in GCC. The patch strategy does not change this.
- `0001-fixincludes-Fix-rsize_t-with-Darwin-modules.patch` — the superseded fixincludes patch. This file is a reference only.
- `inclhack-entry.def` — the fixincludes rule from the superseded patch. This file is a reference only.
- `rsize.cc`, `rsize.ii` — the minimal source and the preprocessed output. We attached these files to the PR.
- `SUBMIT.md` — the work plan for the replacement patch.
- `APPLE-FEEDBACK.md` — the pending Apple report about the SDK guard.
