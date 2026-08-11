# GCC submission queue

Five entries, all verified and ready to file. The previous queue (Darwin `rsize_t`
fixincludes, GMF variable ICE, silent empty `std` module, two Bugzilla comments) was
submitted on 2026-08-10 and removed from the tree; recover that material from git
history before commit `e0a9d27` if a maintainer asks. The one retained artifact is
`gcc-fixincludes-darwin-rsize-t/fixed-header.h`, the live local fixinclude referenced
by `PINS.md` until the upstream patch lands.

DCO means Developer Certificate of Origin. ICE means internal compiler error. GMF means
global module fragment.

## Before you start

1. Use your existing GCC Bugzilla account.
2. Use `Bjorn Pagen <hello@bjornpagen.com>` for every public submission.
3. Read the [GCC DCO](https://gcc.gnu.org/dco.html) before sending any patch.
4. Confirm that you have the right to submit every patch line.

The public mailing-list archive keeps your name, email address, and message permanently.

## Queue summary

| Directory | Action | Status |
|---|---|---|
| `gcc-lto-modules-debug-oom` | File one report against the Darwin target side of debug emission. File one Apple Feedback (dsymutil) after the GCC PR exists. | Ready — Linux control done: ELF fat LTO objects verify clean, Mach-O only |
| `gcc-darwin-fhardened-coverage` | File one `middle-end` report. Send one `configure.ac` Darwin-enablement patch after. | Ready — the umbrella is Linux-only by GCC's own configure |
| `gcc-analyzer-call-summary-ice` | File one `analyzer` report. | Ready — the 21-line reduction ICEs on Linux too; cross-platform |
| `gcc-analyzer-fd-leak-raii-fp` | File two `analyzer` reports. | Ready — both false positives reproduce verbatim on Linux |
| `gcc-modules-freflection-typedef-merge` | File one `c++` report (`rejects-valid`, See Also PR 122785). | Ready — 6-line reduction, fails against both glibc and Apple SDK headers |

Each entry's `README.md` carries the exact commands, verbatim output, environment
tuple, analysis, and suggested Bugzilla component and title.

DANGER, `gcc-lto-modules-debug-oom` only: run its reproduction exclusively under the
memory guard documented in the entry. An unguarded run of the archive-link case grows
without bound and once kernel-panicked the development machine.

## Policy sources

- [GCC contribution and test rules](https://gcc.gnu.org/contribute.html)
- [GCC DCO rules](https://gcc.gnu.org/dco.html)
- [GCC bug-report rules](https://gcc.gnu.org/bugs/)
- [GCC coding conventions](https://gcc.gnu.org/codingconventions.html)
- [GCC mailing-list rules](https://gcc.gnu.org/lists.html)

Send one polite ping after approximately two weeks if nobody replies. Reply to the
original patch thread. Start a new thread for a revised patch and state each change
from the earlier version.
