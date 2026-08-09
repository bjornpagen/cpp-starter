# GCC ICE: GMF variable declared `extern`, then defined `inline` — cc1plus segfault

- **Where:** GCC Bugzilla, component `c++`, Version `16.1.0`, keywords `ice-on-valid-code`; "[modules]" in the summary
- **Kind:** bug report with verified 4-line repro (`repro.cc`); `q.h` + `repro-include.cc` is the same bug via a real `#include`; `repro-stdexec.cc` is the original 12-line reduction from stdexec, kept for provenance
- **Verified:** official FSF `gcc:16.1.0` images, aarch64-linux-gnu AND
  x86_64-linux-gnu (2026-08-09, two independent environments: local
  container + CI) — symbolic backtrace obtained (`transfer_defining_module`
  ← `duplicate_decls`); plus the darwin-arm64 port build, re-verified
  cold 2026-08-09 with all variant claims re-tested individually
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

Transcript from an official FSF build (Docker library image
`gcc:16.1.0`, aarch64-linux-gnu):

```
$ g++ -std=c++26 -fmodules -c repro.cc
repro.cc:2:1: warning: global module fragment contents must be from preprocessor inclusion [-Wglobal-module]
    2 | extern int const q;
      | ^~~~~~
repro.cc:3:22: internal compiler error: Segmentation fault
    3 | inline constexpr int q = 1;
      |                      ^
0x20108cf diagnostics::context::diagnostic_impl(rich_location*, diagnostics::metadata const*, diagnostics::option_id, char const*, std::__va_list*, diagnostics::kind)
	???:0
0x2009a93 internal_error(char const*, ...)
	???:0
0x96ed18 transfer_defining_module(tree_node*, tree_node*)
	???:0
0x8d43ef duplicate_decls(tree_node*, tree_node*, bool, bool)
	???:0
0x98dd27 pushdecl(tree_node*, bool)
	???:0
0x8e9d1f start_decl(cp_declarator const*, cp_decl_specifier_seq*, int, tree_node*, tree_node*, tree_node**)
	???:0
0xa00aff c_parse_file()
	???:0
0xb2414f c_common_parse_file()
	???:0
/usr/local/libexec/gcc/aarch64-linux-gnu/16.1.0/cc1plus -quiet -imultiarch aarch64-linux-gnu -D_GNU_SOURCE repro.cc -quiet -dumpbase repro.cc -dumpbase-ext .cc -mlittle-endian -mabi=lp64 -std=c++26 -fmodules -o /tmp/cceOu8fr.s
Please submit a full bug report, with preprocessed source (by using -freport-bug).
Please include the complete backtrace with any bug report.
See <https://gcc.gnu.org/bugs/> for instructions.
```

The crash is in `transfer_defining_module`, reached from
`duplicate_decls` when the inline definition is pushed over the earlier
non-inline declaration. The identical ICE with the same frames
(addresses differ, e.g. `transfer_defining_module` at 0x945a64)
reproduces on x86_64-linux-gnu with the same official image. It also
reproduces on aarch64-apple-darwin24 with GCC 16.1.0 built from the FSF
release tarball plus the darwin-arm64 port series (upstream has no
aarch64-darwin target); that build prints no backtrace
(`--enable-checking=release`, stripped cc1plus) — under a debugger the
crash there is EXC_BAD_ACCESS reading address 0x0.

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
$ g++ -std=c++26 -fmodules -c repro-include.cc
In file included from repro-include.cc:2:
q.h:2:22: internal compiler error: Segmentation fault
```

(same `transfer_defining_module` backtrace; no `-Wglobal-module`
warning, confirming the warning is not load-bearing — also verified
directly: `-Wno-global-module` still ICEs.)

The code is valid: the first declaration is not a definition, the
definition adds `inline`, and no use precedes the inline declaration, so
[dcl.inline] is satisfied; the variable keeps external linkage from the
`extern const` declaration ([basic.link]). The same two lines compile
fine in a non-module TU.

Triage matrix (each variant re-verified individually on 16.1.0; the
matrix was run on the darwin build — same front end — and the two
transcript variants above additionally confirmed on the official Linux
builds):

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

Environment (primary — official FSF release build, Docker library image
`gcc:16.1.0`):
- g++ (GCC) 16.1.0
- Target: aarch64-linux-gnu (also reproduced: x86_64-linux-gnu, same image family)
- Configured with: /usr/src/gcc/configure --build=aarch64-linux-gnu
  --disable-multilib --enable-languages=c,c++,fortran,go

Also reproduced (corroboration): aarch64-apple-darwin24, GCC 16.1.0
built from the FSF release tarball plus the darwin-arm64 port series —
the configuration the macOS package managers ship; noted because
upstream trunk has no aarch64-darwin target. Configured with:
../gcc-16.1.0/configure --prefix=$HOME/.gcc/versions/16.1.0
--enable-languages=c,c++ --disable-nls --enable-checking=release
--program-suffix=-16 --with-system-zlib --build=aarch64-apple-darwin24
--with-sysroot=<Xcode MacOSX.sdk>

Trunk status: **still fails on master** — 17.0.0 20260809
(experimental), gcc-mirror master cloned 2026-08-09, aarch64-linux-gnu,
`--disable-bootstrap` build with default (enabled) checking. Under
checking the crash is the assertion:

```
repro.cc:3:22: internal compiler error: in transfer_defining_module, at cp/module.cc:22418
0x867d1b fancy_abort(char const*, int, char const*)
0xa78e73 transfer_defining_module(tree_node*, tree_node*)
	gcc/cp/module.cc:22418
0x97d873 duplicate_decls(tree_node*, tree_node*, bool, bool)
	gcc/cp/decl.cc:2721
0xaad6cb pushdecl(tree_node*, bool)
	gcc/cp/name-lookup.cc:4089
0x99aa0f start_decl(cp_declarator const*, cp_decl_specifier_seq*, int, tree_node*, tree_node*, tree_node**)
	gcc/cp/decl.cc:6801
0xb2237f cp_parser_init_declarator
	gcc/cp/parser.cc:26372
(... ordinary parser frames ...)
```

The include variant (`repro-include.cc` + `q.h`) fails the identical
assertion at the same coordinates. So the release-build segfault is
this checking assert: merging the inline definition over the earlier
non-inline declaration (`duplicate_decls`) transfers the defining
module between the two declarations, and `transfer_defining_module`
asserts at module.cc:22418.
