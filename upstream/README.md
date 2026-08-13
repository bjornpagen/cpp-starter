# GCC upstream work

This directory records GCC defects this project filed or depends on, and the
one live patch investigation.

Use these files as the control documents:

- [`BUGS.md`](BUGS.md) is the canonical ledger. Open issues are Bugzilla
  pointers. Do not recreate deleted reproduction trees.
- [`TODO.md`](TODO.md) lists current work and its prerequisites.
- [`SUBMISSION-CHECKLIST.md`](SUBMISSION-CHECKLIST.md) defines the process for
  new reports, comments, and patches.
- [`EVIDENCE-ARCHIVE.md`](EVIDENCE-ARCHIVE.md) locates off-repository
  evidence.
- `ENVIRONMENT-gcc-v.txt` contains the recorded GCC 16.1.0 configuration.
- [`gcc-fixincludes-darwin-rsize-t/`](gcc-fixincludes-darwin-rsize-t/) is the
  only in-tree investigation: PR 126782, `__need_rsize_t`. Start at
  [`LOCAL-AGENT-HANDOFF.md`](gcc-fixincludes-darwin-rsize-t/LOCAL-AGENT-HANDOFF.md).

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
