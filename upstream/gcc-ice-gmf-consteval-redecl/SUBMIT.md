# GCC ICE: GMF variable declared `extern`, then defined `inline` — cc1plus segfault

- **Where:** GCC Bugzilla, component `c++`, Version `16.1.0`, keywords `ice-on-valid-code`; "[modules]" in the summary
- **Kind:** bug report with verified 4-line repro (`repro.cc`); `q.h` + `repro-include.cc` is the same bug via a real `#include`; `repro-stdexec.cc` is the original 12-line reduction from stdexec, kept for provenance
- **Verified:** g++-16 (GCC) 16.1.0, aarch64-apple-darwin24 — re-verified cold 2026-08-09; all variant claims below re-tested individually
- **Verdict:** SEND
- **Note:** during re-verification the earlier claim "the consteval call operator is load-bearing" was found to be **false** and the reduction went further: no class, no consteval, no namespace needed. The directory name predates this; the bug is a GMF extern-then-inline variable redeclaration.

## Duplicate search (GCC Bugzilla, 2026-08-09)

Quicksearch queries run, all with zero relevant results:
`summary:"global module fragment"` (1 hit, a -Wglobal-module suppression
complaint, PR 125704 — unrelated), `summary:GMF`,
`summary:inline summary:module summary:ICE`,
`summary:consteval summary:module`, `summary:extern summary:module`
(3 hits, none about inline redeclaration), `summary:redecl summary:module`,
`summary:"inline constexpr" summary:module`, all-text `forwarding_query`,
all-text `stdexec`. **No duplicate found.**

## Title

```
[modules] ICE (segfault) when a GMF variable declared extern is redefined as inline
```

## Body (paste)

A variable first declared non-inline and then defined `inline` in the
global module fragment segfaults cc1plus at the definition. Complete
4-line testcase (no includes; the file is its own preprocessed source):

```cpp
module;
extern int const q;
inline constexpr int q = 1;
export module m;
```

```
$ g++-16 -std=c++26 -fmodules -c repro.cc
repro.cc:2:1: warning: global module fragment contents must be from preprocessor inclusion [-Wglobal-module]
    2 | extern int const q;
      | ^~~~~~
repro.cc:3:22: internal compiler error: Segmentation fault: 11
    3 | inline constexpr int q = 1;
      |                      ^
/Users/bjorn/.gcc/versions/16.1.0/libexec/gcc/aarch64-apple-darwin24/16.1.0/cc1plus -quiet -D__DYNAMIC__ repro.cc -fPIC -quiet -dumpbase repro.cc -dumpbase-ext .cc -mmacosx-version-min=15.0.0 -mcpu=apple-m1 -mlittle-endian -mabi=lp64 -std=c++26 -fmodules -o /var/folders/.../ccBnYnG9.s
Please submit a full bug report, with preprocessed source (by using -freport-bug).
See <https://gcc.gnu.org/bugs/> for instructions.
```

No further backtrace is printed (compiler built with
`--enable-checking=release`); under a debugger the crash is
EXC_BAD_ACCESS reading address 0x0 in cc1plus (stripped binary, no
symbolic frames).

The `-Wglobal-module` warning is an artifact of the hand-inlined
reduction only; the identical ICE occurs, without that warning, when the
two lines arrive via a real header:

```cpp
// q.h
extern int const q;
inline constexpr int q = 1;

// repro-include.cc
module;
#include "q.h"
export module m;
```

```
$ g++-16 -std=c++26 -fmodules -c repro-include.cc
In file included from repro-include.cc:2:
q.h:2:22: internal compiler error: Segmentation fault: 11
```

The code is valid: the first declaration is not a definition, the
definition adds `inline`, and no use precedes the inline declaration, so
[dcl.inline] is satisfied; the variable keeps external linkage from the
`extern const` declaration ([basic.link]). The same two lines compile
fine in a non-module TU.

Triage matrix (each variant re-verified individually on 16.1.0):

| variant | result |
|---|---|
| `extern int const q;` + `inline constexpr int q = 1;` in GMF | **ICE** |
| `extern int const q;` + `inline int const q = 1;` in GMF | **ICE** |
| `extern int q;` + `inline int q = 1;` in GMF | **ICE** |
| same, `-std=c++20` | **ICE** |
| `inline constexpr int q = 1;` alone in GMF (no prior declaration) | OK |
| `extern ... q;` + non-`inline` definition in GMF | OK |
| same two lines in the module purview instead of the GMF | OK |
| same two lines in a plain non-module TU | OK |
| function analogue: `void f();` + `inline void f() {}` in GMF | OK |

So the trigger is exactly: *variable*, non-inline declaration, then
inline definition, in the global module fragment.

Real-world impact: this declare-then-define + `inline constexpr` object
idiom is how libraries define customization point objects. NVIDIA
stdexec's `execution.hpp` declares `forwarding_query` before defining
its type and the object (`repro-stdexec.cc` in this report is the
12-line reduction of that), so any module unit whose GMF includes
stdexec ICEs the compiler.

Expected behavior: the TU compiles; `q` is usable from the GMF as in a
non-module TU.

Environment:
- g++-16 (GCC) 16.1.0
- Build/host/target: aarch64-apple-darwin24 (native)
- Configured with: ../gcc-16.1.0/configure --prefix=$HOME/.gcc/versions/16.1.0
  --enable-languages=c,c++ --disable-nls --enable-checking=release
  --program-suffix=-16 --with-system-zlib --build=aarch64-apple-darwin24
  --with-sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
