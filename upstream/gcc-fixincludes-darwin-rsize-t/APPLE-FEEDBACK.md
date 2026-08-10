# Apple Feedback Assistant draft (root cause, filed under your Apple ID)

- **Area:** Developer Tools & Resources → SDKs
- **Type:** Incorrect/Unexpected Behavior

## Title

```
macOS SDK sys/_types/_rsize_t.h treats __has_feature(modules) as "compiler is Clang", breaking GCC -fmodules
```

## Description (paste)

The SDK header `usr/include/sys/_types/_rsize_t.h` selects its
implementation strategy with:

    #if defined(__has_feature) && __has_feature(modules)

and, when true, delegates the definition of `rsize_t` to the compiler's
`stddef.h` via the Clang-specific `__need_rsize_t` protocol
(`#define __need_rsize_t` + `#include <stddef.h>`).

`__has_feature(modules)` is a proxy for "this compiler is Clang and its
stddef.h implements that protocol" — and the proxy is no longer valid:
GCC also reports the `modules` feature when invoked with `-fmodules`
(GCC 16.1 and later), but GCC's `stddef.h` does not implement the
`__need_rsize_t` protocol. Result: `rsize_t` is never defined, and the
C11 Annex K declarations in `<string.h>` (via `_string.h`) fail to
parse for any GCC `-fmodules` compile that reaches them. In practice
this breaks the C++ standard library module (`import std;`) for GCC on
macOS entirely.

Suggested fix: make the guard test what it means —

    #if defined(__clang__) && defined(__has_feature) && __has_feature(modules)

The plain-typedef branch the header already contains is correct for
every non-Clang compiler (and for Clang without modules), so the
change is one predicate.

Reproduction (Apple Silicon or Intel, macOS 15 SDK):

1. Install/build GCC 16.1 (e.g. the FSF release).
2. Compile with `-std=c++26 -fmodules` any TU that defines
   `__STDC_WANT_LIB_EXT1__` and includes `<string.h>`:
   error: 'rsize_t' has not been declared.

For symmetry: a GCC-side fixincludes workaround has been submitted to
the GCC project (rewriting the guard at GCC install time), because
already-shipped SDKs cannot be fixed retroactively — but the header is
the right place for the durable fix.
