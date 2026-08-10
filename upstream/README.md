# GCC submission queue

File three new Bugzilla reports. Send one GCC patch after Bugzilla creates the first report.

DCO means Developer Certificate of Origin. ICE means internal compiler error. GMF means global module fragment.

## Before you start

1. Obtain a GCC Bugzilla account.
2. Use `gcc-bugzilla-account-request@gcc.gnu.org` if account creation fails.
3. Use `Bjorn Pagen <bjorn.pagen@alpha.school>` for the patch submission.
4. Read the [GCC DCO](https://gcc.gnu.org/dco.html).
5. Confirm that you have the right to submit every patch line.
6. Obtain an employer or school disclaimer if another organization owns the work.

The public mailing-list archive keeps your name, email address, and message permanently.

## Send order

### 1. File the Darwin `rsize_t` report

Open [`gcc-fixincludes-darwin-rsize-t/SUBMIT.md`](gcc-fixincludes-darwin-rsize-t/SUBMIT.md).

Follow its Bugzilla steps. Bugzilla will assign a number such as `PR123456`.

### 2. Add the Darwin PR number to the patch

Do not send the current patch before this step.

1. Add `[PR123456]` to the GCC commit subject.
2. Add `PR target/123456` above the ChangeLog entry.
3. Keep the `Signed-off-by` line.
4. Generate a new format patch.
5. Run the GCC commit checker.
6. Run the GCC style checker.
7. Apply the patch to a clean current-master worktree.

The Darwin submission file gives the exact text and recipients.

### 3. Send the Darwin patch

Send the patch to `gcc-patches@gcc.gnu.org`.

Use plain text or a `text/x-patch` attachment. State that you do not have GCC write access.

### 4. File the GMF variable ICE report

Open [`gcc-ice-gmf-consteval-redecl/SUBMIT.md`](gcc-ice-gmf-consteval-redecl/SUBMIT.md).

Follow its Bugzilla steps. Attach the small two-file reproduction archive.

### 5. File the libstdc++ fallback report

Open [`libstdcxx-silent-empty-std-module/SUBMIT.md`](libstdcxx-silent-empty-std-module/SUBMIT.md).

Replace `PR target/NNNNN` with the Darwin PR number. Then paste the report into Bugzilla.

### 6. Add the two existing Bugzilla comments

Open [`bugzilla-comments/SUBMIT.md`](bugzilla-comments/SUBMIT.md).

Add one comment to PR124197. Add one comment to PR71962.

### 7. File the Apple report

Wait until the GCC patch appears in the public archive.

Open [`gcc-fixincludes-darwin-rsize-t/APPLE-FEEDBACK.md`](gcc-fixincludes-darwin-rsize-t/APPLE-FEEDBACK.md).

## Queue summary

| Directory | Action | Status |
|---|---|---|
| `gcc-fixincludes-darwin-rsize-t` | File one `target` report. Send one fixincludes patch. | Ready for Bugzilla |
| `gcc-ice-gmf-consteval-redecl` | File one `c++` report. | Ready |
| `libstdcxx-silent-empty-std-module` | File one `libstdc++` report. | Ready after the Darwin PR |
| `bugzilla-comments` | Add comments to two existing reports. | Ready |

## Verified patch status

- The patch changes one fixincludes rule.
- The patch includes the generated `fixincl.x` file.
- The patch includes a fixincludes test fixture.
- `fixincludes make check` passed.
- The real SDK test passed.
- The second fixincludes run made no change.
- The GCC commit checker passed.
- The source diff applies to current GCC master.
- Both default-language bootstraps completed.
- Both full GCC test commands completed.

The full test comparison contained machine-load differences. It contained no baseline PASS that became a patched FAIL.

The baseline analyzer plug-in timed out. The patched run reached the related analyzer tests.

We reran that test group in both trees. Each tree produced 28 PASS results and 5 expected XFAIL results.

Use this exact test statement in the patch email. Do not claim that both full result sets were identical.

## Policy sources

- [GCC contribution and test rules](https://gcc.gnu.org/contribute.html)
- [GCC DCO rules](https://gcc.gnu.org/dco.html)
- [GCC bug-report rules](https://gcc.gnu.org/bugs/)
- [GCC coding conventions](https://gcc.gnu.org/codingconventions.html)
- [GCC mailing-list rules](https://gcc.gnu.org/lists.html)

Send one polite ping after approximately two weeks if nobody replies. Reply to the original patch thread.

Start a new thread for a revised patch. State each change from the earlier version.
