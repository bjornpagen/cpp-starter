# Darwin `rsize_t` report and fixincludes patch

Complete each step in order.

## Step 1: confirm the DCO

Read the [GCC DCO](https://gcc.gnu.org/dco.html).

Confirm that you have the right to submit every patch line. Obtain a disclaimer if an employer or school owns the work.

The patch uses this identity:

```text
Bjorn Pagen <hello@bjornpagen.com>
```

## Step 2: file the Bugzilla report

Open [GCC Bugzilla](https://gcc.gnu.org/bugzilla/enter_bug.cgi?product=gcc).

Set these fields:

| Field | Value |
|---|---|
| Product | `gcc` |
| Component | `target` |
| Version | `16.1.0` |
| Known to fail | `16.1.0` |
| Known to work | Leave empty |
| See Also | `https://gcc.gnu.org/bugzilla/show_bug.cgi?id=116827` |

Attach these two plain files:

- `rsize.cc`
- `rsize.ii`

Do not put these files in an archive. The preprocessed file is 27 KiB.

### Bugzilla title

```text
[Darwin] sys/_types/_rsize_t.h does not define rsize_t with -fmodules
```

### Bugzilla body

Paste this text:

```text
The macOS 26.2 SDK header sys/_types/_rsize_t.h contains this guard:

#if defined(__has_feature) && __has_feature(modules)
#define USE_CLANG_STDDEF 1
#else
#define USE_CLANG_STDDEF 0
#endif

The selected branch defines __need_rsize_t and includes <stddef.h>.

GCC 16.1.0 reports __has_feature(modules) as true with -fmodules. GCC's stddef.h does not support the Clang __need_rsize_t protocol. The header therefore does not define rsize_t. Declarations in the SDK's _string.h then fail to parse.

The feature comes from flag_modules in gcc/cp/cp-objcp-common.cc. This behavior is present in upstream GCC.

Minimal source:

#define __STDC_WANT_LIB_EXT1__ 1
#include <string.h>
rsize_t n;

Command:

g++-16 -std=c++26 -fmodules -c rsize.cc

Output:

_string.h:176:48: error: 'rsize_t' has not been declared; did you mean 'size_t'?

I attached the source and its preprocessed output.

Observed environment:

- GCC 16.1.0 release sources with the documented Darwin arm64 port series
- Build, host, and target: aarch64-apple-darwin24
- macOS 26.2 SDK from Xcode 26.3
- Configure command:

../gcc-16.1.0/configure \
  --prefix=$HOME/.gcc/versions/16.1.0 \
  --enable-languages=c,c++ \
  --disable-nls \
  --enable-checking=release \
  --program-suffix=-16 \
  --with-system-zlib \
  --build=aarch64-apple-darwin24 \
  --with-sysroot=<Xcode MacOSX.sdk>

The Darwin arm64 port is supporting context. The incorrect feature predicate is upstream GCC behavior.

PR target/116827 covers the same SDK assumption for ptrdiff_t and size_t. Its committed workaround does not implement __need_rsize_t. It does not fix this testcase.

Expected result:

The SDK must define rsize_t. The source must compile with -fmodules.

Proposed fix:

A fixincludes rule changes the guard to require __clang__. GCC then uses the SDK's plain typedef branch. Clang behavior does not change.

The rule has a narrow Darwin target pattern. Its bypass makes a second fixincludes run produce no change. The patch includes a generated fixincl.x file and a test fixture.
```

Record the new PR number as `NNNNN`.

## Step 3: add the PR number to the patch

Do not send the current pre-PR patch.

Amend the GCC commit subject to this text:

```text
fixincludes: Fix rsize_t with Darwin modules [PRNNNNN]
```

Add this line inside the `fixincludes/ChangeLog:` block, immediately before
the first `*` entry:

```text
PR target/NNNNN
```

Keep this line at the end of the commit message:

```text
Signed-off-by: Bjorn Pagen <hello@bjornpagen.com>
```

Generate a new patch with `git format-patch -1`.

Run these checks:

1. Run `contrib/gcc-changelog/git_check_commit.py HEAD`.
2. Run `contrib/check_GNU_style.sh` on the new patch.
3. Apply the new patch to a clean current-master worktree.
4. Confirm that the source diff did not change.

## Step 4: send the patch email

Use these recipients:

```text
To: gcc-patches@gcc.gnu.org
Cc: bkorb@gnu.org, iain@sandoe.co.uk, mikestump@comcast.net
```

Use the subject from the generated patch:

```text
[PATCH] fixincludes: Fix rsize_t with Darwin modules [PRNNNNN]
```

Send plain text. Attach the patch as `text/x-patch`, or put the patch inline.

Do not use HTML or an `application/*` attachment. If Gmail hides the transfer
encoding, continue only when it identifies the attachment as text. Remove any
confidentiality footer.

Subscribe before you send, or add your address to GCC's global allow list. This prevents a spam filter from dropping the message.

### Patch email body

Paste this text above the patch:

```text
The macOS SDK uses __has_feature(modules) to select a Clang-specific rsize_t path. GCC also reports this feature with -fmodules.

GCC's stddef.h does not support __need_rsize_t. The SDK therefore leaves rsize_t undefined.

This patch makes the SDK guard require __clang__. GCC then uses the SDK's existing typedef branch. Clang behavior does not change.

The patch adds one Darwin fixincludes rule. It also includes the generated fixincl.x file and a test fixture.

Testing:

- fixincludes built with zero diagnostics.
- fixincludes make check passed.
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

Attach the regenerated patch after this text.

## Verified record

- Bootstrap-tested commit: `9f4a16ae995e455e7d7544a31cee1b5ed5c41986`
- Current commit: `14d1f0c9858e97a10cc26f36a1c923c1adf1183f`
- The current commit changes only the commit text. Its source diff is identical.
- Tested trunk base: `a1ba7736cfb4a5c7d97116934bd010de1207d002`
- Current-master application check: `cee53ed42c753a0c936e005b7dd15f029ca34da7`
- Tested Linux host: `aarch64-unknown-linux-gnu`
- Real SDK test target: `aarch64-apple-darwin24`
- Generated-file tool: AutoGen 5.18.16

The style checker reports patterns from generated output and literal SDK text. Manual review found no style defect in handwritten GCC code.

If nobody replies after approximately two weeks, reply once to the same thread. Include the public archive URL.
