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
- **Verified against trunk (re-run 2026-08-09 on a clean, freshly
  fetched clone):** `git am` applies clean, no fuzz, to master
  @ 0621cf67366 (origin/master had not advanced past the generation
  base). GCC's own server-side commit checker passes:
  `contrib/gcc-changelog/git_check_commit.py HEAD` → `OK`. Trunk
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
  under GCC -fmodules` (classifier + component, < 75 chars). If the
  companion Bugzilla PR is filed first, append ` [PRnnnnn]` (no
  component, no space inside the brackets) and put the full
  `PR other/nnnnn` form in the commit body so Bugzilla links the post.
- The email must state testing done — use the "Testing done" section
  below — and note lack of write access ("I do not have write access to
  the GCC repository."), per contribute.html.
- Send as plain text; the patch inline or as a `text/x-patch`
  attachment, never application/* or base64. gcc-patches accepts
  posts from unsubscribed addresses but runs them through spam
  blocklists — subscribing first (or the `global-allow-subscribe`
  mechanism) avoids a silent drop. The posting address becomes
  permanently public: the archives are never edited.
- No response after ~two weeks: one polite ping on the same thread.

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

## Testing done (state this in the email; re-verified 2026-08-09 on a clean trunk clone)

- Applies clean with `git am` to master @ 0621cf67366; passes
  `contrib/gcc-changelog/git_check_commit.py` (`OK`).
- Trunk fixincludes builds standalone from the patched tree: fixincl.c,
  which #includes the patched fixincl.x, compiles with the Makefile's
  own `-W -Wall -Wwrite-strings ... -pedantic` with zero diagnostics
  (FIX_COUNT/REGEX_COUNT updated as the template computes them).
- fixincludes self-test: the exact steps `make check`'s generated
  check.sh performs for the new fix were replicated manually
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

## Style gate (contrib/check_GNU_style.sh, run 2026-08-09)

The script flags five classes of line in this patch; all are
inapplicable, verified against the tree's own conventions — state this
only if a reviewer asks:

- spaces-not-tabs and macro-count lines in `fixincl.x`: generated-file
  content matching autogen's template output, not hand-styled code;
- `__has_feature(modules)` without the GNU space: verbatim SDK header
  text and the `c_fix_arg`/`test_text` match strings, which must match
  the real header byte-for-byte;
- `#if defined( DARWIN_RSIZE_T_MODULES_CHECK )` in the test fixture:
  the established `tests/base` wrapper convention (same form in
  `assert.h`, `AvailabilityMacros.h`, `time.h`, ...).

## Still to run before sending (contribute.html's testing bar)

Policy: a change outside the front ends requires a complete build —
"bootstrap all default languages, not just C and C++, and run all
testsuites" — with results compared against pre-patch or gcc-testresults
postings, and the email's testing statement must say so. Two steps
remain, both in motion:

1. **Full bootstrap of the patched trunk** (default languages, darwin
   arm64) — running in `~/Documents/gcc-verify-build` (branch
   `verify-rsize-t` = trunk + exactly this patch). When it finishes:
   `make -k check` (requires DejaGnu), compare against current
   aarch64-apple-darwin gcc-testresults postings, then replace this
   section with one line: "Bootstrapped and regression-tested on
   aarch64-apple-darwin24 (all default languages); test results compared
   against <baseline>."
2. **`./genfixes` regeneration** (requires autogen): regenerate
   fixincl.x from the patched inclhack.def, diff against the
   hand-written hunk. If identical, delete the "Note on fixincl.x"
   caveat paragraph from the body; if it differs, take the regenerated
   version into the patch and re-run the self-test.
