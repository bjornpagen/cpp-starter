# GCC upstream work

This directory records GCC defects, reduced testcases, evidence, and follow-up
work from the C++ starter project.

Use these files as the control documents:

- [`BUGS.md`](BUGS.md) is the canonical ledger for public reports and upstream
  outcomes.
- [`TODO.md`](TODO.md) lists current work and its prerequisites.
- [`SUBMISSION-CHECKLIST.md`](SUBMISSION-CHECKLIST.md) defines the process for
  new reports, comments, and patches.
- [`EVIDENCE-ARCHIVE.md`](EVIDENCE-ARCHIVE.md) locates large or machine-specific
  evidence that does not belong in Git.
- `ENVIRONMENT-gcc-v.txt` contains the recorded GCC 16.1.0 configuration.
- [`gcc-fixincludes-darwin-rsize-t/LOCAL-AGENT-HANDOFF.md`](gcc-fixincludes-darwin-rsize-t/LOCAL-AGENT-HANDOFF.md)
  is the local GCC trunk patch handoff for PR 126782.

## Investigation index

| Directory | Public result | Local state |
|---|---|---|
| `gcc-fixincludes-darwin-rsize-t` | GCC PR 126782 | Patch investigation |
| `gcc-modules-gmf-inline-variable-ice` | GCC PR 126783 | Assigned upstream |
| `libstdcxx-empty-module-fallback` | GCC PR 126786 | Awaiting triage |
| `gcc-analyzer-call-summary-ice` | GCC PR 126805 | Awaiting triage |
| `gcc-analyzer-fd-leak-raii-fp` | GCC PR 126806 and PR 126819 | Awaiting triage |
| `gcc-darwin-fhardened-coverage` | GCC PR 126822 and PR 126823 | One active; one closed locally |
| `gcc-lto-modules-debug-oom` | Evidence for GCC PR 82005 and LLVM issue 102965 | Comment work not submitted |
| `gcc-modules-freflection-typedef-merge` | Existing GCC PR 124582 | Fixed upstream; retired |

The ledger separates the Bugzilla status from the local state. For example,
PR 126823 remains UNCONFIRMED in Bugzilla, but the local state is
`closed-no-action` because a maintainer said that the behavior works as
designed.

## Safety rule

Do not run the `gcc-lto-modules-debug-oom` reproduction without the documented
memory and time limits. An unguarded archive-link run caused a kernel panic.
The `dsymutil` process can outlive the compiler driver, so the guard must stop
the complete process tree.

## Public submission rule

Local research is reversible. Public reports, comments, attachments, emails,
and pushes are not. Prepare the complete artifact first. Then get approval for
the exact public values.

Read the current GCC policy before each new submission:

- [Bug reporting](https://gcc.gnu.org/bugs/)
- [Bug management](https://gcc.gnu.org/bugs/management.html)
- [Contribution rules](https://gcc.gnu.org/contribute.html)
- [DCO rules](https://gcc.gnu.org/dco.html)
- [Coding conventions](https://gcc.gnu.org/codingconventions.html)
- [Mailing-list rules](https://gcc.gnu.org/lists.html)
