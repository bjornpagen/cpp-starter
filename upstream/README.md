# upstream/ — GCC submission queue

This directory contains the exact reports, reproductions, and patch artifact
prepared for GCC. NVIDIA/stdexec PRs #2167 and #2168 live in the maintained
stdexec fork and are not part of this queue.

| Item | Submission | Status |
|---|---|---|
| `gcc-fixincludes-darwin-rsize-t` | Bugzilla `target`, then one patch to `gcc-patches@` | **READY after Bugzilla assigns the PR number** |
| `gcc-ice-modules-defaulted-friend-eq` | duplicate record | **DO NOT FILE** — PR 126223 comment 3 already covers it |
| `gcc-ice-gmf-consteval-redecl` | Bugzilla `c++`, `ice-on-valid-code` | report and warning-free repro ready |
| `libstdcxx-silent-empty-std-module` | Bugzilla `libstdc++` | report ready; file after fixincludes PR |

There is one GCC code patch and two new Bugzilla reports in this queue. The
defaulted-comparison directory is only a duplicate/provenance record.

Official policy sources checked for this pass: [contributing and
testing](https://gcc.gnu.org/contribute.html), [DCO
sign-off](https://gcc.gnu.org/dco.html), [bug-report
content](https://gcc.gnu.org/bugs/), and [mailing-list posting
rules](https://gcc.gnu.org/lists.html).

## Submission invariants

- GCC contribution and bug-reporting policy was re-read on 2026-08-10.
- The patch was built and regression-tested exactly as commit `9f4a16ae995`
  on trunk base `a1ba7736cfb`. It contains only the fixincludes change,
  regenerated `fixincl.x`, and its fixture. The unchanged format-patch also
  applies cleanly with `git am` to fetched upstream master `8412da1ce39`
  (2026-08-10).
- Author, committer, and DCO `Signed-off-by` identities are
  `Bjorn Pagen <bjorn.pagen@alpha.school>`.
- Before sending, personally confirm that no employer or school owns the
  contribution. GCC's DCO path certifies that right; if an employer or school
  owns the work, obtain the required disclaimer instead of relying on the
  sign-off alone.
- This patch had AI-assisted drafts. GCC's published contribution and DCO
  pages do not provide a separate AI exception: the human signer still
  certifies the right to submit every line. Personally review the final
  `inclhack.def` change and commit message and sign only if that certification
  is true. The generated `fixincl.x` and fixture are outputs of GCC's own
  `genfixes` pipeline, not model-authored source.
- The paired Linux bootstrap and test runs are preserved. GCC's full-suite
  comparator is noisy because the unpatched analyzer plugin build timed out
  under load and the two runs selected different LTO/ASan option spellings.
  It contains no baseline PASS that became a patched FAIL. A sequential rerun
  of the entire affected analyzer-plugin slice is identical: 28 PASS and 5
  expected XFAIL in both trees; `contrib/compare_tests` exits 0 for that
  focused comparison. Do not shorten this to an unsupported claim that every
  full-suite result was byte-for-byte identical.
- The new GMF module ICE leads with official FSF GCC builds. The out-of-tree
  aarch64-Darwin build is corroboration only.
- GCC asks for an archive only when reproduction requires multiple source
  files. The GMF report therefore has a tiny archive containing only its
  primary text sources; the sources also remain inline in the report body.
- Do not upload build products, CMIs, object files, core files, or the large
  stdexec source tree.

## Why the fixincludes design is the current recommendation

The SDK predicate is the false statement: `__has_feature(modules)` is being
used as a proxy for “Clang with Clang's `__need_rsize_t` protocol.” The patch
corrects that predicate at GCC's existing system-header adaptation boundary.
It does not add a permanent Darwin conditional to every compilation, change
GCC's truthful feature reporting, or teach generic `stddef.h` another
compiler's private protocol.

This matches current GCC practice. Recent Darwin SDK incompatibilities are
handled in fixincludes, and GCC's Darwin maintainer has described fixincludes
as still necessary for shipped SDKs. PR 116827 is useful contrast: it changed
GCC's `stddef.h` only where the SDK include-guard behavior required compiler
cooperation. Here the SDK already has a correct non-Clang typedef branch, so
routing GCC to it is the smaller repair.

The hack's bypass matches the already-corrected predicate, rather than any
unrelated occurrence of `__clang__`. Running the fix twice is therefore
idempotent without broadly skipping future SDK variants.

## Regression-test record

The unpatched and patched trees use the same GCC revision, container image,
configure command, and job count:

```text
/src/configure --prefix=/work/install --disable-nls \
  --enable-checking=release --with-system-zlib
make bootstrap -j10
make -k check -j10
```

Both default-language bootstraps completed on
`aarch64-unknown-linux-gnu`. The normal and strict full-suite comparisons
both report no `Tests that now fail, but worked before` section, but exit 1
because the runs are not fully comparable: the baseline timed out while
building `analyzer_kernel_plugin.so`; the patched run reached its tests; and
resource-sensitive LTO/ASan option variants appeared in only one run.

The five entries under the comparator's `New tests that FAIL` heading are
actually GCC tests marked `XFAIL`, all gated by the analyzer plugin that the
baseline failed to build. We reran that plugin plus all four source files
behind those entries, serially, against both compilers. Each result is exactly
28 expected passes and 5 expected failures, and GCC's comparator reports no
differences (exit 0). This is the defensible result to describe to reviewers;
the full raw comparison must remain available if requested.

## Exact send order

1. Obtain a GCC Bugzilla account. If self-service registration fails, email
   `gcc-bugzilla-account-request@gcc.gnu.org`.
2. File the fixincludes report from its `SUBMIT.md`, Product `gcc`, Component
   `target`, Version `16.1.0`. Record the assigned number as `NNNNN`.
3. In the GCC clone, amend the actual patch commit:
   - change its subject to
     `fixincludes: Fix rsize_t with Darwin modules [PRNNNNN]`;
   - add `PR target/NNNNN` immediately above `fixincludes/ChangeLog:`;
   - keep `Signed-off-by` intact;
   - regenerate the format-patch;
   - rerun `git_check_commit.py`, `check_GNU_style.sh`, and `git am` on a
     clean current-trunk worktree.
4. Email the regenerated format-patch to `gcc-patches@gcc.gnu.org`; its
   generated subject will be
   `[PATCH] fixincludes: Fix rsize_t with Darwin modules [PRNNNNN]`.
   Send plain text or `text/x-patch`, state the tested host/target and exact
   results, and say: “I do not have write access to the GCC repository.” CC
   Bruce Korb, Iain Sandoe, and Mike Stump using the addresses in the current
   GCC `MAINTAINERS` file.
5. Do **not** file the defaulted-friend report or add a redundant comment.
   PR 126223 comment 3 already contains the same minimal `operator==` case and
   matrix.
6. File the GMF report. Attach `gmf-extern-inline-repro.tar.gz`. The
   warning-free `q.h` + `repro-include.cc` pair is the primary testcase;
   `repro.cc` is only a secondary reduction.
7. File the libstdc++ report last. Replace `PR target/NNNNN` with the real
   fixincludes PR. Cross-reference PR 124268, PR 124554, and the patch archive
   URL. The report intentionally preserves the supported-target bootstrap
   rationale and challenges only the invalid installed metadata.
8. File the Apple Feedback draft only after the GCC patch is actually posted,
   or change “has been submitted” to “has been prepared.”

If a patch receives no response after about two weeks, send one polite reply
on the same thread with a brief summary and the archive URL. A revised patch
starts a new thread and explains what changed.

## Existing Bugzilla comment-only updates

- PR 124197: report the verified `template for` / `-Wshadow` behavior on GCC
  16.1.0, including the reflection variant and that it fires once per expanded
  element. Do not open a duplicate.
- PR 71962: add the reflection/consteval impact of UBSan refusing to
  constant-fold `std::string(ptr, size)` over vague-linkage storage, and note
  the candidate patch attached on 2026-08-05. Do not open a duplicate.

## Deliberately not filed

- The partition-BMI `std::expected` corruption lacks a standalone testcase;
  its reduction ledger stays with the workaround pin in bumbledb.
- clang-tidy `misc-unused-using-decls` on `export using` is already fixed by
  llvm/llvm-project#183638 (`ce6a3d9`).
- `^^` on using-declarations and `inplace_vector::try_push_back` returning
  `optional<T&>` conform to the adopted proposals and are not bugs.
