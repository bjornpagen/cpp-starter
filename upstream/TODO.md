# Upstream work queue

This file tracks work that remains after the Bugzilla filings. The canonical
report status is in [`BUGS.md`](BUGS.md).

## Now

No GCC submission work is active.

## Next

1. Respond in the original PR 126782 review thread only if a maintainer asks
   for a change or more evidence.

## Later

1. Re-test every pin in `PINS.md` after a GCC toolchain update.

## Removed work

- Do not write a Darwin `-fhardened` enablement patch. GCC documents
  `-fhardened` as GNU/Linux-only.
- Do not write a partial-application patch for PR 126823.
- Do not send the superseded fixincludes patch for PR 126782.
- Do not continue the `__need_rsize_t` patch without maintainer interest.
  GCC trunk commit `08ede4f` removed the active trigger.
- Do not file the reflection/modules defect. PR 124582 already fixed it.
- Do not recreate deleted reproduction trees for pointer-only ledger rows.
- Do not ask for a GCC 16 `__need_rsize_t` backport. The trunk fix removed the
  incorrect Clang-modules feature claim instead.
