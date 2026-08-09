# GCC fixincludes: macOS SDK `sys/_types/_rsize_t.h` assumes `__has_feature(modules)` implies clang

- **Where:** gcc-patches@gcc.gnu.org (patch inline or as `text/x-patch`
  attachment, per https://gcc.gnu.org/contribute.html). CC fixincludes /
  Darwin maintainers per MAINTAINERS if desired.
- **Kind:** fixincludes patch. The complete, ready-to-send patch is
  `0001-fixincludes-darwin-sys-_types-_rsize_t.h-breaks-unde.patch`
  (git format-patch, commit-message ChangeLog + DCO Signed-off-by).
  `inclhack-entry.def` shows just the new hack; `fixed-header.h` is the
  earlier production local fixinclude (full header replacement) — the
  upstream patch instead rewrites the guard, see below.
- **Verified against trunk:** patch generated on and applies cleanly
  (`git apply --check`) to master @ 0621cf67366 (2026-08-09). Trunk
  `fixincludes/inclhack.def` has no rsize_t/darwin-modules hack (grep
  `rsize` — no matches), so the fix is still needed.
- **Verified behavior:** GCC 16.1.0 aarch64-apple-darwin24, macOS 15
  SDK (Xcode). Full matrix below.
- **Verdict:** SEND

## Legal / mechanics checklist (gcc contribute.html, checked 2026-08-09)

- DCO `Signed-off-by:` in the commit message (GCC accepts DCO
  certification as an alternative to FSF copyright assignment; a
  fixincludes hack of this size also plausibly falls under "small
  change", but the sign-off makes it unambiguous).
- ChangeLog entries live in the commit message in the current
  `fixincludes/ChangeLog:` format (verified against recent fixincludes
  commits on master, e.g. 5d2205b8017 and 0a48b1fe986):

  ```
  fixincludes/ChangeLog:

  	* inclhack.def (darwin_rsize_t_modules): New fix.
  	* fixincl.x: Regenerate.
  	* tests/base/sys/_types/_rsize_t.h: New test.
  ```

- Subject: `[PATCH] fixincludes: darwin sys/_types/_rsize_t.h breaks
  under GCC -fmodules` (classifier + component, < 75 chars).
- The email must state testing done — use the "Testing done" section
  below, including the autogen caveat.

## Title

```
fixincludes: darwin sys/_types/_rsize_t.h breaks under GCC -fmodules
```

## Body (paste)

The macOS SDK's `sys/_types/_rsize_t.h` reads:

```c
#if defined(__has_feature) && __has_feature(modules)
#define USE_CLANG_STDDEF 1
#else
#define USE_CLANG_STDDEF 0
#endif

#if USE_CLANG_STDDEF
...
#define __need_rsize_t
#include <stddef.h>
```

i.e. it treats `__has_feature(modules)` as "this compiler is clang and its
`stddef.h` honors the `__need_rsize_t` protocol". GCC also reports the
modules feature under `-fmodules` (verified with GCC 16.1.0:
`__has_feature(modules)` evaluates true with `g++ -std=c++26 -fmodules`,
false without; the feature is registered from `flag_modules` in
`gcc/cp/cp-objcp-common.cc`), but GCC's `stddef.h` does not implement
that clang-only protocol, so `rsize_t` is never defined and the C11
Annex K declarations in the SDK's `<_string.h>` fail to parse:

```
_string.h:176:48: error: 'rsize_t' has not been declared; did you mean 'size_t'?
```

The practical blast radius is large: libstdc++'s `std` module
(`src/c++23/std.cc`, which reaches these declarations) fails to compile on
darwin under `-fmodules`, and the build then installs an empty fallback
with exit 0 (reported separately to libstdc++), leaving `import std;`
broken for every darwin GCC user.

The fix is a fixincludes hack (`darwin_rsize_t_modules`) that rewrites the
guard to additionally require `__clang__`, so GCC always takes the SDK's
own plain-typedef branch; the header is unchanged for clang. A `bypass`
on `__clang__` keeps the fix idempotent and skips SDKs that already check
for clang. tests/base fixture included; fixincl.x regenerated.

Note on fixincl.x: autogen is not available on the build machine, so the
fixincl.x hunk was written by hand to match the autogen template output
exactly (verified by building fixincl from the patched tree and running
the tests below); feel free to re-run ./genfixes when applying.

## Testing done (state this in the email)

- Trunk fixincludes builds standalone from the patched tree
  (hand-regenerated fixincl.x compiles clean; FIX_COUNT/REGEX_COUNT
  updated as the template computes them).
- fixincludes self-test: autogen (needed by `make check` to generate
  check.sh from check.tpl) is unavailable on this machine, so the exact
  steps check.sh performs for the new fix were replicated manually
  (TEST_MODE=true, TARGET_MACHINE='*', test_text wrapped in
  `DARWIN_RSIZE_T_MODULES_CHECK`): fixincl output is byte-identical to
  the new `tests/base/sys/_types/_rsize_t.h` fixture.
- Real SDK header (macOS 15 SDK): fixincl with
  TARGET_MACHINE=aarch64-apple-darwin24 rewrites only the guard line;
  re-running fixincl on the fixed header makes no change (bypass works).
- Compile matrix with GCC 16.1.0 on aarch64-apple-darwin24, test program
  `#define __STDC_WANT_LIB_EXT1__ 1` + `#include <string.h>` + use of
  `rsize_t`:
  - unfixed header, `-std=c++26 -fmodules`: fails with the `rsize_t`
    error above (bug reproduced);
  - fixed header, `-std=c++26 -fmodules`: compiles;
  - fixed header, `-std=c++26` (no -fmodules): compiles.
- A full-header-replacement variant of this fix has been in production
  use on this host's GCC 16.1.0 toolchain since 2026-08-08 with the
  complete libstdc++ std module building and running.
