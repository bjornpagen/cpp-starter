# GCC submission queue

The queue has six entries. Five entries are verified and ready to file. One entry
is reopened for patch rework. We submitted the previously filed material (GMF
variable ICE, silent empty `std` module, two Bugzilla comments) on 2026-08-10.
Then we removed that material from the tree. If a maintainer asks for that
material, recover it from the git history before commit `e0a9d27`. The `rsize_t`
entry stays in the queue. Its report is filed (PR target/126782). But maintainer
feedback redirected the fix from a Darwin fixincludes rule to `__need_rsize_t`
support in GCC's `<stddef.h>`. Thus the patch work is open again.

DCO means Developer Certificate of Origin. ICE means internal compiler error. GMF means
global module fragment.

## Before you start

1. Use your existing GCC Bugzilla account.
2. Use `Bjorn Pagen <hello@bjornpagen.com>` for every public submission.
3. Read the [GCC DCO](https://gcc.gnu.org/dco.html) before you send any patch.
4. Confirm that you have the right to submit every patch line.

The public mailing-list archive keeps your name, email address, and message permanently.

## Queue summary

| Directory | Action | Status |
|---|---|---|
| `gcc-lto-modules-debug-oom` | File one report against the Darwin target side of debug emission. File one Apple Feedback (dsymutil) after the GCC PR exists. | Ready — Linux control done: ELF fat LTO objects verify clean, Mach-O only |
| `gcc-darwin-fhardened-coverage` | File one `middle-end` report. Send one `configure.ac` Darwin-enablement patch after. | Ready — the umbrella is Linux-only by GCC's own configure |
| `gcc-analyzer-call-summary-ice` | File one `analyzer` report. | Ready — the 21-line reduction ICEs on Linux too; cross-platform |
| `gcc-analyzer-fd-leak-raii-fp` | File two `analyzer` reports. | Ready — defect 1 reproduces on Linux; defect 2 occurs only on libcs without nothrow annotations |
| `gcc-modules-freflection-typedef-merge` | File one `c++` report (`rejects-valid`, See Also PR 122785). | Ready — 6-line reduction, fails against both glibc and Apple SDK headers |
| `gcc-fixincludes-darwin-rsize-t` | Write the replacement patch: `__need_rsize_t` in GCC `<stddef.h>` (PR target/126782, comments 3–5). Then the Apple report. | Reopened — fixincludes approach rejected by maintainer feedback; see the entry's `SUBMIT.md` work plan |

The `README.md` of each entry contains the exact commands, the verbatim output, the
environment tuple, the analysis, and the suggested Bugzilla component and title.

DANGER — this warning applies only to `gcc-lto-modules-debug-oom`. Run its
reproduction only under the memory guard that the entry documents. An unguarded run
of the archive-link case grows without bound. One unguarded run caused a kernel
panic on the development machine.

## Policy sources

- [GCC contribution and test rules](https://gcc.gnu.org/contribute.html)
- [GCC DCO rules](https://gcc.gnu.org/dco.html)
- [GCC bug-report rules](https://gcc.gnu.org/bugs/)
- [GCC coding conventions](https://gcc.gnu.org/codingconventions.html)
- [GCC mailing-list rules](https://gcc.gnu.org/lists.html)

If nobody replies after approximately two weeks, send one polite ping. Send the
ping as a reply to the original patch thread. Start a new thread for a revised
patch. In that new thread, state each change from the earlier version.
