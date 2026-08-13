# Upstream work queue

This file tracks work that remains after the Bugzilla filings. The canonical
report status is in [`BUGS.md`](BUGS.md).

## Now

1. PR 126782: local patch packet is in
   [`gcc-fixincludes-darwin-rsize-t/`](gcc-fixincludes-darwin-rsize-t/).
   - Public send is blocked.
   - Bjorn must approve the exact values in
     `gcc-fixincludes-darwin-rsize-t/APPROVAL-PACKET.md`.
   - Then a sending agent follows `HANDOFF.md`.
   - Do not mail gcc-patches, comment on Bugzilla, change Bugzilla
     fields, or push a GCC remote until that approval names this
     SHA-256:
     `385acc6a7a51883837234427e18dd877712cced040bff6972077a9f084d786de`.

## Next

1. After the gcc-patches archive shows the mail, draft a one-line
   Bugzilla note with that URL and get a separate approval.
2. Prepare the Apple `_rsize_t.h` report after the PR 126782 fix
   direction is stable.

## Later

1. Re-test every pin in `PINS.md` after a GCC toolchain update.
2. Send one polite ping after an appropriate period of silence. Recheck GCC
   policy and trunk before each ping. Reply to the original thread. Do not
   start a new ping thread.

## Removed work

- Do not write a Darwin `-fhardened` enablement patch. GCC documents
  `-fhardened` as GNU/Linux-only.
- Do not write a partial-application patch for PR 126823.
- Do not send the superseded fixincludes patch for PR 126782.
- Do not file the reflection/modules defect. PR 124582 already fixed it.
- Do not recreate deleted reproduction trees for pointer-only ledger rows.
- Do not ask for a GCC 16 backport of PR 126782. Trunk only.
