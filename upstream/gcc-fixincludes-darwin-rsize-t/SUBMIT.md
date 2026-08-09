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

fixincl.x regenerated with ./genfixes (AutoGen 5.18.16); tests/base
fixture included.

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

## genfixes gate: CLEARED (2026-08-09)

`./genfixes` run with AutoGen 5.18.16 (vanilla gcc:16.1.0 container over
the patched tree): the regenerated `fixincl.x` matched the hand-written
hunk byte-for-byte except the dated "AutoGen-ed ..." header. The
regenerated file was adopted into the patch (amended commit `64ba9daf`,
`git_check_commit.py` re-passed `OK`); the hand-written caveat is gone
from the body above.

## fixincludes `make check` gate: CLEARED (2026-08-09)

The real `make check` (autogen present, Linux CI over the patched
trunk): passed — this supersedes the earlier manual replication of
check.sh's steps, which remains recorded above as the darwin-side
verification.

## Still to run before sending (contribute.html's testing bar)

One step remains, in motion: **full bootstrap + regression test of the
patched trunk** — all default languages plus `make -k check`, running in
CI on aarch64-linux-gnu (a native darwin trunk bootstrap is impossible:
upstream has no aarch64-darwin target; the port is out of tree). When
the artifact lands, replace this section with one line: "Bootstrapped
(all default languages) and regression-tested with make -k check on
aarch64-linux-gnu; test summary attached / compared against
gcc-testresults."
