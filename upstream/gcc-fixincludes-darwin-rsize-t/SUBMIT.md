# Work plan: `__need_rsize_t` support in GCC `<stddef.h>`

We filed the Bugzilla report as PR target/126782. The maintainer prefers a fix in
GCC's `<stddef.h>` over fixincludes. This file is the researched design for that
patch. The patch is not ready to send.

## Context from the upstream research (2026-08-11)

1. PR 116827 is the precedent. Its fix (commit r15-9499-g9cf6b52d04df2272, backported
   to gcc-14 as r14-11713) touched only `gcc/ginclude/stddef.h`. It undefines the
   SDK's includer-side guards (`__PTRDIFF_T`, `__SIZE_T`) inside an `__APPLE__` block.
   It deliberately did not implement `__need_rsize_t`. It added no testcase. The
   review was one style comment (Rainer Orth: add an explanatory comment); the author
   then self-applied it as Darwin-local.
2. Trunk no longer claims `__has_feature(modules)`. Commit 08ede4fbbe6d38e0
   (2026-08-04, approved by Jason Merrill) removed the `modules` entry from
   `cp_feature_table`, because the feature test is Clang-specific and the macOS SDK
   uses it to mean "is Clang". On trunk, the SDK takes its plain-typedef branch and
   the original symptom is gone. The `releases/gcc-16` branch still claims the
   feature. We do not request a backport. The first patch must stand on support
   for the explicit `__need_rsize_t` protocol. Do not present it as a trunk fix
   for the original Darwin symptom.
3. The SDK guard family: `rsize_t` is the only missing typedef in GCC's `__need_*`
   protocol. The SDK also requests `__need_offsetof` (GCC has no such protocol) and
   `__need_va_list` (GCC's `stdarg.h` only knows the spelling `__need___va_list`).
   Mention both in the submission as known adjacent gaps; do not patch them here.
4. One sharp SDK detail for the report: `sys/types.h:177` includes `_rsize_t.h`
   without any Annex K gate. Under a modules-claiming GCC, that include sets the
   SDK guard `__RSIZE_T` permanently, so a later `__STDC_WANT_LIB_EXT1__` opt-in
   can never recover the typedef in that translation unit.
5. Clang's reference implementation (`__stddef_rsize_t.h`): an explicit
   `__need_rsize_t` works unconditionally; the `__STDC_WANT_LIB_EXT1__ >= 1` gate
   applies only to full inclusion; the guard macro is `_RSIZE_T`; the typedef is
   `typedef __SIZE_TYPE__ rsize_t;`. Clang defines no `RSIZE_MAX`, no Annex K
   functions, and no `__STDC_LIB_EXT1__`.
6. glibc and musl define `rsize_t` nowhere (both rejected Annex K; see WG14 N1967).
   The typedef appears only on request, so the patch changes nothing for them.

## Proposed first patch

Touch only `gcc/ginclude/stddef.h` plus tests. Do not touch fixincludes, the Darwin
driver, or `stdint-gcc.h` (`RSIZE_MAX` belongs to the C library; the SDK's
`stdint.h:176` already defines it). Do not define `__STDC_LIB_EXT1__`.

1. Add `|| defined(__need_rsize_t)` to the entry gate (the `__need_*` list at the
   top of the file). Add `!defined(__need_rsize_t)` to the "whole job" check, so a
   partial request does not set `_STDDEF_H`.
2. Insert a new per-type section directly after the `size_t` section, in the file's
   exact idiom. This is a design sketch. Check current trunk before using it:

```c
/* Define the restricted-size type when a system header requests it.  */
#if defined (__need_rsize_t)
#ifndef _RSIZE_T	/* in case the OS headers have defined it.  */
#define _RSIZE_T
typedef __SIZE_TYPE__ rsize_t;
#endif /* _RSIZE_T */
#undef __need_rsize_t
#endif /* __need_rsize_t */
```

3. The guard choice is `_RSIZE_T`, deliberately: it matches Clang's helper and the
   SDK's non-modular branch (no double typedef either way). The SDK's modular
   includer-side guard is the different spelling `__RSIZE_T`, so this section needs
   no PR-116827-style undef in the `__APPLE__` block.
4. Reviewers expect a clear explanatory comment (the one review comment on the
   PR 116827 patch asked for exactly that).
5. Do not add the full-inclusion `__STDC_WANT_LIB_EXT1__` path in the first patch.
   That is a separate conformance question with a larger namespace effect.

## Deferred design question

Clang also defines `rsize_t` during a full `<stddef.h>` inclusion when
`__STDC_WANT_LIB_EXT1__ >= 1`. That behavior might be a useful follow-up. It is
not required to support a system header that explicitly defines
`__need_rsize_t`. Research and review it separately. Do not expand the first
patch merely to match all Clang behavior.

## Testsuite shape (target-independent; no SDK dependence)

1. `gcc.dg/stddef-need-rsize-1.c` — `#define __need_rsize_t` then include: `rsize_t`
   is usable and compatible with `__SIZE_TYPE__`; `_STDDEF_H` is not defined; a use
   of `size_t` gets `dg-error` (the partial include did not do the whole job).
2. `gcc.dg/stddef-need-rsize-2.c` — full include first, then the need-macro dance
   again (models the SDK's `sys/types.h` ordering): `rsize_t` must still appear.
3. A repeated-request or include-order test that follows current `<stddef.h>`
   testsuite conventions.
4. A `g++.dg` copy only if current testsuite practice requires separate C++
   coverage.
5. Do not add a full-inclusion Annex K test unless the patch also adds that
   behavior.

## Known review risks

- "Why, now that trunk does not claim `feature(modules)`?" — answer: system
  headers use GCC's selective `<stddef.h>` protocol directly. The patch adds one
  missing explicit request. It does not restore the obsolete trunk trigger.
- Scope objection: keep the patch limited to an explicit request. Do not rely on
  broad Annex K conformance or namespace arguments.
- This file is generic, not Darwin-local, so it needs a global reviewer (Jason
  Merrill and Joseph Myers are the natural choices). The PR 116827 patch waited 2.5
  weeks with two pings; plan for that.

## Send checklist (when the patch exists)

1. Make sure the subject line contains `[PR target/126782]`. Make sure the ChangeLog
   contains `PR target/126782`.
2. Confirm the legal contribution route before adding any `Signed-off-by` line.
3. Run the GCC commit checker. Run the style checker.
4. Apply the patch to a clean current-master worktree.
5. Send the patch to `gcc-patches@gcc.gnu.org` as plain text or a `text/x-patch`
   attachment. State that you do not have GCC write access.

## Reading list before sending

- PR 116827 (precedent; open, assigned; Wakely and Sandoe design opinions) and
  PR 126782 (our report; comments 3-5 are the mandate). The Bugzilla HTML UI blocks
  tools; the REST API works: `https://gcc.gnu.org/bugzilla/rest/bug/<id>/comment`.
- PR 126786 — our libstdc++ report on the silent empty-module fallback; same
  root-cause family as the "empty std.cc" motivation in commit 08ede4fb.
- PR c/60512 — where `__has_feature` and the original `modules` entry landed in
  GCC 14.
- gcc-patches threads: the PR 116827 patch and pings
  (`gcc.gnu.org/pipermail/gcc-patches/2025-March/678908.html`); the
  feature(modules) removal and its approval
  (`gcc.gnu.org/pipermail/gcc-patches/2026-August/726278.html`, `.../726363.html`).
- FB15255066 (`gist.github.com/fxcoudert/1e3ed3470febf220a392152189c143a3`) — the
  Apple report that `defined(__has_feature) && __has_feature(modules)` is itself a
  bug in the SDK guard.
- Clang's stddef split (Ian Anderson, D159383 era) — background on why the SDK does
  includer-side guarding.
- WG14 N1967 "Field Experience With Annex K" — the citation for keeping the scope
  typedef-only.
