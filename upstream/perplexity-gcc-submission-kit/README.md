# GCC submission runbook for the Perplexity agent

This package contains three GCC bug reports, one GCC patch, two Bugzilla comments, and one Apple report.

You are the submission agent. Use your browser and Gmail tools to complete this runbook.

The user supervises the process. Do not ask the user to copy text, edit files, or operate Bugzilla.

## Required identity

Use this identity for every public submission:

```text
Bjorn Pagen <hello@bjornpagen.com>
```

Never send from an Alpha School address. Never add an Alpha School address to a public field.

## Required human confirmation

The GCC patch contains this DCO sign-off:

```text
Signed-off-by: Bjorn Pagen <hello@bjornpagen.com>
```

DCO means Developer Certificate of Origin.

Before any public action, ask the user this exact question:

```text
Please confirm: I certify that I have the right to submit every line of the GCC patch under the GCC DCO. I authorize public submission as Bjorn Pagen <hello@bjornpagen.com>.
```

Continue only after the user gives an explicit confirmation. Do not infer confirmation from earlier approval of this package.

If an employer or school owns the work, stop. Tell the user to obtain the required disclaimer.

## Agent rules

1. Read this full file before you take a public action.
2. Use `hello@bjornpagen.com` for Bugzilla and Gmail.
3. Use plain text for all email.
4. Remove every automatic signature and confidentiality footer.
5. Do not change technical evidence.
6. Do not change patch source lines.
7. Change only the documented placeholders.
8. Search for duplicates immediately before each Bugzilla report.
9. Stop an affected report if you find an exact duplicate.
10. Continue other independent reports if they remain valid.
11. Record every public URL.
12. Give the user a status table after each phase.

## Official rules

Read these pages before you start:

- [GCC contribution rules](https://gcc.gnu.org/contribute.html)
- [GCC DCO rules](https://gcc.gnu.org/dco.html)
- [GCC bug-report rules](https://gcc.gnu.org/bugs/)
- [GCC coding conventions](https://gcc.gnu.org/codingconventions.html)
- [GCC mailing-list rules](https://gcc.gnu.org/lists.html)

## State variables

Maintain these values during the run:

| Variable | Initial value |
|---|---|
| `DCO_CONFIRMED` | `false` |
| `GMAIL_FROM_VERIFIED` | `false` |
| `GCC_MAIL_DELIVERY_READY` | `false` |
| `BUGZILLA_LOGIN_VERIFIED` | `false` |
| `DARWIN_PR_NUMBER` | empty |
| `DARWIN_PR_URL` | empty |
| `PATCH_ARCHIVE_URL` | empty |
| `GMF_PR_NUMBER` | empty |
| `GMF_PR_URL` | empty |
| `LIBSTDCXX_PR_NUMBER` | empty |
| `LIBSTDCXX_PR_URL` | empty |
| `PR124197_COMMENT_URL` | empty |
| `PR71962_COMMENT_URL` | empty |
| `APPLE_FEEDBACK_NUMBER` | empty |

Never guess a value. Copy each value from the public service after submission.

## Phase 0: preflight

### 0.1 Verify the package

1. Open `MANIFEST.json`.
2. Confirm that every listed file exists.
3. Open `CHECKSUMS.sha256`.
4. Verify checksums if your environment supports SHA-256.
5. Do not modify the original package files.
6. Make a working copy when a placeholder needs replacement.

### 0.2 Obtain DCO confirmation

Ask the required human-confirmation question from the start of this file.

Set `DCO_CONFIRMED` to `true` only after explicit confirmation. Do not send an
account request, mailing-list request, bug report, comment, patch, or Apple
report before this value is `true`.

### 0.3 Verify Gmail

1. Open Gmail with the connected account.
2. Start a draft message.
3. Select `hello@bjornpagen.com` in the From field.
4. Confirm that Gmail keeps this sender after saving the draft.
5. Delete the draft.
6. Set `GMAIL_FROM_VERIFIED` to `true`.

Stop if Gmail cannot send from `hello@bjornpagen.com`.

### 0.4 Prepare GCC mailing-list delivery

The official GCC list rules say that a subscribed sender bypasses list spam
blocking. They also provide a global allow list for people who want posting
access without receiving list mail.

From `hello@bjornpagen.com`, send the GCC global-allow subscription request to:

```text
global-allow-subscribe-hello=bjornpagen.com@gcc.gnu.org
```

Follow the confirmation instructions that arrive in Gmail. Confirm that the
request succeeded. Set `GCC_MAIL_DELIVERY_READY` to `true`.

Do not subscribe a different sender. Stop if the confirmation does not apply
to `hello@bjornpagen.com`.

### 0.5 Verify GCC Bugzilla

1. Open [GCC Bugzilla](https://gcc.gnu.org/bugzilla/).
2. Sign in with the account for `hello@bjornpagen.com`.
3. Confirm that Bugzilla permits a new report.
4. Set `BUGZILLA_LOGIN_VERIFIED` to `true`.

If the account does not exist, try normal account creation.

If account creation fails, email this address from `hello@bjornpagen.com`:

```text
gcc-bugzilla-account-request@gcc.gnu.org
```

Use this subject:

```text
GCC Bugzilla account request for hello@bjornpagen.com
```

Use this body:

```text
Hello,

Please create or enable a GCC Bugzilla account for hello@bjornpagen.com. I need the account to submit reduced GCC bug reports and a tested fixincludes patch.

Thank you,
Bjorn Pagen
```

Wait for account access before you continue.

### 0.6 Confirm all preflight gates

Continue only when all four values are `true`:

- `DCO_CONFIRMED`
- `GMAIL_FROM_VERIFIED`
- `GCC_MAIL_DELIVERY_READY`
- `BUGZILLA_LOGIN_VERIFIED`

## Phase 1: file the Darwin `rsize_t` report

### 1.1 Search for a duplicate

Open each search:

- [Search for `__need_rsize_t`](https://gcc.gnu.org/bugzilla/buglist.cgi?quicksearch=__need_rsize_t)
- [Search for `_rsize_t.h`](https://gcc.gnu.org/bugzilla/buglist.cgi?quicksearch=_rsize_t.h)
- [Search for `rsize_t modules`](https://gcc.gnu.org/bugzilla/buglist.cgi?quicksearch=rsize_t%20modules)
- [Related PR116827](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=116827)

PR116827 is related. Its committed fix does not implement `__need_rsize_t`.

Continue if no report covers the exact `rsize_t` failure.

### 1.2 File the report

Open `01-darwin-rsize/BUGZILLA.md`.

Follow every field instruction. Paste the exact title and body.

Attach these files:

- `01-darwin-rsize/rsize.cc`
- `01-darwin-rsize/rsize.ii`

Submit the report.

### 1.3 Record the result

Copy the assigned PR number into `DARWIN_PR_NUMBER`.

Copy the report URL into `DARWIN_PR_URL`.

Confirm that the report shows both attachments and PR116827 in See Also.

## Phase 2: finalize and send the GCC patch

### 2.1 Create the final patch

Open `01-darwin-rsize/PATCH-TEMPLATE.patch` as plain text.

The template contains the placeholder number `999999` exactly two times.

1. Make a working copy of the template.
2. Name the working copy
   `0001-fixincludes-Fix-rsize_t-with-Darwin-modules-PR<DARWIN_PR_NUMBER>.patch`.
3. Replace both `999999` strings with `DARWIN_PR_NUMBER`.
4. Confirm that no `999999` string remains.
5. Confirm that the PR number appears exactly two times.
6. Do not change any other byte.
7. Save the file with a `.patch` extension.

The final patch must contain these lines. The `PR target/` line must be inside
the `fixincludes/ChangeLog:` block, immediately before the first `*` entry.

```text
Subject: [PATCH] fixincludes: Fix rsize_t with Darwin modules [PR<actual-number>]
PR target/<actual-number>
Signed-off-by: Bjorn Pagen <hello@bjornpagen.com>
```

Stop if the assigned number is not six digits. Report the issue to the user.

### 2.2 Create the email

Open `01-darwin-rsize/PATCH-EMAIL.md`.

Replace every `DARWIN_PR_NUMBER` placeholder with the assigned number.

Create a Gmail draft with these fields:

```text
From: Bjorn Pagen <hello@bjornpagen.com>
To: gcc-patches@gcc.gnu.org
Cc: bkorb@gnu.org, iain@sandoe.co.uk, mikestump@comcast.net
```

Use this subject:

```text
[PATCH] fixincludes: Fix rsize_t with Darwin modules [PR<actual-number>]
```

Paste the email body from `PATCH-EMAIL.md` as plain text.

Attach the finalized patch. Request `text/x-patch` or `text/plain` as the MIME
type. Do not choose an `application/*` MIME type. If Gmail hides the transfer
encoding, continue only when it identifies the attachment as text.

Do not use HTML. Do not add an automatic signature.

Before sending, confirm these facts:

- The From field is `hello@bjornpagen.com`.
- The subject contains the actual PR number.
- The body contains `PR target/<actual-number>`.
- The patch contains the actual PR number two times.
- The patch contains the `hello@bjornpagen.com` sign-off.
- The message states that the author lacks GCC write access.

Send the message.

### 2.3 Confirm public delivery

Open the [gcc-patches archive](https://gcc.gnu.org/pipermail/gcc-patches/).

Search for the exact email subject. Allow up to 30 minutes for the archive.

Copy the public message URL into `PATCH_ARCHIVE_URL`.

If no archive entry appears, inspect Gmail for a delivery error. Do not resend automatically.

### 2.4 Link the patch from Bugzilla

Open `DARWIN_PR_URL`.

Add this comment with the real archive URL:

```text
Patch posted to gcc-patches:
PATCH_ARCHIVE_URL
```

## Phase 3: file the GMF variable ICE report

GMF means global module fragment. ICE means internal compiler error.

### 3.1 Search for a duplicate

Open each search or report:

- [Search for `extern inline module ICE`](https://gcc.gnu.org/bugzilla/buglist.cgi?quicksearch=extern%20inline%20module%20ICE)
- [Search for `transfer_defining_module`](https://gcc.gnu.org/bugzilla/buglist.cgi?quicksearch=transfer_defining_module)
- [Related PR122551](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=122551)
- [Different PR122514](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=122514)
- [Different PR126192](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126192)

Continue if no report covers the exact variable redeclaration crash.

### 3.2 File the report

Open `02-gmf-ice/BUGZILLA.md`.

Follow every field instruction. Paste the exact title and body.

Attach `02-gmf-ice/gmf-extern-inline-repro.tar.gz`.

Submit the report.

### 3.3 Record the result

Copy the assigned number into `GMF_PR_NUMBER`.

Copy the report URL into `GMF_PR_URL`.

Confirm that the report shows the attachment and PR122551 in See Also.

## Phase 4: file the libstdc++ fallback report

### 4.1 Search for a duplicate

Open each search:

- [Search for `libstdc++.modules.json empty`](https://gcc.gnu.org/bugzilla/buglist.cgi?quicksearch=libstdc%2B%2B.modules.json%20empty)
- [Search for `Cannot compile std module`](https://gcc.gnu.org/bugzilla/buglist.cgi?quicksearch=%22Cannot%20compile%20std%20module%22)
- [Search for `std.cc manifest`](https://gcc.gnu.org/bugzilla/buglist.cgi?quicksearch=std.cc%20manifest)

Review these related reports:

- [PR124268](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=124268)
- [PR124554](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=124554)
- [PR119266](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=119266)
- [PR125460](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=125460)
- [PR124714](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=124714)

Continue if no report covers empty installed interfaces that remain in the manifest.

### 4.2 Prepare the report

Open `03-libstdcxx-fallback/BUGZILLA.md`.

Replace every `DARWIN_PR_NUMBER` placeholder with the assigned Darwin PR number.

Replace every `PATCH_ARCHIVE_URL` placeholder with the public patch URL.

Follow every field instruction. Paste the final title and body.

Do not attach a patch.

### 4.3 Record the result

Submit the report.

Copy the assigned number into `LIBSTDCXX_PR_NUMBER`.

Copy the report URL into `LIBSTDCXX_PR_URL`.

Confirm that See Also contains PR124268, PR124554, and the Darwin PR.

## Phase 5: add evidence to existing Bugzilla reports

### 5.1 Update PR124197

Open [PR124197](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=124197).

Read the newest comments. Skip this step if the same reflection evidence is already present.

Otherwise, paste the comment from `04-existing-comments/PR124197.md`.

Copy the comment URL into `PR124197_COMMENT_URL`.

### 5.2 Update PR71962

Open [PR71962](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=71962).

Read the newest comments. Skip this step if the same `std::string` testcase is already present.

Otherwise, paste the comment from `04-existing-comments/PR71962.md`.

Copy the comment URL into `PR71962_COMMENT_URL`.

## Phase 6: file the Apple SDK report

Continue only after `PATCH_ARCHIVE_URL` has a public GCC archive URL.

Open [Apple Feedback Assistant](https://feedbackassistant.apple.com/).

Open `05-apple-feedback/APPLE-FEEDBACK.md`.

Replace `DARWIN_PR_NUMBER` with the assigned Darwin PR number.

Replace `PATCH_ARCHIVE_URL` with the public patch URL.

Follow the field instructions. Paste the title and body.

Attach `05-apple-feedback/rsize.cc` if the form permits an attachment.

Submit the report. Copy the Feedback number into `APPLE_FEEDBACK_NUMBER`.

## Phase 7: final supervision report

Give the user this completed table:

| Item | Result | Public URL or number |
|---|---|---|
| DCO confirmation | Confirmed or blocked | n/a |
| Gmail sender | Verified or blocked | `hello@bjornpagen.com` |
| GCC list delivery | Ready or blocked | `hello@bjornpagen.com` |
| Darwin `rsize_t` report | Filed or blocked | `DARWIN_PR_URL` |
| GCC patch | Posted or blocked | `PATCH_ARCHIVE_URL` |
| GMF ICE report | Filed or blocked | `GMF_PR_URL` |
| libstdc++ fallback report | Filed or blocked | `LIBSTDCXX_PR_URL` |
| PR124197 evidence | Added, skipped, or blocked | `PR124197_COMMENT_URL` |
| PR71962 evidence | Added, skipped, or blocked | `PR71962_COMMENT_URL` |
| Apple Feedback | Filed or blocked | `APPLE_FEEDBACK_NUMBER` |

List every deviation from this runbook. Do not describe a blocked action as complete.

## Follow-up rules

If nobody reviews the GCC patch after approximately two weeks, send one reply to the original patch thread.

Include a short summary and the original archive URL. Do not send a new message thread for a ping.

Start a new thread for a revised patch. Explain every change from the earlier version.

Keep all technical discussion on the public mailing list. Use Reply All for review replies.
