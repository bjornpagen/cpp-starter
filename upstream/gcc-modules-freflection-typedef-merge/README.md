# -freflection breaks module merging of typedef-named unnamed types shared between the GMF and a textual include

Status: reproduction verified under guard on Darwin today, reduced to a 6-line library-free testcase, Linux (glibc) evidence recorded from the probe logs. Ready to file.

Every compile is fast and small; each one below was still run under the standard watchdog (`/tmp/buildguard.sh 8192 12288 300 ...`, one compile at a time). The guard never fired.

Local verification corrected the original hypothesis. The probe framed this as "GMF containing `<meta>` breaks the import"; the control run here shows the `<meta>`-free control module fails identically once it is compiled with `-freflection`, and passes only when the flag is absent. The trigger is the `-freflection` flag itself (on either side of the import), not the reflection header; the directory name and report text both use the corrected framing.

## Symptom

`g++-16 -std=c++26 -fmodules -freflection` rejects valid code. A named module whose global module fragment includes any libstdc++ header that reaches the libc `__mbstate_t` typedef (`<string>` suffices, via `cwchar`), imported by a consumer that first textually includes a libc header that also declares `__mbstate_t` (`<cstdio>` suffices), fails at the import with:

```text
error: conflicting imported declaration 'typedef union __mbstate_t __mbstate_t'
note: existing declaration 'typedef union __mbstate_t __mbstate_t'
note: during load of binding '::reflected@mr'
```

(`union` on Apple SDK headers, `struct` on glibc — both libcs declare `__mbstate_t` as a typedef naming an unnamed class, and both fail.) Without `-freflection` the identical files compile and link cleanly, so this is rejects-valid. `-freflection` on either the interface compile or the importer compile alone is sufficient to break it. LTO is irrelevant: the Darwin runs use `-O0 -c` with no LTO; the recorded Linux failure happens in the importer's front end before any LTO stage.

The library-free reduction shows the essential shape: a header declaring a typedef that names an unnamed struct (typedef-for-linkage), where that type is used as a template argument of a specialization reachable from an exported binding, included both in the GMF and textually in the importer. libc's `__mbstate_t` is exactly this shape, and libstdc++ reaches it through `char_traits<char>::state_type` / `fpos<__mbstate_t>` from `<string>`. Practical impact: with `-freflection`, essentially no named module whose GMF touches `<string>` can be imported into a TU that has already included `<cstdio>` or `<wchar.h>` textually — the flag is unusable with host libc headers in mixed include/import TUs.

## Environment

- GCC 16.1.0, self-built, `/Users/bjorn/.gcc/current`, target `aarch64-apple-darwin24`
- macOS arm64 (Darwin 24.6.0), Apple Silicon, 96 GB RAM; Apple SDK headers (`MacOSX.sdk/usr/include/arm/_types.h:70` declares `__mbstate_t`)
- Second platform (recorded evidence, not re-run here): GCC 16.1.0 self-built, target `aarch64-unknown-linux-gnu`, Debian trixie container, glibc 2.4x (`/usr/include/aarch64-linux-gnu/bits/types/__mbstate_t.h:21`); log at `/Users/bjorn/finch-gcc16/logs3/cell-refl_O0_g-flto-main.log`

## Files

- `mr.cc` — original discovery module, copied verbatim from the probe cell: GMF has `<meta>` and `<string>`, exports an `identifier_of`-based `type_name<T>()` and `reflected()`
- `main_refl.cc` — consumer: `#include <cstdio>` then `import mr;` — the failing TU
- `mh-control.cc` / `main_ctrl.cc` — `<meta>`-free control: GMF has `<vector> <string> <map> <format> <algorithm>`, same consumer shape; passes without `-freflection`, fails identically with it
- `reduced.h` / `reduced-module.cc` / `reduced-main.cc` — library-free reduction (6-line header, no libc or libstdc++ involvement) producing the same diagnostic

## Reproduction (verified under guard)

Original form (in a scratch directory, `gcm.cache/` is created beside the objects):

```sh
g++-16 -std=c++26 -fmodules -freflection -O0 -c mr.cc          # exit 0
g++-16 -std=c++26 -fmodules -freflection -O0 -c main_refl.cc   # fails
```

Verbatim result of the second command on Darwin:

```text
In file included from /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/machine/_types.h:34,
                 from /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/sys/_types.h:33,
                 from /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/_types.h:27,
                 from /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/_wchar.h:71,
                 from /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/wchar.h:67,
                 from /Users/bjorn/.gcc/versions/16.1.0/include/c++/16.1.0/cwchar:49,
                 from /Users/bjorn/.gcc/versions/16.1.0/include/c++/16.1.0/bits/postypes.h:42,
                 from /Users/bjorn/.gcc/versions/16.1.0/include/c++/16.1.0/bits/char_traits.h:44,
                 from /Users/bjorn/.gcc/versions/16.1.0/include/c++/16.1.0/string:45,
                 from /Users/bjorn/.gcc/versions/16.1.0/include/c++/16.1.0/bits/stdexcept_throw.h:57,
                 from /Users/bjorn/.gcc/versions/16.1.0/include/c++/16.1.0/array:44,
                 from /Users/bjorn/.gcc/versions/16.1.0/include/c++/16.1.0/meta:42,
                 from mr.cc:2,
of module mr, imported at main_refl.cc:2:
/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/arm/_types.h: In function 'int main()':
/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/arm/_types.h:70:3: error: conflicting imported declaration 'typedef union __mbstate_t __mbstate_t'
   70 | } __mbstate_t;
      |   ^~~~~~~~~~~
In file included from /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/machine/_types.h:34,
                 from /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/sys/_types.h:33,
                 from /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/_types.h:27,
                 from /Users/bjorn/.gcc/versions/16.1.0/lib/gcc/aarch64-apple-darwin24/16.1.0/include-fixed/_stdio.h:82,
                 from /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/stdio.h:61,
                 from /Users/bjorn/.gcc/versions/16.1.0/include/c++/16.1.0/cstdio:47,
                 from main_refl.cc:1:
/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/arm/_types.h:70:3: note: existing declaration 'typedef union __mbstate_t __mbstate_t'
   70 | } __mbstate_t;
      |   ^~~~~~~~~~~
main_refl.cc:3:24: note: during load of binding '::reflected@mr'
    3 | int main() { std::puts(reflected().c_str()); }
      |                        ^~~~~~~~~
```

Expected result: both commands exit 0 with no diagnostic, as they do when `-freflection` is dropped from both.

Library-free reduction, same flags:

```sh
g++-16 -std=c++26 -fmodules -freflection -O0 -c reduced-module.cc   # exit 0
g++-16 -std=c++26 -fmodules -freflection -O0 -c reduced-main.cc     # fails
```

```text
In file included from reduced-module.cc:2,
of module mu3, imported at reduced-main.cc:2:
reduced.h: In function 'int main()':
reduced.h:1:27: error: conflicting imported declaration 'typedef struct my_state_t my_state_t'
    1 | typedef struct { int c; } my_state_t;
      |                           ^~~~~~~~~~
In file included from reduced-main.cc:1:
reduced.h:1:27: note: existing declaration 'typedef struct my_state_t my_state_t'
    1 | typedef struct { int c; } my_state_t;
      |                           ^~~~~~~~~~
reduced.h:4:45: error: conflicting imported declaration 'using my_str<char>::pos = struct my_pos<my_state_t>'
    4 | template <typename C> struct my_str { using pos = my_pos<typename my_traits<C>::state_type>; C ch; pos p; };
      |                                             ^~~
reduced.h:4:45: note: existing declaration 'using my_str<char>::pos = struct my_pos<my_state_t>'
    4 | template <typename C> struct my_str { using pos = my_pos<typename my_traits<C>::state_type>; C ch; pos p; };
      |                                             ^~~
reduced-main.cc:3:21: note: during load of binding '::go@mu3'
    3 | int main() { return go().ch; }
      |                     ^~
```

Both reduction compiles exit 0 when `-freflection` is dropped (verified).

Linux evidence (recorded 2026-08-10, not re-run; quoted from `/Users/bjorn/finch-gcc16/logs3/cell-refl_O0_g-flto-main.log`, command was `g++ -std=c++26 -fmodules -freflection -O0 -g -flto -c ../main_refl.cc -o main.o` against the same `mr.cc`):

```text
/usr/include/aarch64-linux-gnu/bits/types/__mbstate_t.h:21:3: error: conflicting imported declaration 'typedef struct __mbstate_t __mbstate_t'
   21 | } __mbstate_t;
/usr/include/aarch64-linux-gnu/bits/types/__mbstate_t.h:21:3: note: existing declaration 'typedef struct __mbstate_t __mbstate_t'
   21 | } __mbstate_t;
../main_refl.cc:3:24: note: during load of binding '::reflected@mr'
```

The `-g -flto` in that log is incidental; the Darwin matrix shows plain `-O0 -c` reproduces.

## Trigger matrix (each cell verified under guard on Darwin, `-std=c++26 -fmodules -O0 -c` throughout)

| Variation | Result |
|---|---|
| `mr.cc` + `main_refl.cc`, both `-freflection` | rejected (mbstate output above) |
| `mr.cc` + include-free consumer (`import mr;` only), both `-freflection` | compiles |
| `mh-control.cc` (GMF `<vector> <string> <map> <format> <algorithm>`, no `<meta>`) + `main_ctrl.cc`, both `-freflection` | rejected identically — refutes the `<meta>` hypothesis |
| `mh-control.cc` + `main_ctrl.cc`, no `-freflection` on either | compiles |
| GMF is only `<string>`, consumer `#include <cstdio>`, both `-freflection` | rejected |
| module `-freflection`, consumer without | rejected |
| module without, consumer `-freflection` | rejected (then `confused by earlier errors, bailing out`) |
| `reduced.h` shape, both `-freflection` | rejected (typedef-for-linkage diagnostic above) |
| `reduced.h` shape, no `-freflection` | compiles |
| `reduced.h` with `struct my_state_t { int c; };` (named) instead of the typedef | compiles |
| typedef-of-unnamed struct used only by value in an inline function (no template-argument use) | compiles |
| glibc-shaped typedef (unnamed struct with anonymous-union member, `extern "C"`), by-value use only | compiles |
| same header moved behind `-isystem` | no effect (still compiles in the by-value shape) |

Necessary combination: (a) `-freflection` on at least one of the two compiles, (b) a typedef that names an unnamed class (typedef-for-linkage), (c) that type used as a template argument of a class template specialization reachable from the exported binding, (d) the declaring header included both in the module's GMF and textually in the importing TU before the `import`. Standard version was held at `-std=c++26`; `-O` level does not matter for the shape (all runs at `-O0`).

## Analysis

The rejection happens during lazy load of the exported binding (`during load of binding '::reflected@mr'`): the importer already has a textual declaration of the typedef and its dependent specializations, streams in the GMF copies from the CMI, and `duplicate_decls`-level merging concludes they conflict instead of merging them, even though both come from the same header text. GMF declarations are attached to the global module precisely so this merge must succeed; the code is valid, so this is rejects-valid, not a diagnosis.

`-freflection` is the whole trigger, established two independent ways: the `<meta>`-free control module fails once the flag is added, and the same files pass once it is removed. That either side of the import alone suffices indicates the flag perturbs both stream-out and stream-in of these declarations — consistent with the reflection implementation attaching or requiring additional identity for unnamed types (a typedef-for-linkage unnamed struct has no name of its own; reflection's `identifier_of`/naming machinery is exactly the code that cares about that). A named struct in the same position merges fine, and a typedef-for-linkage type merges fine until it appears as a template argument of a streamed specialization, so the mismatch is plausibly in how the specialization's argument identity is keyed with reflection enabled. This is a hypothesis from behavior only; the GCC module streaming code was not traced for this entry.

Cross-platform: reproduced against both glibc (`typedef struct ... __mbstate_t`, Debian trixie, aarch64) and Apple SDK headers (`typedef union ... __mbstate_t`, Darwin arm64), so nothing here is Darwin- or fixincludes-specific. LTO is irrelevant (see above). GCC 16 is the first release with `-freflection`, so this cannot be a regression from a released compiler.

## Suggested upstream destination

GCC Bugzilla, product `gcc`, component `c++`, version `16.1.0`, keywords `rejects-valid`. Title suggestion: `[modules] -freflection breaks merging of typedef-named unnamed structs between GMF and textual include (conflicting imported declaration for libc __mbstate_t)`. Attach `reduced.h`/`reduced-module.cc`/`reduced-main.cc` as the primary testcase (small enough to inline in the report) and `mr.cc`/`main_refl.cc` as the real-world form; state that the practical effect is that `#include <cstdio>` before `import` of any `<string>`-using module fails on both glibc and Apple SDK headers. Mention that the probe originally blamed the GMF `<meta>` include and that the control run disproved it, so triagers do not chase the reflection header.

Duplicate check (web + Bugzilla search, 2026-08-11): no existing report combines `-freflection` with this merge failure. PR 122785 (`[Reflection] -freflection and 'import std;' causes 'recursive lazy load'`) is a different symptom in the same flag/modules intersection and is worth a See Also. PR 98770 (conflicting global module declarations from stdlib headers in two GMFs) is the closest historical modules bug but predates reflection and was fixed.

## Local workaround

Do not mix `-freflection` with `-fmodules` in TUs that textually include libc-reaching headers before an `import`; either make consumers include-free before imports or drop `-freflection` from module builds. No build configuration in this repository enables `-freflection`; the blocker arose in the GCC 16 reflection probe cells. No `PINS.md` entry.
