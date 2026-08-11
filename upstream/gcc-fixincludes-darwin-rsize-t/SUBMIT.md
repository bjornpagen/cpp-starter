# Work plan: `__need_rsize_t` support in GCC `<stddef.h>`

We filed the Bugzilla report as PR target/126782. The original fixincludes submission
plan is obsolete. In feedback on the PR, the maintainer prefers a fix in GCC's `<stddef.h>`.
This file is the plan for that replacement patch. The patch is not ready to send.

## Design

1. In `gcc/ginclude/stddef.h`, add the `__need_rsize_t` branch next to the existing
   `__need_size_t` handling. When a header defines `__need_rsize_t` and includes
   `<stddef.h>`, the branch must define `typedef __SIZE_TYPE__ rsize_t;`. The branch
   must then undefine the request macro. Follow the exact idiom of the
   `__need_size_t` block.
2. For a full inclusion (no `__need_*` macro is defined), define `rsize_t` only when
   `__STDC_WANT_LIB_EXT1__ >= 1`. This behavior matches Clang and C11 Annex K.
3. Do not implement any other part of Annex K. The scope is one typedef.

## Open questions to resolve before sending

- Guard interaction: make sure that no double definition occurs when the SDK's plain
  typedef branch and the new stddef.h branch are both active in one translation unit.
- C front end: decide whether the same support applies to `gcc/ginclude/stddef.h`
  for C (`gcc` driver) or only for C++. Clang applies the support to both languages.
- Testsuite location: decide where to put two tests. One test is Darwin-conditional
  and uses the same `__need_rsize_t` + include pattern as the SDK. One test is
  target-independent and checks the `__STDC_WANT_LIB_EXT1__` gate.

## Send checklist (when the patch exists)

1. Make sure that the subject contains `[PR target/126782]`. Make sure that the
   ChangeLog contains `PR target/126782`.
2. Keep the `Signed-off-by` line (DCO).
3. Run the GCC commit checker. Run the style checker.
4. Apply the patch to a clean current-master worktree.
5. Send the patch to `gcc-patches@gcc.gnu.org` as plain text or as a `text/x-patch`
   attachment. State that you do not have GCC write access.
