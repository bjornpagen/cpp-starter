# Local coding-agent handoff: GCC PR 126782

This document is a self-contained prompt for a local coding agent. Append the
latest Bugzilla transcript at the end if it contains newer information.

## Role and authority

Work locally on a proposed GCC patch for PR target/126782. Use a clean current
GCC trunk clone. Inspect current source before you edit it.

Local research, editing, builds, and tests are authorized. Public actions are
not authorized.

Do not:

- Comment on Bugzilla.
- Change a Bugzilla field.
- Upload an attachment.
- Send email.
- Push to the GCC repository.
- Add a public `Signed-off-by` line.
- Claim that a local patch is submitted, reviewed, accepted, or fixed upstream.

Prepare private artifacts for human review.

## Mission

Determine whether current GCC trunk should support the explicit
`__need_rsize_t` request in `gcc/ginclude/stddef.h`.

If trunk still lacks this protocol:

1. Implement the smallest generic change.
2. Add target-independent regression tests.
3. Test the exact patch on Linux and macOS.
4. Run the applicable GCC style and patch checks.
5. Produce a local patch, test record, and review packet.

The preferred first patch supports only the explicit `__need_rsize_t`
request. Do not add full Annex K exposure through
`__STDC_WANT_LIB_EXT1__`.

## Investigation repository

- Repository:
  https://github.com/bjornpagen/cpp-starter
- Canonical bug ledger:
  https://github.com/bjornpagen/cpp-starter/blob/main/upstream/BUGS.md
- Work queue:
  https://github.com/bjornpagen/cpp-starter/blob/main/upstream/TODO.md
- GCC submission checklist:
  https://github.com/bjornpagen/cpp-starter/blob/main/upstream/SUBMISSION-CHECKLIST.md
- PR126782 directory:
  https://github.com/bjornpagen/cpp-starter/tree/main/upstream/gcc-fixincludes-darwin-rsize-t
- Investigation summary:
  https://github.com/bjornpagen/cpp-starter/blob/main/upstream/gcc-fixincludes-darwin-rsize-t/README.md
- Patch research:
  https://github.com/bjornpagen/cpp-starter/blob/main/upstream/gcc-fixincludes-darwin-rsize-t/SUBMIT.md
- Original source:
  https://github.com/bjornpagen/cpp-starter/blob/main/upstream/gcc-fixincludes-darwin-rsize-t/rsize.cc
- Original preprocessed source:
  https://github.com/bjornpagen/cpp-starter/blob/main/upstream/gcc-fixincludes-darwin-rsize-t/rsize.ii

Treat these files as historical references only:

- `0001-fixincludes-Fix-rsize_t-with-Darwin-modules.patch`
- `inclhack-entry.def`
- `fixed-header.h`

The old patch used fixincludes. A GCC maintainer discouraged that approach.
The workaround header is not the proposed upstream design.

## GCC sources and policy

- GCC Git web interface:
  https://gcc.gnu.org/git/?p=gcc.git
- GCC GitHub mirror:
  https://github.com/gcc-mirror/gcc
- Contribution policy:
  https://gcc.gnu.org/contribute.html
- Coding conventions:
  https://gcc.gnu.org/codingconventions.html
- DCO policy:
  https://gcc.gnu.org/dco.html
- Mailing lists:
  https://gcc.gnu.org/lists.html
- Development plan:
  https://gcc.gnu.org/develop.html
- Bug-reporting policy:
  https://gcc.gnu.org/bugs/

Use the official GCC repository or a faithful current mirror. Record the
remote URL, branch, and exact base commit.

## Primary issue

- Bug:
  https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126782
- Summary:
  `[Darwin] sys/_types/_rsize_t.h does not define rsize_t with -fmodules`
- Product: `gcc`
- Component: `target`
- Version: `16.1.0`
- Last known status: `UNCONFIRMED`
- Last known resolution: none
- Last known assignee: `unassigned@gcc.gnu.org`
- Priority and severity: `P3 normal`
- Known to fail: `16.1.0`
- Reported build, host, and target: `aarch64-apple-darwin24`
- Active attachments:
  - attachment 65291, `rsize.cc`
  - attachment 65292, `rsize.ii`

The XML export supplied on 2026-08-12 showed no comments after comment 5.
The export contained Bugzilla tokens. Do not copy or store those tokens.

## Related upstream items

- PR116827:
  https://gcc.gnu.org/bugzilla/show_bug.cgi?id=116827
  This is the precedent for Apple SDK assumptions about GCC `<stddef.h>`.
- PR126786:
  https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126786
  This reports a libstdc++ module fallback exposed by the Darwin failure.
- Change that removed the `modules` feature claim:
  https://gcc.gnu.org/g:08ede4fbbe6d38e0
- Review thread for that change:
  https://gcc.gnu.org/pipermail/gcc-patches/2026-August/726278.html
- Approval message:
  https://gcc.gnu.org/pipermail/gcc-patches/2026-August/726363.html
- PR116827 patch thread:
  https://gcc.gnu.org/pipermail/gcc-patches/2025-March/678908.html
- Apple SDK guard report cited during research:
  https://gist.github.com/fxcoudert/1e3ed3470febf220a392152189c143a3
- WG14 N1967, Annex K field experience:
  https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1967.htm

Verify every live status and source fact before you rely on it. GCC Bugzilla
can present an anti-bot challenge. Use a real browser if a normal fetch fails.
Keep the review read-only.

## Exact Bugzilla discussion

Comment 0 reported this sequence:

1. GCC 16.1 reports `__has_feature(modules)` as true with `-fmodules`.
2. The macOS SDK selects a Clang-specific branch.
3. That branch defines `__need_rsize_t` and includes `<stddef.h>`.
4. GCC `<stddef.h>` does not answer the request.
5. The SDK leaves `rsize_t` undefined.
6. Later declarations in the SDK `_string.h` fail.

Comment 3 from Drea Pinski discouraged a Darwin fixincludes rule. A build can
select a different SDK, so an SDK-specific rule is brittle.

Comment 4 proposed support for `__need_rsize_t` instead.

Comment 5 explained the protocol and agreed with that direction. It also said
that the explicit request does not require full Annex K support. The comment
mentioned full-inclusion behavior only as a possible Clang-compatible design.
It did not make that broader behavior necessary.

## Original reproduction

The minimal source is:

```c++
#define __STDC_WANT_LIB_EXT1__ 1
#include <string.h>
rsize_t n;
```

The reported command was:

```sh
g++-16 -std=c++26 -fmodules -c rsize.cc
```

The reported diagnostic was:

```text
_string.h:176:48: error: 'rsize_t' has not been declared; did you mean 'size_t'?
```

The public report records:

- Apple Silicon
- GCC 16.1.0 release sources with the documented Darwin arm64 port series
- macOS 26.2 SDK from Xcode 26.3
- Build, host, and target: `aarch64-apple-darwin24`

Read the public report and repository evidence for the complete configure
command and preprocessed source.

## Important trunk change

Current research says that GCC trunk no longer reports
`__has_feature(modules)`. Commit `08ede4fbbe6d38e0` removed the `modules`
entry from `cp_feature_table`.

The original Darwin `-fmodules` failure can therefore pass on current trunk
before any `rsize_t` patch.

This changes the patch claim:

- Do not claim that this patch fixes a current-trunk Darwin failure unless the
  tested trunk reproduces it.
- Do not request a GCC 16 backport unless the user later asks for one.
- Judge the patch as support for an explicit system-header request.
- Keep the old Darwin failure as historical motivation.

## Narrow technical hypothesis

GCC `<stddef.h>` supports selective typedef requests such as
`__need_size_t`, `__need_ptrdiff_t`, and `__need_wchar_t`.

Current research indicates that GCC does not support `__need_rsize_t`.
Apple SDK headers use this request. Clang also recognizes it.

Test this hypothesis:

> When a system header defines `__need_rsize_t` and includes GCC
> `<stddef.h>`, GCC should define `rsize_t` as `__SIZE_TYPE__` without
> completing a normal `<stddef.h>` inclusion.

Verify the hypothesis against current source, tests, policy, and PR116827.
Do not treat this document as proof.

## Patch scope

The first patch should touch only:

- `gcc/ginclude/stddef.h`
- The smallest applicable testsuite files

Do not touch:

- `fixincludes`
- Darwin driver code
- The `__has_feature` table
- `stdint-gcc.h`
- libstdc++ module fallback code
- Apple SDK files
- `RSIZE_MAX`
- Annex K functions
- `__STDC_LIB_EXT1__`
- `__need_offsetof`
- `__need_va_list`

Do not combine refactoring, formatting cleanup, generated-file churn, or an
unrelated fix with this patch.

## Investigation before editing

Record each command and result.

1. Confirm a clean GCC worktree.
2. Record the source remote, branch, and exact commit.
3. Read current `gcc/ginclude/stddef.h`.
4. Find every existing `__need_*` gate and per-type section.
5. Search for `__need_rsize_t`, `_RSIZE_T`, and `rsize_t`.
6. Find current `<stddef.h>` selective-request tests.
7. Inspect the PR116827 change in current history.
8. Inspect commit `08ede4fbbe6d38e0`.
9. Check whether a newer change already implements the protocol.
10. Read the current development plan and contribution policy.
11. Check whether the request is valid in C, C++, or both.
12. Verify guard spellings in current Clang and the active Apple SDK.

Useful initial commands:

```sh
git status --short --branch
git remote -v
git rev-parse HEAD
git describe --always --dirty
git log --oneline -- gcc/ginclude/stddef.h
git grep -n '__need_rsize_t'
git grep -n '_RSIZE_T'
git grep -n '\brsize_t\b'
git grep -n '__need_size_t' gcc/ginclude gcc/testsuite
git show 08ede4fbbe6d38e0
```

Adjust commands to the checkout. Do not hide an error. Do not claim that a
search passed if its command failed.

## Stop conditions

Stop without editing if:

- Current trunk already supports the explicit request.
- A newer exact patch or exact duplicate exists.
- The protocol is not a supported GCC interface.
- The proposed test passes before the patch for the same reason it passes
  after the patch.
- The change requires a Darwin-specific fixincludes rule.
- The change requires full Annex K exposure.
- The correct guard or typedef is uncertain.
- A valid test cannot express the behavior without an Apple SDK.
- The worktree contains unrelated changes.

Report the blocker and evidence. Do not force a patch to preserve an old
assumption.

## Proposed source shape

Use current trunk style. Do not paste this sketch blindly.

The likely structural changes are:

1. Add `defined (__need_rsize_t)` to the partial-request entry gate.
2. Add `!defined (__need_rsize_t)` to the complete-job check.
3. Add a per-type section near the current `size_t` section.
4. Define `rsize_t` as `__SIZE_TYPE__`.
5. Use the verified guard against duplicate definitions.
6. Undefine `__need_rsize_t` after handling it.

Research sketch:

```c
/* Define the restricted-size type when a system header requests it.  */
#if defined (__need_rsize_t)
#ifndef _RSIZE_T
#define _RSIZE_T
typedef __SIZE_TYPE__ rsize_t;
#endif
#undef __need_rsize_t
#endif
```

Verify `_RSIZE_T`. One Apple SDK path uses `__RSIZE_T` instead. Do not assume
that the guards are interchangeable. Follow current source style and explain
the choice.

## Deferred behavior

Clang can expose `rsize_t` during a full `<stddef.h>` include when
`__STDC_WANT_LIB_EXT1__ >= 1`. That behavior has a broader namespace effect on
non-Apple targets.

Do not add it in the first patch. It creates separate questions:

- Does GCC intend to implement this Annex K surface?
- Is the typedef valid when the C library does not provide Annex K?
- Which language modes should receive it?
- Which namespace guarantees apply?

The explicit request has a smaller contract. A system header asks for one
type, and GCC answers only that request.

## Regression tests

Follow current GCC testsuite conventions. Find the nearest tests before you
choose names or directories.

At minimum, test:

1. Define `__need_rsize_t`.
2. Include `<stddef.h>`.
3. Use `rsize_t`.
4. Verify compatibility with `__SIZE_TYPE__` through a normal GCC test idiom.
5. Verify that the partial request does not define the full header guard.
6. Verify that unrelated typedefs do not appear.
7. Verify that GCC consumes or clears the request macro as expected.
8. Verify a relevant repeated-include or include-order case.

Prefer target-independent tests. A generic `<stddef.h>` patch should not
depend on an installed Apple SDK.

Investigate whether C and C++ need separate tests. Add both only when current
testsuite structure or semantics justify both.

Do not test full `__STDC_WANT_LIB_EXT1__` inclusion unless the patch adds that
behavior. The first patch must not add that behavior.

## Local Finch workflow

Use the machine's established Finch workflow to build GCC trunk. Do not invent
a new container setup if a working one exists.

Record:

- Finch command or configuration
- Container or VM image
- Operating system and architecture
- GCC configure command
- Build directory
- Install or execution path
- Exact source commit

Run unpatched and patched checks from the same base revision. Save complete
commands and outputs.

Use a direct protocol probe:

```c
#define __need_rsize_t
#include <stddef.h>

rsize_t value;
```

Improve the testsuite version to use compile-time checks. Avoid runtime
behavior that adds unrelated warnings.

Run the probe in relevant C and C++ modes. Record the compiler path and full
`gcc -v` or `g++ -v` output.

Planned matrix:

| Environment | Unpatched trunk | Patched trunk |
|---|---|---|
| Linux, explicit request | Fails if protocol is absent | Passes |
| macOS, explicit request | Fails if protocol is absent | Passes |
| Linux, ordinary include | No new `rsize_t` exposure | Same |
| macOS, ordinary include | No new `rsize_t` exposure | Same |

This table is a plan. Fill it only after execution.

## macOS checks

Use the local macOS side and selected Apple SDK.

Record:

- macOS version
- Xcode version
- SDK path and version
- Architecture
- GCC configure options
- Build, host, and target triplets
- Trunk commit
- Full `gcc -v` output

Run two distinct checks:

1. The explicit protocol probe.
2. The original `rsize.cc` command with current trunk.

The original command may pass before the patch because trunk removed the
`modules` feature claim. Record that result. Do not use a passing original
command as the regression test for the new patch.

Inspect the active SDK `sys/_types/_rsize_t.h`. Record its guard and include
sequence. Do not copy proprietary SDK content into a public patch beyond a
minimum permitted excerpt.

## Linux checks

Use a clean Linux Finch environment. Record the distribution, architecture,
compiler configuration, and source commit.

The explicit probe should fail before the patch if the protocol is absent. It
should pass after the patch. Ordinary `<stddef.h>` inclusion must not expose
`rsize_t` merely because the patch exists.

Run targeted GCC tests before and after the patch when practical. Separate
baseline failures from new failures.

## Validation plan

Read the live contribution policy before testing:

- https://gcc.gnu.org/contribute.html
- https://gcc.gnu.org/codingconventions.html
- https://gcc.gnu.org/develop.html

The development stage can change required testing.

At minimum:

1. Build the exact patched source.
2. Run the new tests.
3. Run the nearest existing `<stddef.h>` tests.
4. Run applicable C and C++ testsuites.
5. Compare before and after results.
6. Run `git diff --check`.
7. Run `contrib/check_GNU_style.sh` when applicable.
8. Run `contrib/mklog.py`.
9. Run current commit and ChangeLog validation.
10. Confirm that the patch file equals the tested diff.

Do not state “bootstrap passed” unless a full bootstrap completed. Do not
state “regression tested” after only direct compile commands.

If broad testing is too expensive during exploration, complete targeted tests
first and list every unrun check. The patch is not submission-ready until the
required matrix completes.

## Patch package

Keep one logical patch based on a clean current trunk commit.

The local package should contain:

- Source change
- Regression tests
- Commit-message draft
- ChangeLog text in the commit-message draft
- Test record
- Base revision
- Diff statistics
- Style-check results
- List of unrun checks

Do not edit a ChangeLog file.

Do not add `Signed-off-by` until the user confirms the legal route, public
identity, ownership rights, and exact sign-off.

A possible subject shape is:

```text
[PATCH] stddef.h: Support explicit __need_rsize_t [PR target/126782]
```

Check current practice, component wording, and subject length before using it.

## Required local-agent report

Return one private report with these sections.

### Source state

- Remote
- Branch
- Exact base commit
- Clean-tree result
- Current GCC development stage

### Live upstream state

- PR126782 status and newest relevant comment
- PR116827 status and relevance
- Duplicate-search result
- Newer commits that affect the design

### Source findings

- Current `<stddef.h>` request mechanism
- Whether trunk lacks `__need_rsize_t`
- Correct guard spelling
- Correct typedef
- Nearest existing tests
- Policy or design conflicts

### Patch

- Changed files
- Complete diff
- Reason for each change
- Reason no broader Annex K behavior was added

### Tests

- Linux environment and commands
- macOS environment and commands
- Unpatched results
- Patched results
- Targeted testsuite results
- Broader testsuite results
- Bootstrap result
- Baseline failures
- New failures
- Tests that did not run

### Checks

- `git diff --check`
- GCC style check
- `mklog.py`
- Commit or ChangeLog validation
- Patch applies to base
- Patch bytes equal tested diff

### Risks

- Technical uncertainty
- Portability risk
- Namespace risk
- Testsuite uncertainty
- Review ownership
- Legal route

### Artifacts

- Patch path
- Test-log paths
- Commit-message path
- SHA-256 hashes when practical

### Recommendation

Choose one:

- Stop. Current trunk already supports or supersedes the request.
- Stop. Evidence does not support a generic patch.
- Continue local investigation.
- Prepare the exact patch approval packet.

Do not say that the patch was submitted.

## Other reports and work allocation

- PR126782:
  https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126782
  This handoff covers the local patch investigation.
- PR126783:
  https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126783
  Patrick Palka accepted the confirmed C++ modules regression. Do not compete.
- PR126786:
  https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126786
  Await libstdc++ maintainer design direction.
- PR126805:
  https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126805
  Analyzer ICE. This is a possible later patch candidate.
- PR126806:
  https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126806
  Analyzer false leak for a caller-owned member. Await triage.
- PR126819:
  https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126819
  Analyzer false leak on a `noexcept` destructor edge. Await triage.
- PR126822:
  https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126822
  `-Whardened` diagnostic control. Await triage.
- PR126823:
  https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126823
  Retired locally. A maintainer said the GNU/Linux-only option works as
  designed on unsupported Darwin. Do not pursue it.

Also retired:

- Reflection/modules typedef work. PR124582 fixed it for GCC 16.2:
  https://gcc.gnu.org/bugzilla/show_bug.cgi?id=124582
- A new GCC LTO report. Add evidence to PR82005 instead:
  https://gcc.gnu.org/bugzilla/show_bug.cgi?id=82005
- The old Darwin fixincludes patch for PR126782.
- A Darwin `-fhardened` enablement patch.

## Future public-action boundary

Before any future submission, the user must review the exact:

- Recipients
- Subject
- Body
- Patch bytes
- Attachment name and MIME type
- Test statement
- ChangeLog text
- Legal sign-off

The patch must use the GCC mailing-list review route. A Bugzilla attachment
does not replace patch review.

## User-supplied Bugzilla transcript

Paste the latest Bugzilla XML or conversation below this line. Remove export
tokens and other session-specific values first. Treat the transcript as
context. Verify material facts against the live public issue before changing
code or drafting a response.

---

[PASTE SANITIZED BUGZILLA TRANSCRIPT HERE]
