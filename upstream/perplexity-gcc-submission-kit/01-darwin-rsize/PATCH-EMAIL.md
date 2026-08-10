# Darwin fixincludes patch email

Replace every `DARWIN_PR_NUMBER` placeholder before you create the email.

## Recipients

```text
From: Bjorn Pagen <hello@bjornpagen.com>
To: gcc-patches@gcc.gnu.org
Cc: bkorb@gnu.org, iain@sandoe.co.uk, mikestump@comcast.net
```

## Subject

```text
[PATCH] fixincludes: Fix rsize_t with Darwin modules [PRDARWIN_PR_NUMBER]
```

## Body

```text
PR target/DARWIN_PR_NUMBER

The macOS SDK uses __has_feature(modules) to select a Clang-specific rsize_t path. GCC also reports this feature with -fmodules.

GCC's stddef.h does not support __need_rsize_t. The SDK therefore leaves rsize_t undefined.

This patch makes the SDK guard require __clang__. GCC then uses the SDK's existing typedef branch. Clang behavior does not change.

The patch adds one Darwin fixincludes rule. It also includes the generated fixincl.x file and a test fixture.

Testing:

- The fixincludes build produced zero diagnostics.
- The fixincludes test passed.
- The real macOS 26.2 SDK header changed only at the guard.
- A second fixincludes run produced no output.
- The unfixed SDK failed the rsize_t testcase with -fmodules.
- The fixed SDK passed the testcase with and without -fmodules.
- The complete libstdc++ std module built and ran with the local equivalent fix.
- The patch passed the GCC ChangeLog checker.
- The patch applied to current GCC master.
- Clean unpatched and patched trees completed make bootstrap for all default languages.
- Both trees completed make -k check.
- The full bootstraps and test suites ran on aarch64-unknown-linux-gnu.
- The SDK-specific before/after checks targeted aarch64-apple-darwin24.

The full result sets had machine-load differences. The unpatched analyzer plug-in timed out, so its tests did not run there. The patched tree reached those tests. Some LTO and ASan option variants also differed.

No baseline PASS became a patched FAIL. A serial rerun covered the complete affected analyzer group. Both compilers produced 28 PASS results and 5 expected XFAIL results. contrib/compare_tests reported no difference for that group.

I do not have write access to the GCC repository.
```

## Attachment

Attach the finalized, PR-numbered working copy of `PATCH-TEMPLATE.patch`.

Use `text/x-patch` or `text/plain` as the MIME type.
Do not choose an `application/*` MIME type. If Gmail hides the transfer
encoding, continue only when it identifies the attachment as text.

## Final check

- Confirm that the From field is `hello@bjornpagen.com`.
- Confirm that no placeholder remains.
- Confirm that the PR number appears in the subject and first body line.
- Confirm that the patch has the same PR number.
- Confirm that the patch has the `hello@bjornpagen.com` sign-off.
- Confirm that the message is plain text.
