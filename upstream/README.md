# GCC submission queue

The queue has six entries. Three entries produce five new Bugzilla reports: one
analyzer ICE report, two analyzer fd-leak reports, and two `-fhardened` reports.
One entry produces two comments on existing reports (GCC PR 82005 and
llvm-project issue #102965), plus one Apple Feedback after. One entry is retired:
upstream already fixed its defect as PR 124582 for GCC 16.2. One entry
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
| `gcc-lto-modules-debug-oom` | Comment on PR 82005 + comment on llvm-project #102965 + Apple Feedback after. | Ready to comment — Linux control done: ELF fat LTO objects verify clean, Mach-O only |
| `gcc-darwin-fhardened-coverage` | File two reports (warning class; silent partial application); enablement patch queued in `TODO.md`. | Ready — the umbrella is Linux-only by GCC's own configure; the gate persists on master 475e9eff |
| `gcc-analyzer-call-summary-ice` | File one `analyzer` report, Blocks: 99390. | Ready — trunk-run confirmed: both reproductions ICE on master 475e9eff; cross-platform |
| `gcc-analyzer-fd-leak-raii-fp` | File two `analyzer` reports. | Ready — defect 1 confirmed by run on master 475e9eff with identical counts; defect 2 occurs only on libcs without nothrow annotations, and its basis persists on master |
| `gcc-modules-freflection-typedef-merge` | DO NOT FILE — fixed upstream as PR 124582 (16.2); entry retained as record. | Retired — all four reproduction compiles exit 0 on master 475e9eff |
| `gcc-fixincludes-darwin-rsize-t` | Write the replacement patch: `__need_rsize_t` in GCC `<stddef.h>` (PR target/126782, comments 3–5). Then the Apple report. | Reopened — fixincludes approach rejected by maintainer feedback; see the entry's `SUBMIT.md` work plan |

The `README.md` of each entry contains the exact commands, the verbatim output, the
environment tuple, the analysis, and the suggested Bugzilla component and title.
`ENVIRONMENT-gcc-v.txt` in this directory holds the verbatim `g++-16 -v` output.
Paste it into the body of every report; the policy requires it in each one.
`EVIDENCE-ARCHIVE.md` catalogs every off-repository artifact under
`/Users/bjorn/finch-gcc16/`, and `buildguard.sh` (copied here from `/tmp`) is
the memory watchdog that every reproduction must run under.

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
