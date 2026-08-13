# Upstream work queue

This file tracks work that remains after the Bugzilla filings. The canonical
report status is in [`BUGS.md`](BUGS.md).

## Now

1. Investigate `__need_rsize_t` support for PR 126782.
   - Start with
     [`gcc-fixincludes-darwin-rsize-t/LOCAL-AGENT-HANDOFF.md`](gcc-fixincludes-darwin-rsize-t/LOCAL-AGENT-HANDOFF.md).
   - Replace the superseded Darwin fixincludes design.
   - Implement only the explicit `__need_rsize_t` protocol in the first patch.
   - Add a regression test.
   - Run the exact patch on Linux and macOS before preparing a patch email.

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
