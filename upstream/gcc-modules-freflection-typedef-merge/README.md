# -freflection breaks module merging of typedef-named unnamed types shared between the GMF and a textual include

Status: DO NOT FILE. The defect is PR 124582, RESOLVED FIXED for GCC 16.2 (fix r17-205, backport r16-8841). Our trunk run confirms the fix: all four reproduction compiles (`mr.cc`, `main_refl.cc`, `reduced-module.cc`, `reduced-main.cc`) exit 0 on master commit 475e9eff (Linux container build, 2026-08-12; matrix at `/Users/bjorn/finch-gcc16/trunkcheck/trunk-matrix.txt`). The fix empirically covers our reduction. This entry stays as a record. The retire action is a toolchain bump to a GCC that contains the fix.

We verified the reproduction under guard on Darwin (GCC 16.1.0). We reduced the bug to a 6-line library-free testcase. We recorded Linux (glibc) evidence from the probe logs. See the section "Related reports" below.

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

## Related reports

We verified each reference below on 2026-08-11 against the GCC Bugzilla REST API, the gcc.gnu.org git server, and the gcc-patches archives. A deeper search than the duplicate check above found the defect already on file. This section supersedes that duplicate check.

### The same defect, already filed and fixed upstream

- PR 124582 — `Conflicting imported declaration with both modules and reflection enabled` — this is our defect: the same diagnostic on glibc's `pthread_mutex_t` (a typedef that names an unnamed union), with a GMF include plus a textual include; RESOLVED FIXED, target milestone 16.2.
- PR 123810 — `internal compiler error: in members_cmp, at cp/reflect.cc:6450` — RESOLVED FIXED; its fix, r16-7903-gf8152db38660 (Jakub Jelinek, 2026-03-05), changed the representation of typedef-named unnamed types, only under `-freflection`; that commit introduced our failure.
- Root cause, PR 124582 comment 2 (Nathaniel Shead, 2026-03-20): "This was caused by r16-7903-gf8152db38660061623150b346d84765676e92844, reflection now uses a different representation of `typedef struct {} foo;` which modules isn't yet equipped to deal with."
- Fix commit r17-205-g7802275c29d3 — `c++/modules+reflection: fix merging typedef struct { } A [PR124582]` (Patrick Palka, 2026-04-29) — teaches modules merging the `-freflection` representation, in which the unnamed decl "isn't visible to name lookup but still has the same DECL_NAME as the typedef decl".
- Backport r16-8841-gfd65d688f7a7 (releases/gcc-16, 2026-04-30) — puts the fix into GCC 16.2; our GCC 16.1.0 predates it, and our local source tree has no `g++.dg/modules/anon-4*` testcase.
- Testcase `g++.dg/modules/anon-4.h`, added by the fix — starts with `typedef struct { } A;` and has the same shape as our `reduced.h`.
- PR 124709 — `when "-freflection" is enabled, "import std;" and header includes of std library doesn't compile` — RESOLVED DUPLICATE of PR 124582.
- PR 125468 — `Compiling with modules and reflection has conflicting declaration` — RESOLVED DUPLICATE of PR 124582.
- PR 125787 — `[modules] Including headers and using modules with -freflection enabled causes conflicting imported declarations` — RESOLVED DUPLICATE of PR 124582; filed against 16.1.1.
- Workaround, PR 124582 comment 6 (Patrick Palka): `--compile-std-module` builds `<bits/stdc++.h>` as a header unit; the preprocessor then translates standard-library includes into imports, and this sidesteps the merge failure.

Action: do not file a new report. Verify the reduction against GCC 16.2 or trunk. If the reduction still fails there, reopen PR 124582 and attach `reduced.h`/`reduced-module.cc`/`reduced-main.cc`. Our reduction stays valuable: it is library-free, and the upstream testcase is not.

### Nearby reports in the same machinery

- PR 122774 — `The compiler ICEs when compiling a file that mixes a module import with a traditional header include` — NEW; Andrew Pinski first flagged PR 124582 as a possible duplicate of it; it stayed a separate defect.
- PR 124200 — `[modules][reflection] members_of does not see members of namespaces provided via modules` — NEW; another open reflection-and-modules interaction defect.
- PR 118829 — `[modules] ICE in add_indirects emitting template typedef struct` — NEW; an open typedef-struct modules defect without reflection.
- PR 99208 — `[modules] ICE with partitions & instantiations of linkage-typedef structs` — RESOLVED FIXED (2021); the earliest typedef-for-linkage modules defect.
- PR 98885 — `[modules] forward declaration of classes prevent them from being exported at the point of actual declaration` — RESOLVED FIXED for GCC 14.
- PR 102341 — `[modules] "error: conflicting exporting declaration" for anything previously declared` — RESOLVED DUPLICATE of PR 98885.
- PR 103524 — `[meta-bug] modules issue` — the modules meta-bug; its dependency list contains PR 124582, PR 122785, PR 122774, PR 118829, and our PR 126783.
- PR 126783 — `[16/17 Regression] [modules] ICE when a GMF variable is later defined inline` — our filing of 2026-08-10; a different defect in the same modules-merging machinery; it blocks PR 103524.
- PR 122785 — `[Reflection] -freflection and 'import std;' causes 'recursive lazy load' when 'std' module entity used` — RESOLVED FIXED; the one-line description above matches the upstream summary.
- PR 98770 — `[modules] including certain stdlib headers in the global module fragment of different modules causes conflicting global module declarations` — RESOLVED FIXED; the one-line description above matches the upstream summary.

### Mechanism in the GCC 16.1.0 source

We located the mechanism in our local tree (`/Users/bjorn/finch-gcc16/src/gcc-16.1.0/`). It agrees with the upstream root cause.

- `gcc/cp/decl.cc:13845` (`name_unnamed_type`) — the `flag_reflection` branch at lines 13851-13858 keeps the anonymous `TYPE_DECL` as `TYPE_NAME` and copies the typedef's name onto it (`DECL_NAME (orig) = DECL_NAME (decl)`, line 13856) instead of replacing the decl. r16-7903 added this branch. The branch tests only the flag. Therefore the two sides of an import build different ASTs from the same header text, and the flag on either side breaks the merge.
- `gcc/cp/module.cc:12069` (`check_mergeable_decl`), `TYPE_DECL` case at lines 12142-12152 — the match logic predates the new representation; the renamed unnamed decl is not visible to name lookup, so the imported class fails to merge with the textual class. r17-205 patches exactly this case.
- `gcc/cp/module.cc:12547` (`trees_in::is_matching_decl`) — the typedef branch at lines 12707-12711 then fails `same_type_p` on `DECL_ORIGINAL_TYPE`, and line 12712 emits `conflicting imported declaration`.

### gcc-patches record

- The reflection announcement series is `[PATCH 0/9] c++: C++26 Reflection [PR120775]` (Marek Polacek; v1 2025-11-15, v2 2025-12-17, v3 2026-01-14; inbox.sourceware.org).
- No cover letter calls out module interaction as a risk. The v3 cover letter says only: "The feature is hidden behind -freflection so should not be disruptive." The whole series changes `gcc/cp/module.cc` by 9 lines (META_TYPE streaming only). The option documentation calls the feature "experimental C++26 Reflection".
- The fix thread is `[PATCH] c++/modules+reflection: fix merging typedef struct { } A [PR124582]` (Patrick Palka, April 2026, gcc-patches message 714795).

### Clean searches

These Bugzilla queries returned no further hits on 2026-08-11: quicksearch `modules mbstate`; quicksearch `modules GMF merge typedef`; summary search `typedef-for-linkage`; summary search `mbstate` (only pre-modules libstdc++ reports, oldest PR 28975). The productive query was the summary substring search `conflicting imported`.

## Local workaround

Do not mix `-freflection` with `-fmodules` in TUs that textually include libc-reaching headers before an `import`. Use one of these two options:

- Make consumers include-free before imports.
- Remove `-freflection` from module builds.

No build configuration in this repository enables `-freflection`. The blocker occurred in the GCC 16 reflection probe cells. There is no `PINS.md` entry.
