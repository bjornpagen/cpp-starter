# PR 126782 gcc-patches packet

This directory holds the private packet for
[PR target/126782](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126782).

Work type: Patch. Bugzilla already exists. Do not file a duplicate.

Bugzilla status at last live browser read (2026-08-13): UNCONFIRMED.
Comments 0 through 5. A later curl fetch hit Anubis and does not count.
The sending agent must re-read the report in a real browser before send.

Do not restore the old Darwin fixincludes files.

## Terms

- `__need_rsize_t`: a request macro. A system header sets it, then includes
  `<stddef.h>`, to get only the `rsize_t` type.
- Annex K: the optional C11 bounds-checking library. This patch does not
  implement it.
- bootstrap: GCC builds itself in three stages.
- `gcc-patches`: the mailing list that reviews GCC patches.

## Files

- `0001-stddef.h-Support-explicit-__need_rsize_t-PR126782.patch`:
  the public `git format-patch` message. This is what `git send-email`
  must send.
- `0001-stddef.h-Support-explicit-__need_rsize_t-PR126782.patch.sha256`:
  SHA-256 of that file.
- `EMAIL.txt`: From, To, Cc, Subject, and the public body. No agent notes.
- `COMMIT-MESSAGE.txt`: same body plus the subject line used for the
  local GCC commit.
- `APPROVAL-PACKET.md`: exact public values. Get Bjorn's yes before send.
- `HANDOFF.md`: send command and stop conditions for the sending agent.
- `PRIVATE-RESEARCH.md`: private notes. Do not paste them into Bugzilla.
- `TEST-RECORD.md`: Darwin probes, Finch bootstrap, and `make -k check`.
- `evidence/`: logs, probes, style output, and the compliance list.

Base revision: gcc-mirror `master` `c5d147d7370fb36834c9348c5d3bab229d89fb3e`.
Local GCC commit (do not push): `63e3cdeb413866b501b102e5e37afcd6a1f510d7`.
Patch SHA-256: `385acc6a7a51883837234427e18dd877712cced040bff6972077a9f084d786de`.
Size: 7930 bytes.

## What the patch does

GCC `<stddef.h>` answers an explicit `__need_rsize_t` request.
It defines `rsize_t` as `__SIZE_TYPE__`.
It guards the type with `_RSIZE_T`.
It then clears `__need_rsize_t`.
A partial request does not set `_STDDEF_H`.
A normal include does not declare `rsize_t`.
The same holds when `__STDC_WANT_LIB_EXT1__` is set.

This is not Annex K.
This is not a Darwin fixincludes rule.
This is not a claim that Darwin `-fmodules` still fails on trunk.
Trunk no longer reports `__has_feature(modules)`.

## Send rules

- Status: local preparation complete. Public send is blocked.
- Send the patch to `gcc-patches@gcc.gnu.org` only after Bjorn approves
  the exact values in `APPROVAL-PACKET.md`.
- A Bugzilla attachment does not replace that mail.
- Do not comment on Bugzilla before the gcc-patches archive shows the mail.
- Do not change Bugzilla fields.
- Do not push to a GCC remote.
- Do not ask the list for a GCC 16 backport.
- Recheck live GCC policy and PR 126782 in a real browser before send.

Likely reviewers: Joseph Myers, Jason Merrill.
The change is in the generic header, not in Darwin-only code.
