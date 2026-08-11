# -freflection breaks module merging of typedef-named unnamed types shared between the GMF and a textual include

Status: We verified the reproduction under guard on Darwin today. We reduced the bug to a 6-line library-free testcase. We recorded Linux (glibc) evidence from the probe logs. The report is ready to file.

Each compile is fast and small. We still ran each compile below under the standard watchdog (`/tmp/buildguard.sh 8192 12288 300 ...`), one compile at a time. The guard never fired.

Local verification corrected the original hypothesis. The probe said that a GMF that contains `<meta>` breaks the import. The control run here disproves that. The `<meta>`-free control module fails identically when we compile it with `-freflection`. It passes only when the flag is absent. The trigger is the `-freflection` flag itself, on either side of the import. The trigger is not the reflection header. The directory name and the report text both use the corrected framing.

## Symptom

`g++-16 -std=c++26 -fmodules -freflection` rejects valid code. The failure occurs in this configuration:

- The global module fragment (GMF) of a named module includes a libstdc++ header that reaches the libc `__mbstate_t` typedef. `<string>` is sufficient, through `cwchar`.
- A consumer first textually includes a libc header that also declares `__mbstate_t`. `<cstdio>` is sufficient.
- The consumer then imports the module.

The import fails with this diagnostic:

```text
error: conflicting imported declaration 'typedef union __mbstate_t __mbstate_t'
note: existing declaration 'typedef union __mbstate_t __mbstate_t'
note: during load of binding '::reflected@mr'
```

The diagnostic shows `union` on Apple SDK headers and `struct` on glibc. Both libcs declare `__mbstate_t` as a typedef that names an unnamed class, and both fail. Without `-freflection`, the identical files compile and link cleanly. Thus this failure is rejects-valid. `-freflection` on only the interface compile, or on only the importer compile, is sufficient to break the import. LTO is irrelevant. The Darwin runs use `-O0 -c` with no LTO. The recorded Linux failure occurs in the importer's front end, before any LTO stage.

The library-free reduction shows the essential shape:

- A header declares a typedef that names an unnamed struct (typedef-for-linkage).
- The code uses that type as a template argument of a specialization, and an exported binding can reach that specialization.
- The GMF includes the header, and the importer also includes the same header textually.

libc's `__mbstate_t` has exactly this shape. libstdc++ reaches it through `char_traits<char>::state_type` / `fpos<__mbstate_t>` from `<string>`. Practical impact: with `-freflection`, almost no named module whose GMF touches `<string>` can be imported into a TU that already textually includes `<cstdio>` or `<wchar.h>`. Thus the flag is not usable with host libc headers in TUs that mix includes and imports.

## Environment

- GCC 16.1.0, self-built, `/Users/bjorn/.gcc/current`, target `aarch64-apple-darwin24`
- macOS arm64 (Darwin 24.6.0), Apple Silicon, 96 GB RAM; Apple SDK headers (`MacOSX.sdk/usr/include/arm/_types.h:70` declares `__mbstate_t`)
- Second platform (recorded evidence, not re-run here): GCC 16.1.0 self-built, target `aarch64-unknown-linux-gnu`, Debian trixie container, glibc 2.4x (`/usr/include/aarch64-linux-gnu/bits/types/__mbstate_t.h:21`); log at `/Users/bjorn/finch-gcc16/logs3/cell-refl_O0_g-flto-main.log`

## Files

- `mr.cc` — the original discovery module, copied verbatim from the probe cell. Its GMF has `<meta>` and `<string>`. It exports an `identifier_of`-based `type_name<T>()` and `reflected()`.
- `main_refl.cc` — the consumer, and the failing TU. It does `#include <cstdio>`, then `import mr;`.
- `mh-control.cc` / `main_ctrl.cc` — the `<meta>`-free control. Its GMF has `<vector> <string> <map> <format> <algorithm>`, and the consumer has the same shape. It passes without `-freflection` and fails identically with it.
- `reduced.h` / `reduced-module.cc` / `reduced-main.cc` — the library-free reduction. The header has 6 lines and uses no libc or libstdc++ code. It produces the same diagnostic.

## Reproduction (verified under guard)

Original form. Run the commands in a scratch directory. The compiler creates `gcm.cache/` beside the objects:

```sh
g++-16 -std=c++26 -fmodules -freflection -O0 -c mr.cc          # exit 0
g++-16 -std=c++26 -fmodules -freflection -O0 -c main_refl.cc   # fails
```

This is the verbatim result of the second command on Darwin:

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

Expected result: both commands exit 0 with no diagnostic. The commands do this when we remove `-freflection` from both.

The library-free reduction uses the same flags:

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

We verified this: both reduction compiles exit 0 when we remove `-freflection`.

Linux evidence: we recorded it on 2026-08-10 and did not re-run it. We quote from `/Users/bjorn/finch-gcc16/logs3/cell-refl_O0_g-flto-main.log`. The command was `g++ -std=c++26 -fmodules -freflection -O0 -g -flto -c ../main_refl.cc -o main.o` against the same `mr.cc`:

```text
/usr/include/aarch64-linux-gnu/bits/types/__mbstate_t.h:21:3: error: conflicting imported declaration 'typedef struct __mbstate_t __mbstate_t'
   21 | } __mbstate_t;
/usr/include/aarch64-linux-gnu/bits/types/__mbstate_t.h:21:3: note: existing declaration 'typedef struct __mbstate_t __mbstate_t'
   21 | } __mbstate_t;
../main_refl.cc:3:24: note: during load of binding '::reflected@mr'
```

The `-g -flto` in that log is incidental. The Darwin matrix shows that plain `-O0 -c` reproduces the failure.

## Trigger matrix

We verified each cell under guard on Darwin. All runs used `-std=c++26 -fmodules -O0 -c`.

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

The failure needs all of these conditions together:

1. `-freflection` is on at least one of the two compiles.
2. A typedef names an unnamed class (typedef-for-linkage).
3. The code uses that type as a template argument of a class template specialization, and the exported binding can reach that specialization.
4. The module's GMF includes the declaring header, and the importing TU also includes it textually before the `import`.

We held the standard version at `-std=c++26`. The `-O` level does not change the shape. All runs used `-O0`.

## Analysis

The rejection occurs during lazy load of the exported binding (`during load of binding '::reflected@mr'`). The sequence is:

1. The importer already has a textual declaration of the typedef and its dependent specializations.
2. The importer streams in the GMF copies from the CMI.
3. The `duplicate_decls`-level merge decides that the declarations conflict, and it does not merge them. This occurs although both declarations come from the same header text.

GCC attaches GMF declarations to the global module exactly so that this merge must succeed. The code is valid. Thus this is a rejects-valid failure, not a diagnosis of invalid code.

`-freflection` is the whole trigger. We established this in two independent ways:

- The `<meta>`-free control module fails when we add the flag.
- The same files pass when we remove the flag.

The flag on either side of the import alone is sufficient. This indicates that the flag changes both stream-out and stream-in of these declarations. This behavior agrees with a reflection implementation that attaches or requires additional identity for unnamed types. A typedef-for-linkage unnamed struct has no name of its own, and reflection's `identifier_of`/naming machinery is exactly the code that must handle that missing name. A named struct in the same position merges correctly. A typedef-for-linkage type merges correctly until it appears as a template argument of a streamed specialization. Thus the mismatch is plausibly in how GCC keys the specialization's argument identity when reflection is enabled. This is a hypothesis from behavior only. We did not trace the GCC module streaming code for this entry.

Cross-platform: we reproduced the failure against both glibc (`typedef struct ... __mbstate_t`, Debian trixie, aarch64) and Apple SDK headers (`typedef union ... __mbstate_t`, Darwin arm64). Thus nothing here is specific to Darwin or to fixincludes. LTO is irrelevant (see above). GCC 16 is the first release with `-freflection`. Thus this cannot be a regression from a released compiler.

## Suggested upstream destination

File in GCC Bugzilla, product `gcc`, component `c++`, version `16.1.0`, keywords `rejects-valid`. Title suggestion: `[modules] -freflection breaks merging of typedef-named unnamed structs between GMF and textual include (conflicting imported declaration for libc __mbstate_t)`.

In the report:

1. Attach `reduced.h`/`reduced-module.cc`/`reduced-main.cc` as the primary testcase. These files are small enough to put inline in the report.
2. Attach `mr.cc`/`main_refl.cc` as the real-world form.
3. State the practical effect: `#include <cstdio>` before `import` of any `<string>`-using module fails on both glibc and Apple SDK headers.
4. State that the probe originally blamed the GMF `<meta>` include, and that the control run disproved it. Then triagers will not chase the reflection header.

Duplicate check (web + Bugzilla search, 2026-08-11): no existing report combines `-freflection` with this merge failure. PR 122785 (`[Reflection] -freflection and 'import std;' causes 'recursive lazy load'`) is a different symptom in the same flag/modules intersection. It is worth a See Also. PR 98770 (conflicting global module declarations from stdlib headers in two GMFs) is the closest historical modules bug. But it came before reflection, and the GCC developers fixed it.

## Local workaround

Do not mix `-freflection` with `-fmodules` in TUs that textually include libc-reaching headers before an `import`. Use one of these two options:

- Make consumers include-free before imports.
- Remove `-freflection` from module builds.

No build configuration in this repository enables `-freflection`. The blocker occurred in the GCC 16 reflection probe cells. There is no `PINS.md` entry.
