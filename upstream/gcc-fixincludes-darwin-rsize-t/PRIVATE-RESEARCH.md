# Private research record — PR 126782

Work type: Patch for an existing Bugzilla report.
Do not file a new report.
Do not paste this file, TEST-RECORD.md, HANDOFF.md, APPROVAL-PACKET.md,
COMPLIANCE.md, or evidence logs into Bugzilla or gcc-patches.

## Keep private

- Variant matrices (`evidence/protocol-matrix.txt`).
- Finch container names, pipe-full hang, ptrace tee redirect, resume.sh.
- Competing designs: Darwin fixincludes, Annex K full inclusion, Clang
  `__STDC_WANT_LIB_EXT1__` path from comment 5.
- Uncertain assumptions about SDK modular `__RSIZE_T` vs `_RSIZE_T`.
- Cursor `Co-authored-by` injection and the commit-tree rewrite.
- The Darwin `rsize.cc -fmodules` history that already compiles on trunk.

## Public surfaces

- Bugzilla: already filed as PR 126782. No new comment until a
  gcc-patches archive URL exists, and then only after a separate
  approval of that comment.
- Patch mail: `EMAIL.txt` plus
  `0001-stddef.h-Support-explicit-__need_rsize_t-PR126782.patch`.

## Why trunk Darwin `-fmodules` is not the regression test

GCC 16 reported `__has_feature(modules)`, so the SDK took its
`__need_rsize_t` branch. Trunk no longer reports that feature. The
original `rsize.cc` already compiles. Comment 4 suggested support for
`__need_rsize_t` instead of a Darwin fixincludes rule. The patch follows
that direction. Do not post a Bugzilla comment that only says trunk
already works. That would add no useful evidence.
