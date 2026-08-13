# PR 126782 gcc-patches packet

This directory holds the private packet for
[PR target/126782](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126782).

Bugzilla status is UNCONFIRMED. Comments are 0 through 5. Last live read:
2026-08-13.

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
  the current source diff. Rebuild it with `git format-patch` after
  the Finch tests end.
- `COMMIT-MESSAGE.txt`: subject, body, ChangeLog, and `Signed-off-by`.
- `EMAIL.txt`: draft gcc-patches mail. Do not send it yet.
- `TEST-RECORD.md`: tests that already ran, and tests that still run.
- `evidence/`: logs, probes, style output, and the compliance list.

Base revision: gcc-mirror `master` `c5d147d7370fb36834c9348c5d3bab229d89fb3e`.
Current diff SHA-256: `1f154efcfcba938e938b3c06256bf9cbc2893209d1e54ccd329a251d4a09f621`.
Rebuild the patch with `git format-patch` after the Finch tests end.

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

- Send the patch to `gcc-patches@gcc.gnu.org`.
- A Bugzilla attachment does not replace that mail.
- Do not comment on Bugzilla.
- Do not change Bugzilla fields.
- Do not push to a GCC remote.
- This patch is for trunk only. Do not ask for a GCC 16 backport.
- Get user approval of the exact From, To, subject, body, and patch
  bytes before any public send.

Likely reviewers: Joseph Myers, Jason Merrill.
The change is in the generic header, not in Darwin-only code.
