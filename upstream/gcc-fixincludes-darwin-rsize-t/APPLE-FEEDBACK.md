# Apple SDK report

Wait until the fix direction for PR 126782 is settled. Then file this report.

## Feedback Assistant fields

| Field | Value |
|---|---|
| Area | Developer Tools & Resources → SDKs |
| Type | Incorrect/Unexpected Behavior |

## Title

```text
macOS SDK _rsize_t.h uses a Clang protocol when GCC enables modules
```

## Description

Paste this text:

```text
The macOS SDK header usr/include/sys/_types/_rsize_t.h contains this guard:

#if defined(__has_feature) && __has_feature(modules)

When this condition is true, the header defines __need_rsize_t and includes <stddef.h>.

This condition assumes that a compiler with the modules feature supports Clang's __need_rsize_t protocol. GCC does not support that protocol.

GCC 16.1 reports __has_feature(modules) as true with -fmodules. The SDK then selects the Clang path. GCC's stddef.h does not define rsize_t. Declarations in <string.h> then fail to parse.

Suggested guard:

#if defined(__clang__) && defined(__has_feature) && __has_feature(modules)

The existing plain typedef branch works for non-Clang compilers. This change does not change Clang behavior.

Reproduction environment:

- Apple Silicon
- macOS 26.2 SDK from Xcode 26.3
- GCC 16.1.0 with the Darwin arm64 port series

Test source:

#define __STDC_WANT_LIB_EXT1__ 1
#include <string.h>
rsize_t n;

Command:

g++-16 -std=c++26 -fmodules -c rsize.cc

Result:

_string.h:176:48: error: 'rsize_t' has not been declared; did you mean 'size_t'?

This failure also prevents GCC from building the C++ standard-library module on macOS.

The GCC project received a fixincludes patch for SDK versions that already exist. Apple must change the SDK header for a permanent fix.
```

If Feedback Assistant requests a source file, attach `rsize.cc`.
