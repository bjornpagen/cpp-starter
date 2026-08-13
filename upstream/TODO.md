# Upstream work queue

This file tracks work that remains after the Bugzilla filings. The canonical
report status is in [`BUGS.md`](BUGS.md).

## Now

1. PR 126782 private packet is ready.
   - Review [`gcc-fixincludes-darwin-rsize-t/0001-stddef.h-Support-explicit-__need_rsize_t-PR-target-126782.patch`](gcc-fixincludes-darwin-rsize-t/0001-stddef.h-Support-explicit-__need_rsize_t-PR-target-126782.patch)
     and [`COMMIT-MESSAGE.txt`](gcc-fixincludes-darwin-rsize-t/COMMIT-MESSAGE.txt).
   - Do not mail gcc-patches, comment on Bugzilla, or add `Signed-off-by`
     until the exact public artifact and legal route are approved.

## Next

1. Prepare the Apple `_rsize_t.h` report after the PR 126782 fix direction is
   stable.

## Later

1. Re-test every pin in `PINS.md` after a GCC toolchain update.
2. Send one polite ping after an appropriate period of silence. Recheck GCC
   policy and trunk before each ping.

## Removed work

- Do not write a Darwin `-fhardened` enablement patch. GCC documents
  `-fhardened` as GNU/Linux-only.
- Do not write a partial-application patch for PR 126823.
- Do not send the superseded fixincludes patch for PR 126782.
- Do not file the reflection/modules defect. PR 124582 already fixed it.
- Do not recreate deleted reproduction trees for pointer-only ledger rows.
