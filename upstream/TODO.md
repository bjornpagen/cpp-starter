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
2. Monitor PR 126783.
   - Patrick Palka confirmed the regression range and accepted the assignment.
   - Do not duplicate the assigned work.
   - Supply tests or verification if the maintainer requests them.
3. Monitor PR 126786, PR 126805, PR 126806, PR 126819, and PR 126822.
   - Read new comments before drafting any reply.
   - Add evidence only when it is new and decisive.
4. Keep PR 126823 closed locally.
   - Do not defend the unsupported-target semantics.
   - Keep the investigation as a record of the support-scope failure.

## Next

1. Draft one evidence comment for GCC PR 82005.
   - Include the invalid Mach-O DWARF verification.
   - Include the Linux ELF control.
   - Include the memory guard warning.
2. Draft one comment for LLVM issue 102965.
   - Include the measured `dsymutil` growth.
   - Keep the GCC producer defect separate from the consumer hardening request.
3. Prepare the Apple `dsymutil` report after the public GCC and LLVM references
   exist.
4. Prepare the Apple `_rsize_t.h` report after the PR 126782 fix direction is
   stable.

## Later

1. Re-run open reproductions after a GCC toolchain update.
2. Update Known to Work or Known to Fail only after an executed test.
3. Prepare patches for analyzer reports only after the root cause, tests, and
   maintainer direction support the design.
4. Add a read-only Bugzilla status checker if manual ledger updates become
   burdensome.
5. Send one polite ping after an appropriate period of silence. Recheck GCC
   policy and trunk before each ping.

## Removed work

- Do not write a Darwin `-fhardened` enablement patch. GCC documents
  `-fhardened` as GNU/Linux-only.
- Do not write a partial-application patch for PR 126823.
- Do not send the superseded fixincludes patch for PR 126782.
- Do not file the reflection/modules defect. PR 124582 already fixed it.
