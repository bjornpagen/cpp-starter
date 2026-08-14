# PR 126782: explicit `__need_rsize_t`

This directory preserves the technical record for
[PR target/126782](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126782).

The patch was submitted to `gcc-patches` on 2026-08-14:

https://inbox.sourceware.org/gcc-patches/20260814032423.29082-1-hello@bjornpagen.com/

Bugzilla comment 6 links the review thread. The report remains UNCONFIRMED.

## Patch

The patch makes GCC `<stddef.h>` answer an explicit `__need_rsize_t`
request. It does not add Annex K, change normal inclusion, or restore the old
Darwin `-fmodules` trigger.

Base revision: gcc-mirror `master`
`c5d147d7370fb36834c9348c5d3bab229d89fb3e`.

Local GCC commit: `63e3cdeb413866b501b102e5e37afcd6a1f510d7`.

Submitted patch:
`0001-stddef.h-Support-explicit-__need_rsize_t-PR126782.patch`.

SHA-256:
`5d8a6a60cec5186c740246b94bc4fcd48e4d227b2b8bce603388b917457fa4a0`.

## Retained evidence

- `TEST-RECORD.md` records the Darwin probes, three-stage bootstrap, and
  `make -k check`.
- `evidence/logs/` contains the build and test outputs.
- `evidence/probes/` and `evidence/scripts/` contain the reproducible local
  checks.

Wait for review. If a maintainer requests a revision, prepare and test a new
patch from the GCC source tree, then reply with a new version in the review
thread.
