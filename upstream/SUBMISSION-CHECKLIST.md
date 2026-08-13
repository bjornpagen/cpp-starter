# GCC submission checklist

Read the current GCC policy before each submission. Current policy overrides
this file.

Sources:

- https://gcc.gnu.org/bugs/
- https://gcc.gnu.org/bugs/minimize.html
- https://gcc.gnu.org/bugs/management.html
- https://gcc.gnu.org/contribute.html
- https://gcc.gnu.org/codingconventions.html
- https://gcc.gnu.org/dco.html
- https://gcc.gnu.org/lists.html
- https://gcc.gnu.org/develop.html

## Establish the facts

- Record the exact compiler version and complete `gcc -v` output.
- Record build, host, and target separately. Do not infer missing values.
- Record the complete command and compiler output.
- State expected and actual behavior.
- State the language and standard mode.
- Confirm that the input is valid.
- Record each executed Known to Work and Known to Fail version.
- Do not convert source inspection into a tested-version claim.

## Check documented scope

- Read the manual for the option, feature, target, and runtime.
- Record each documented support boundary.
- Do not infer supported semantics from an unsupported target.
- Stop an unsupported-target report unless the behavior violates a separate
  documented guarantee or causes an independent ICE, wrong-code result, data
  loss, or uncontrollable diagnostic.
- If the defect reproduces on a supported target, use that reproduction as the
  canonical report.
- Put unsupported-target evidence last under `Additional observation:`, or
  omit it.

## Exclude user and build errors

- Check GCC's current non-bugs and the relevant porting guide.
- Identify unofficial or vendor builds.
- Reproduce with an official build when practical.
- Run `-Wall -Wextra`.
- Use relevant controls for wrong-code, sanitizer, C++, PCH, and optimization
  claims.
- Record checks that did not run.

## Search before filing

- Search the exact diagnostic.
- Search the failing function or internal symbol.
- Search the language feature, header, library, and target.
- Open the nearest reports and state why they differ.
- Test a current release and trunk or a current snapshot when practical.
- Stop when an exact duplicate exists.
- Add evidence to an existing report only when the evidence is new.

## Prepare the testcase

- Prefer one self-contained source file under approximately 30 lines.
- Otherwise, attach one preprocessed `.i`, `.ii`, or `.f` file.
- Use an archive only when the defect requires multiple files.
- Generate preprocessed source with the complete command and `-save-temps`.
- State the reason when no preprocessed source is attached.
- Do not use a repository, package URL, CI job, or external archive as the
  reproducer.
- Do not attach objects, executables, assembly, core files, or PCH files.

## Prepare Bugzilla fields

- Select Product, Component, Version, Severity, and valid keywords.
- Set Known to Fail only for executed failing compilers.
- Set Known to Work only for executed passing compilers.
- Set Build, Host, and Target only from explicit evidence.
- Leave priority at P3.
- Leave the target milestone, assignee, and CC list unchanged.
- Use `ice-on-valid-code` only for an ICE on valid input.

## Write the report

- Write plain text.
- Start with the observed behavior, expected behavior, and smallest trigger.
- Present the testcase, command, output, versions, and environment in that
  order.
- Keep private analysis and competing fix designs out of Bugzilla.
- Do not use Markdown tables, headings, emphasis, code fences, or HTML.
- Do not claim that an unrun check passed.
- Do not use private employer, customer, or school information.

## Approve and submit

- Show the exact summary, fields, body, and attachments.
- Show attachment names, MIME types, sizes, and hashes when practical.
- Record duplicate searches, tested versions, missing checks, and deviations.
- Get approval for the exact public artifact.
- Create the report before uploading attachments.
- Reload the report and confirm that Comment 0 contains the complete body.
- Stop before uploads if Comment 0 is incomplete.
- Upload attachments through a binary-safe path.
- Fetch the stored bytes and verify their size and hash.
- Do not trust the compact attachment-table size.
- Do not add a cosmetic correction comment.

## After submission

- Add the report to [`BUGS.md`](BUGS.md) as a Bugzilla pointer.
- Keep an investigation directory only when this repository is producing a
  patch. Today that is `gcc-fixincludes-darwin-rsize-t/` for PR 126782.
- Record automatic Bugzilla results, such as the component assignee.
- Record the attachment IDs and any obsolete replacements.
- Recheck the report and activity log without changing them.
- Update the ledger after each maintainer response or status change.

## Patch gate

- Keep Bugzilla reporting and patch review separate.
- Confirm the legal contribution route before adding a sign-off.
- Include one regression test unless a clear reason prevents it.
- Test the exact patch that will be sent.
- Run GCC style, ChangeLog, whitespace, apply, and testsuite checks.
- Send patches through the current GCC mailing-list route.
- Confirm delivery in the public archive.
- Never describe an unreviewed Bugzilla attachment as an accepted patch.
