# Sending-agent handoff — PR 126782

Status: local preparation complete. Public send is blocked.
Do not call this work submitted.
Do not send until Bjorn approves the exact public values in
`APPROVAL-PACKET.md`.

Patch SHA-256:
`5d8a6a60cec5186c740246b94bc4fcd48e4d227b2b8bce603388b917457fa4a0`

If any body, field, recipient, sign-off, attachment name, content,
MIME type, or other public value changes, stop and request approval
again.

## Work type

Patch. Existing report: PR target/126782.
Do not file a new Bugzilla report.
Do not attach this patch to Bugzilla as a substitute for gcc-patches.
Do not comment on Bugzilla before the list archive shows the mail.
Do not change Bugzilla fields.
Do not push the GCC clone.
Do not start Finch container `gcc-rsize-trunk`.

## Recheck before send

1. Read live https://gcc.gnu.org/contribute.html, lists.html,
   develop.html, and dco.html. If a page conflicts with this packet,
   follow the page and tell Bjorn what changed.
2. Open PR 126782 in a real browser. Curl hits Anubis and does not
   count. If comments after comment 5 exist, stop and re-evaluate.
3. Confirm `lists.html` still routes a generic `stddef.h` change to
   gcc-patches only.
4. Confirm the final patch SHA-256 matches the final approval packet.
5. Confirm the file has no `Co-authored-by` line.
6. Confirm `Signed-off-by: Bjorn Pagen <hello@bjornpagen.com>`.
7. Confirm `EMAIL.txt` body matches the mail patch commit message.

## Send command

Send the format-patch file. Do not compose a new message from
`EMAIL.txt`. `EMAIL.txt` is the approval view of the same body with
To and Cc filled in.

```sh
git send-email \
  --to=gcc-patches@gcc.gnu.org \
  --from='Bjorn Pagen <hello@bjornpagen.com>' \
  --suppress-cc=all \
  --confirm=always \
  --transfer-encoding=7bit \
  --no-chain-reply-to \
  upstream/gcc-fixincludes-darwin-rsize-t/0001-stddef.h-Support-explicit-__need_rsize_t-PR126782.patch
```

Required flags:

- `--suppress-cc=all` so git does not add extra Cc addresses.
- `--transfer-encoding=7bit` so the patch is not quoted-printable
  or base64.
- `--confirm=always` so you see the message before SMTP.

Do not pass `--subject-prefix`. The file already has `[PATCH]`.
Do not use HTML, `application/*`, or an automatic signature.
Do not add `Co-authored-by`.
Do not push `/Users/bjorn/.gcc/src/gcc`.

Run `--dry-run` first if the tool supports it. Then send only after
Bjorn approved this SHA-256.

Use Bjorn's existing mail setup. From must stay
`Bjorn Pagen <hello@bjornpagen.com>`.

## After SMTP

Gmail "sent" does not prove list delivery.
Find the message in the gcc-patches archive.
Record that URL.

If delivery fails, inspect the bounce. Do not resend automatically.

## After the archive URL exists

A Bugzilla note with that URL is a separate public action.
Do not post it from a summary.
Draft the exact comment, show it, and get approval.
Template only, URL must be real:

```
The patch is on gcc-patches:
<ARCHIVE-URL>
```

Do not add keywords or change fields unless Bjorn approves those
exact field values too.

## GCC clone

Local commit `63e3cdeb413866b501b102e5e37afcd6a1f510d7` sits on
gcc-mirror `master` one commit ahead of
`c5d147d7370fb36834c9348c5d3bab229d89fb3e`.
That commit is for format-patch only. Never push it to
github.com/gcc-mirror/gcc or any other GCC remote.

Finch container `gcc-rsize-bootstrap` may still exist. Leave it.
Do not start `gcc-rsize-trunk`.
