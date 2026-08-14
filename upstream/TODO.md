# Upstream work queue

This file tracks work that remains after the Bugzilla filings. The canonical
report status is in [`BUGS.md`](BUGS.md).

## Now

1. PR 126782: wait for review of the
   [submitted patch](https://inbox.sourceware.org/gcc-patches/20260814032423.29082-1-hello@bjornpagen.com/).
   Reply in the same thread if a maintainer requests changes.

## Next

1. Prepare the Apple `_rsize_t.h` report after the PR 126782 fix
   direction is stable.

## Later

1. Re-test every pin in `PINS.md` after a GCC toolchain update.
2. If PR 126782 receives no review after about two weeks, send one short
   ping in the original thread. Recheck GCC policy and trunk first.

## Removed work

- Do not write a Darwin `-fhardened` enablement patch. GCC documents
  `-fhardened` as GNU/Linux-only.
- Do not write a partial-application patch for PR 126823.
- Do not send the superseded fixincludes patch for PR 126782.
- Do not file the reflection/modules defect. PR 124582 already fixed it.
- Do not recreate deleted reproduction trees for pointer-only ledger rows.
- Do not ask for a GCC 16 backport of PR 126782. Trunk only.
