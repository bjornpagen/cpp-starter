# GCC fixincludes: macOS SDK `sys/_types/_rsize_t.h` assumes `__has_feature(modules)` implies clang

- **Where:** gcc-patches@gcc.gnu.org. Send the patch inline or as a
  `text/x-patch` attachment (per https://gcc.gnu.org/contribute.html).
  Optional: CC the fixincludes / Darwin maintainers from MAINTAINERS.
- **Kind:** fixincludes patch. The complete, ready-to-send patch is
  `0001-fixincludes-darwin-sys-_types-_rsize_t.h-breaks-unde.patch`
  (git format-patch; the commit message carries the ChangeLog and the
  DCO Signed-off-by). `inclhack-entry.def` shows only the new hack.
  `fixed-header.h` is the earlier production local fixinclude (a full
  header replacement). The upstream patch instead rewrites the guard;
  see below.
- **Verified against trunk (re-run 2026-08-09 on a clean, freshly
  fetched clone):** The patch applies clean with `git am`, no fuzz, to
  master @ 0621cf67366. origin/master had not advanced past the
  generation base. GCC's own server-side commit checker passes:
  `contrib/gcc-changelog/git_check_commit.py HEAD` → `OK`. Trunk
  `fixincludes/inclhack.def` has no rsize_t/darwin-modules hack (grep
  `rsize`: no matches). The fix is still needed.
- **Verified behavior:** GCC 16.1.0, aarch64-apple-darwin24, macOS 15
  SDK (Xcode). The full matrix is below.
- **Verdict:** SEND

## Legal / mechanics checklist (gcc contribute.html, checked 2026-08-09)

- The commit message carries a DCO `Signed-off-by:` tag. GCC accepts
  DCO certification as an alternative to FSF copyright assignment. A
  fixincludes hack of this size also plausibly counts as a "small
  change". The sign-off makes the legal status unambiguous either way.
- The ChangeLog entries live in the commit message, in the current
  `fixincludes/ChangeLog:` format. We verified the format against
  recent fixincludes commits on master (5d2205b8017, 0a48b1fe986):

  ```
  fixincludes/ChangeLog:

  	* inclhack.def (darwin_rsize_t_modules): New fix.
  	* fixincl.x: Regenerate.
  	* tests/base/sys/_types/_rsize_t.h: New test.
  ```

- Subject without a bug number: `[PATCH] fixincludes: darwin
  sys/_types/_rsize_t.h breaks under GCC -fmodules` (classifier +
  component, under 75 chars). If you file the companion Bugzilla report
  first, use the short subject with ` [PRnnnnn]` appended (no component,
  no space inside the brackets). Then put the full `PR other/nnnnn`
  form in the commit body, so Bugzilla links the post.
- State the testing done in the email. Use the "Testing done" section
  below. Also add one line: "I do not have write access to the GCC
  repository." Both items come from contribute.html.
- Send the mail as plain text. Attach the patch as `text/x-patch`, or
  put it inline. Never send it as application/* or base64. gcc-patches
  accepts posts from unsubscribed addresses, but it runs them through
  spam blocklists. Subscribe first (or use the `global-allow-subscribe`
  mechanism) to avoid a silent drop. The posting address becomes
  permanently public: the archives are never edited.
- If there is no response after about two weeks, send one polite ping
  on the same thread.

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

The guard treats `__has_feature(modules)` as proof of two things: the
compiler is clang, and its `stddef.h` honors the `__need_rsize_t`
protocol. That assumption no longer holds. GCC also reports the
`modules` feature under `-fmodules`. We verified this with GCC 16.1.0:
`__has_feature(modules)` is true with `g++ -std=c++26 -fmodules` and
false without. The feature is registered from `flag_modules` in
`gcc/cp/cp-objcp-common.cc`. But GCC's `stddef.h` does not implement
the clang-only protocol. As a result, `rsize_t` is never defined, and
the C11 Annex K declarations in the SDK's `<_string.h>` fail to parse:

```
_string.h:176:48: error: 'rsize_t' has not been declared; did you mean 'size_t'?
```

The practical impact is large. libstdc++'s `std` module
(`src/c++23/std.cc`) reaches these declarations, so it fails to compile
on darwin under `-fmodules`. The build then installs an empty fallback
and exits 0 (reported separately to libstdc++). The result: `import
std;` is broken for every darwin GCC user.

The fix is a fixincludes hack (`darwin_rsize_t_modules`). It rewrites
the guard to also require `__clang__`. GCC then always takes the SDK's
own plain-typedef branch. The header is unchanged for clang. A `bypass`
on `__clang__` keeps the fix idempotent, and it skips SDKs that already
check for clang. The patch includes a tests/base fixture. fixincl.x is
regenerated with ./genfixes (AutoGen 5.18.16).

Alternatives considered, for the record:

1. GCC's stddef.h could honor the clang-extension `__need_rsize_t`
   protocol member. That fixes the class, not only this instance: any
   header that speaks the protocol, on any target. The cost: a public
   GCC header on every target starts to track a Clang extension, and
   the Annex K question re-opens. If maintainers prefer that direction,
   I am happy to help. The fixincludes hack is wanted either way,
   because SDKs already shipped never change.
2. The root cause is the SDK guard itself: it uses
   `__has_feature(modules)` as a proxy for "is clang". That is Apple's
   to fix. A fixed future SDK does not help any SDK already shipped.

## Testing done (state this in the email; re-verified 2026-08-09 on a clean trunk clone)

- The patch applies clean with `git am` to master @ 0621cf67366. It
  passes `contrib/gcc-changelog/git_check_commit.py` (`OK`).
- Trunk fixincludes builds standalone from the patched tree. fixincl.c
  #includes the patched fixincl.x. It compiles with the Makefile's own
  `-W -Wall -Wwrite-strings ... -pedantic` flags and zero diagnostics.
  FIX_COUNT and REGEX_COUNT are updated as the template computes them.
- fixincludes self-test: we replicated by hand the exact steps that the
  generated check.sh performs for the new fix (TEST_MODE=true,
  TARGET_MACHINE='*', test_text wrapped in
  `DARWIN_RSIZE_T_MODULES_CHECK`). The fixincl output is byte-identical
  to the new `tests/base/sys/_types/_rsize_t.h` fixture.
- Real SDK header (macOS 15 SDK): fixincl with
  TARGET_MACHINE=aarch64-apple-darwin24 rewrites only the guard line.
  A second fixincl run on the fixed header makes no change (the bypass
  works).
- Compile matrix with GCC 16.1.0 on aarch64-apple-darwin24. The test
  program is `#define __STDC_WANT_LIB_EXT1__ 1` + `#include <string.h>`
  + a use of `rsize_t`:
  - unfixed header, `-std=c++26 -fmodules`: fails with the `rsize_t`
    error above (bug reproduced);
  - fixed header, `-std=c++26 -fmodules`: compiles;
  - fixed header, `-std=c++26` (no -fmodules): compiles.
- A full-header-replacement variant of this fix went into production on
  this host's GCC 16.1.0 toolchain on 2026-08-08. The complete
  libstdc++ std module builds and runs with it.

## Style gate (contrib/check_GNU_style.sh, run 2026-08-09)

The script flags five classes of line in this patch. All are
inapplicable. We verified each against the tree's own conventions.
State this only if a reviewer asks:

- Spaces-not-tabs and macro-count lines in `fixincl.x`: this is
  generated-file content that matches autogen's template output. It is
  not hand-styled code.
- `__has_feature(modules)` without the GNU space: this is verbatim SDK
  header text, plus the `c_fix_arg`/`test_text` match strings. Those
  strings must match the real header byte-for-byte.
- `#if defined( DARWIN_RSIZE_T_MODULES_CHECK )` in the test fixture:
  this is the established `tests/base` wrapper convention. The same
  form appears in `assert.h`, `AvailabilityMacros.h`, `time.h`, and
  others.

## Design space (crib sheet for review replies — NOT part of the email)

The email's "Alternatives considered" paragraph is the compressed form.
This is the full analysis. It is kept here so the reasoning is on hand
when a reviewer pushes on it.

- **A. Fix the SDK (Apple).** The root cause: the guard tests
  `__has_feature(modules)` as a proxy for "is clang whose stddef.h
  implements the `__need_rsize_t` protocol". This fix is correct in
  principle. It is not actionable as the primary fix: the SDK is
  closed, Feedback timescales are years, and every already-shipped SDK
  stays broken forever. GCC needs the fixinclude for those SDKs
  regardless. Context for tone: the header's delegation is not
  gratuitous. Under clang modules, one module (the builtin stddef
  module) should own a typedef, so the header defers to clang's
  stddef.h to avoid an ownership conflict. The plain-typedef branch we
  route GCC onto is the branch Apple already maintains for every
  non-clang, non-modules compile.
- **B. The deeper GCC fix: honor `__need_rsize_t` in
  gcc/ginclude/stddef.h.** GCC's stddef.h already implements its own
  old `__need_*` protocol (`__need_size_t`, `__need_wchar_t`, ...).
  Clang extended the family, and `__need_rsize_t` is one of its
  members. Honoring it is about three lines (`rsize_t` is `size_t`,
  C11 K.3.3). It fixes the CLASS — any header that speaks clang's
  protocol, on any target. The fixinclude fixes the INSTANCE. Why B is
  not the lead: it changes a public GCC header on every target to
  track a *clang extension* (a slippery slope — clang has more
  `__need_*` members), and it re-opens the Annex K question (the
  GCC/glibc world has a decade of hostility to `rsize_t`'s family; even
  a typedef-under-explicit-request may draw that fight). Both are
  front-end-maintainer debates with stall risk. fixincludes is the
  purpose-built, darwin-scoped channel with zero blast radius. **If a
  reviewer prefers B, the answer is yes**: offer to implement it as a
  follow-up. Note that the fixinclude stays wanted either way, for
  symmetry with shipped SDKs.
- **C. The fixinclude (this patch).** One predicate corrected to test
  what it always meant (`defined(__clang__) && ...`). Idempotent via
  the `__clang__` bypass. Fixture and regenerated table included. This
  is not "ifdef hell": no conditional is added. The existing one is
  made truthful, and the repair is quarantined at install time instead
  of leaking into code.
- **D. Stop reporting `__has_feature(modules)`.** This lies about a
  real capability to dodge one broken header. It breaks every
  legitimate feature test. Disqualified.
- **E. Patch libstdc++ (`std.cc`) around it.** This fixes one consumer.
  Every other `<string.h>`-under-modules user stays broken. It is the
  wrong layer — the symptom's surface, not the wrong thing itself.

## genfixes gate: CLEARED (2026-08-09)

We ran `./genfixes` with AutoGen 5.18.16 (vanilla gcc:16.1.0 container,
over the patched tree). The regenerated `fixincl.x` matched the
hand-written hunk byte-for-byte, except the dated "AutoGen-ed ..."
header. We adopted the regenerated file into the patch (amended commit
`64ba9daf`; `git_check_commit.py` re-passed `OK`). The hand-written
caveat is gone from the body above.

## fixincludes `make check` gate: CLEARED (2026-08-09)

The real `make check` ran with autogen present (Linux CI, over the
patched trunk) and passed. This supersedes the earlier manual
replication of check.sh's steps. The manual replication stays recorded
above as the darwin-side verification.

## Bootstrap + regtest gate: CLEARED (2026-08-10)

Full three-stage bootstrap of the patched trunk, **all default
languages**, on aarch64-unknown-linux-gnu (Debian trixie, Apple Silicon
host). A native darwin trunk bootstrap is impossible: upstream has no
aarch64-darwin target; the port is out of tree. `make -j10` completed
clean. `make -k check -j10` totals:

| suite | passes | FAIL |
|---|---|---|
| gcc | 407,351 | 42 |
| g++ | 472,371 | 16 |
| gfortran | 75,971 | 9 |
| objc | 2,849 | 0 |
| libstdc++ | 18,896 | 7 |
| libgomp | 17,866 | 5 |
| libitm / libatomic | 44 / 54 | 0 |

Every failure is environmental or pre-existing trunk noise. None is
patch-related. The 7 libstdc++ FAILs are filesystem
`copy`/`last_write_time` execution tests on a virtiofs mount. The 5
libgomp FAILs are load-induced compile timeouts. The remainder is
current trunk noise. The patch cannot affect Linux results by
construction: the hack is mach-gated to `*-*-darwin*` and inert
elsewhere. The tests that exercise the change are the fixincludes
testsuite (`make check` with autogen: PASS) and the darwin SDK matrix
above. The `.sum` files and `test_summary` are kept at
`~/Documents/gcc-bootstrap-linux/`.

**Testing statement for the email:** "Bootstrapped (all default
languages) and regression-tested with make -k check on
aarch64-unknown-linux-gnu. Observed failures are pre-existing or
environment noise; none relates to this change (the hack is
darwin-gated). The fixincludes testsuite passes with the new hack
exercised. The fixed header was verified against the real macOS 15 SDK
on aarch64-apple-darwin24 (matrix above)."
