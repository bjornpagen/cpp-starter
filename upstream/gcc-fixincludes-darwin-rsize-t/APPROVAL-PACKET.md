# Patch approval packet — PR 126782

Work type: Patch.
Status: ready for exact approval. Public send remains blocked until Bjorn
approves every public value in this packet.

## Legal status

Route: per-patch Developer Certificate of Origin.
Not FSF copyright assignment.
Not a MAINTAINERS.yml DCO entry.
Not the small-change exception.

Contributor: Bjorn Pagen <hello@bjornpagen.com>
Ownership: Bjorn confirmed the work is his (2026-08-13).
Employer or school does not own the contribution (same confirmation).

Sign-off line, already in the patch:

    Signed-off-by: Bjorn Pagen <hello@bjornpagen.com>

## Base revision

gcc-mirror master `c5d147d7370fb36834c9348c5d3bab229d89fb3e`
Local commit (do not push): `63e3cdeb413866b501b102e5e37afcd6a1f510d7`
The timeline on `develop.html` placed GCC 17 in Stage 1 on 2026-08-13.
The page was last modified on 2026-08-07. Recheck the stage before sending.

## Changed files

- gcc/ginclude/stddef.h
- gcc/testsuite/gcc.dg/stddef-need-rsize-1.c
- gcc/testsuite/gcc.dg/stddef-need-rsize-2.c
- gcc/testsuite/gcc.dg/stddef-need-rsize-3.c
- gcc/testsuite/gcc.dg/stddef-need-rsize-4.c
- gcc/testsuite/g++.dg/stddef-need-rsize-1.C

One logical change. No formatting-only files. No generated files.

## Tests

Canonical names: build = host = target = aarch64-unknown-linux-gnu

Bootstrap: `configure` without `--disable-bootstrap` and without
`--enable-languages`. Then `make -j10`. Default languages
`c,c++,fortran,lto,objc`. Stages 2 and 3 compared equal.
`--enable-checking=yes --disable-nls --disable-multilib --with-system-zlib`.
Compiler: gcc 17.0.0 20260813 (experimental).

Testsuite: `make -k check` from the top of the build tree.
Exit 2, expected under `-k` when FAILs exist.
Totals: PASS=997628 FAIL=57 XFAIL=5623 XPASS=0 UNSUPPORTED=11149
UNRESOLVED=7 ERROR=0.

New tests, all PASS:

- gcc.dg/stddef-need-rsize-1.c
- gcc.dg/stddef-need-rsize-2.c
- gcc.dg/stddef-need-rsize-3.c
- gcc.dg/stddef-need-rsize-4.c
- g++.dg/stddef-need-rsize-1.C at c++98, c++20, and c++29

The 57 failures are in aarch64 SME, SVE, and AdvSIMD tests, two libgomp
C++ compilations, and libstdc++ filesystem copy tests. None mention
`stddef.h` or `rsize_t`.

This is a post-patch result only. No local pre-patch full-suite baseline
ran, and no `gcc-testresults` comparison was used. The public message
discloses this deviation.

Darwin protocol probes (header override, gcc 17.0.0 20260810,
aarch64-apple-darwin24) are in TEST-RECORD.md. They are extra. They
are not the policy bootstrap.

## Checks that ran

- git diff --check: clean
- git apply --check on parent: clean
- applied tree equals commit tree
- contrib/check_GNU_style.sh: two hits that match existing stddef.h
  style; see evidence/logs/style-notes.txt
- contrib/mklog.py: ran; handwritten ChangeLog kept
- contrib/gcc-changelog/git_check_commit.py: OK
- source diff from the tested local commit
- mail patch derived from `git format-patch -1`; public prose revised
- Darwin protocol probes

## Checks that did not run

- Pre-patch `make -k check` on this host. No `gcc-testresults`
  comparison was used.
- Literal make-target name `make bootstrap`. The build used
  `make -j10` with bootstrap enabled. Stages 2 and 3 compared equal.
- `make -C gcc -k check-c++-all`. This is not a C++ front-end change.
  Full `make -k check` already ran the new g++ test at three -std= levels.
- Two in-tree MPFR tests, `tget_ld_2exp` and `tset_ld`, hung with empty
  logs and were stopped with SIGTERM. They are outside GCC's DejaGnu
  testsuite and do not test `stddef.h`.
- Live Bugzilla re-read this pass: Anubis challenge. Last browser read
  2026-08-13, comments 0-5.

## Recipients

From: Bjorn Pagen <hello@bjornpagen.com>
To: gcc-patches@gcc.gnu.org
Cc: (none)

lists.html extra lists do not apply. This is generic stddef.h, not
libstdc++, Fortran, GCC Rust, libgccjit, Algol 68, or BPF.

## Subject

[PATCH] stddef.h: Support explicit __need_rsize_t [PR126782]

Text after the classifier: 52 characters. Limit 75.
Uses [PR126782], not [PR target/126782].

## Body

Exact public body. It is also in `EMAIL.txt`, `COMMIT-MESSAGE.txt`, and
the mail patch.

```
GCC's <stddef.h> supports selective type requests such as
__need_size_t.  System headers, including the macOS SDK, use the same
protocol to request rsize_t with __need_rsize_t.  Clang handles this
request, but GCC's <stddef.h> returned without defining rsize_t.

This patch handles only an explicit __need_rsize_t request.  It defines
rsize_t as __SIZE_TYPE__, uses _RSIZE_T to match Clang and the SDK's
non-modular path, and undefines __need_rsize_t after processing it.  As
with other partial requests, it does not mark <stddef.h> as fully
included.

A normal inclusion still does not define rsize_t, including when
__STDC_WANT_LIB_EXT1__ is set.  Defining it in that case would expose an
Annex K name on targets whose C libraries do not implement Annex K.
RSIZE_MAX and the bounds-checking functions remain the C library's
responsibility.

PR target/126782 was first observed on Darwin with -fmodules.  GCC 16
reported __has_feature(modules), which made the SDK request
__need_rsize_t.  GCC trunk no longer reports that feature, so the
original testcase now compiles without this patch.  The patch adds
support for the underlying header protocol without restoring the old
trigger.

Bootstrapped all default languages on aarch64-unknown-linux-gnu.
Stages 2 and 3 compared equal.  A full make -k check completed with:
PASS=997628 FAIL=57 XFAIL=5623 XPASS=0 UNSUPPORTED=11149 UNRESOLVED=7
ERROR=0.  All five new tests passed.  No local pre-patch full-suite
baseline was run.  The 57 failures do not mention stddef.h or rsize_t.
Two in-tree MPFR long-double tests outside GCC's DejaGnu suite hung
with empty logs and were terminated.

I do not have GCC write access.
OK for trunk?

gcc/ChangeLog:

	PR target/126782
	* ginclude/stddef.h (__need_rsize_t): New selective-inclusion
	request.  Define rsize_t as __SIZE_TYPE__.

gcc/testsuite/ChangeLog:

	PR target/126782
	* gcc.dg/stddef-need-rsize-1.c: New test.
	* gcc.dg/stddef-need-rsize-2.c: New test.
	* gcc.dg/stddef-need-rsize-3.c: New test.
	* gcc.dg/stddef-need-rsize-4.c: New test.
	* g++.dg/stddef-need-rsize-1.C: New test.

Signed-off-by: Bjorn Pagen <hello@bjornpagen.com>
```

## MIME details

Attachment / send file:
`0001-stddef.h-Support-explicit-__need_rsize_t-PR126782.patch`
SHA-256: `51b34741bdbffdbdb7c9e81cae509d01461ed9f6982f7e3d0acf9a580f067d05`
Size: 7990 bytes
charset: us-ascii
Method: `git send-email` of that file, `--transfer-encoding=7bit`.
Intended MIME: text/plain or text/x-patch.
Forbidden: application/*, base64, quoted-printable, HTML.

Write access statement: present.
ChangeLog: present in the commit message. No ChangeLog file edit.
PR number: present.
Sign-off: present.
Co-authored-by: absent.

## Open items and deviations

1. No pre-patch full testsuite or suitable `gcc-testresults` comparison
   is recorded. The public test statement discloses this deviation.
2. Used `make -j10` with bootstrap enabled, not the literal target
   name `make bootstrap`. Stages 2 and 3 compared equal.
3. Two in-tree MPFR long-double tests were stopped with SIGTERM after
   they hung. They are not GCC DejaGnu tests.
4. Did not run `make -C gcc -k check-c++-all` (not a C++ front-end
   change).
5. This pass could not re-read Bugzilla in a browser. Curl hit Anubis.
6. `check_GNU_style.sh` reports the existing `defined(__need_*)`
   no-space style and `#endif /* _RSIZE_T */`. The patch matches
   `gcc/ginclude/stddef.h`. Those hits were not "fixed".
7. `mklog.py` named extra stddef.h tokens. The handwritten ChangeLog
   line was kept.

## Approval question

May the sending agent send this exact mail?

- From: Bjorn Pagen <hello@bjornpagen.com>
- To: gcc-patches@gcc.gnu.org
- Cc: none
- Subject: [PATCH] stddef.h: Support explicit __need_rsize_t [PR126782]
- Body: the body block above
- File: 0001-stddef.h-Support-explicit-__need_rsize_t-PR126782.patch
- SHA-256: 51b34741bdbffdbdb7c9e81cae509d01461ed9f6982f7e3d0acf9a580f067d05
- MIME: git send-email, 7bit, text/plain or text/x-patch
- Sign-off: Signed-off-by: Bjorn Pagen <hello@bjornpagen.com>

Answer yes only for these values. Public send stays blocked until then.
