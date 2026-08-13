# PR 126782 — gcc-patches packet

Private packet for [PR target/126782](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126782).
Bugzilla is UNCONFIRMED, comments 0–5. Last live read: 2026-08-13.

This directory is only the submission. Do not restore the old Darwin
fixincludes files.

## Files

| File | Role |
|---|---|
| `0001-stddef.h-Support-explicit-__need_rsize_t-PR-target-126782.patch` | Exact tested diff |
| `COMMIT-MESSAGE.txt` | Subject, body, ChangeLog text. No `Signed-off-by` |
| `TEST-RECORD.md` | What ran. Do not claim a bootstrap |

SHA-256 of the patch: `b56ad4476a7587e789c1276f915041c2690be3ae0afb3b8dbad95b739c362536`

Base: gcc-mirror `master` `c5d147d7370fb36834c9348c5d3bab229d89fb3e`

## What the patch is

GCC `<stddef.h>` honors `__need_rsize_t`. It defines `rsize_t` as
`__SIZE_TYPE__` behind `_RSIZE_T` and consumes the request. A partial
request does not set `_STDDEF_H`. A normal include, including with
`__STDC_WANT_LIB_EXT1__`, does not declare `rsize_t`.

It is not Annex K. It is not a Darwin fixincludes rule. It is not a
claim that current-trunk Darwin `-fmodules` still fails; trunk no longer
reports `__has_feature(modules)`.

## Send rules

- Route: `gcc-patches@gcc.gnu.org`. A Bugzilla attachment does not replace that.
- Do not comment on Bugzilla, change Bugzilla fields, or push to GCC.
- Do not add `Signed-off-by` until the user confirms the legal route and
  the exact From identity.
- Do not request a GCC 16 backport unless the user asks.
- Recipients, subject, body, patch bytes, and test statement need user
  approval before they leave this machine.

Likely reviewers: Joseph Myers, Jason Merrill. The file is generic, not
Darwin-local.
