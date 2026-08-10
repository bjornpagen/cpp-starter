# Darwin `rsize_t` Bugzilla report

Use [GCC Bugzilla](https://gcc.gnu.org/bugzilla/enter_bug.cgi?product=gcc).

## Fields

| Field | Value |
|---|---|
| Product | `gcc` |
| Component | `target` |
| Version | `16.1.0` |
| Known to fail | `16.1.0` |
| Known to work | Leave empty |
| See Also | `https://gcc.gnu.org/bugzilla/show_bug.cgi?id=116827` |

## Attachments

Attach both files as plain files:

- `rsize.cc`
- `rsize.ii`

Do not archive these files.

## Title

```text
[Darwin] sys/_types/_rsize_t.h does not define rsize_t with -fmodules
```

## Body

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

## Final check

- Confirm that the title matches exactly.
- Confirm that the body includes the configure command.
- Confirm that Bugzilla shows both attachments.
- Confirm that See Also contains PR116827.
- Record the assigned PR number and URL.
