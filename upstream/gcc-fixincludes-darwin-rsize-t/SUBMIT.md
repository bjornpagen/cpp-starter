# GCC fixincludes: macOS SDK `sys/_types/_rsize_t.h` assumes `__has_feature(modules)` implies clang

- **Where:** gcc-patches mailing list (fixincludes/), or GCC Bugzilla component `other` with the patch attached
- **Kind:** fixincludes patch, draft hack entry in `inclhack-entry.def`; the proven fixed text is `fixed-header.h` (in production use since 2026-08-08)
- **Verified:** GCC 16.1.0 aarch64-apple-darwin24, macOS 15.7 SDK

## Title

```
fixincludes: darwin sys/_types/_rsize_t.h breaks under GCC -fmodules
```

## Body (paste)

The macOS SDK's `sys/_types/_rsize_t.h` reads:

```c
#if defined(__has_feature) && __has_feature(modules)
#define USE_CLANG_STDDEF 1
...
#define __need_rsize_t
#include <stddef.h>
```

i.e. it treats `__has_feature(modules)` as "this compiler is clang and its
`stddef.h` honors the `__need_rsize_t` protocol". GCC also reports the
modules feature under `-fmodules` (verified: `#if __has_feature(modules)`
is taken with `g++ -std=c++26 -fmodules`, not without), but GCC's
`stddef.h` does not implement that clang-only protocol, so `rsize_t` is
never defined and the C11 Annex K declarations in the SDK's `<_string.h>`
fail to parse:

```
/_string.h:176:48: error: 'rsize_t' has not been declared; did you mean 'ssize_t'?
```

The practical blast radius is large: libstdc++'s `std` module
(`src/c++23/std.cc`, which reaches these declarations) fails to compile on
darwin under `-fmodules`, and the build then silently installs an empty
fallback (reported separately to libstdc++), leaving `import std;` broken
for every darwin GCC user.

The fix is a fixincludes hack replacing the header with the plain typedef
(the non-clang branch of the SDK's own conditional). Draft entry attached;
the fixed text has been in production use on aarch64-apple-darwin24 since
2026-08-08 with the full libstdc++ std module building and running.
