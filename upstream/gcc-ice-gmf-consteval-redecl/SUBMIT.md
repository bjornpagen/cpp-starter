# GCC regression: GMF variable declared `extern`, then defined `inline`

- **Where:** GCC Bugzilla, component `c++`, Version `16.1.0`, keyword
  `ice-on-valid-code`; `[16/17 Regression] [modules]` in the summary
- **Known to fail:** `16.1.0`, `17.0`
- **Known to work:** `15.2.0`
- **Kind:** bug report with a warning-free, valid two-file repro (`q.h` +
  `repro-include.cc`). `repro.cc` is a secondary four-line reduction that
  also triggers GCC's intentional `-Wglobal-module` warning. Attach
  `gmf-extern-inline-repro.tar.gz`, containing only the primary two files;
  keep both sources inline in the report body as well.
  `repro-stdexec.cc` is the original 12-line reduction from stdexec,
  kept for provenance.
- **Verified:** official FSF `gcc:16.1.0` images, on aarch64-linux-gnu
  and x86_64-linux-gnu (re-verified 2026-08-10 in two independent
  environments: local container and CI). A symbolic backtrace was obtained
  (`transfer_defining_module` ← `duplicate_decls`). Also verified on
  the darwin-arm64 port build (re-verified cold, 2026-08-09; every
  variant claim re-tested individually).
- **Regression evidence:** the warning-free primary testcase compiles with
  official FSF GCC 15.2.0 (aarch64-linux-gnu, `-std=c++20 -fmodules`)
  and ICEs with GCC 16.1.0 and current GCC 17 trunk.
- **Verdict:** SEND
- **Note:** re-verification showed the earlier claim "the consteval
  call operator is load-bearing" was **false**. The reduction then went
  further: no class, no consteval, no namespace is needed. The
  directory name predates this. The bug is a GMF extern-then-inline
  variable redeclaration.

## Duplicate search (GCC Bugzilla, 2026-08-10)

We ran these quicksearch queries. All returned zero relevant results:
`summary:"global module fragment"` (1 hit, a -Wglobal-module suppression
complaint, PR 125704 — unrelated), `summary:GMF`,
`summary:inline summary:module summary:ICE`,
`summary:consteval summary:module`, `summary:extern summary:module`
(3 hits, none about inline redeclaration), `summary:redecl summary:module`,
`summary:"inline constexpr" summary:module`, all-text `forwarding_query`,
all-text `stdexec`. **No duplicate found.**

PR 122551 is related but not a duplicate. Its r16-5213 fix introduced
`transfer_defining_module` and calls it from `duplicate_decls`; this report
reaches that path while merging a non-inline variable declaration with its
inline definition. Add PR 122551 as See Also. Do not claim it as the regression
point without a completed bisection.

## Title

```
[16/17 Regression] [modules] ICE when a GMF variable is later defined inline
```

## Body (paste)

The following complete testcase declares a variable and then defines it
`inline` in a header included by the global module fragment:

```cpp
// q.h
extern int const q;
inline constexpr int q = 1;

// repro-include.cc
module;
#include "q.h"
export module m;
```

Transcript from an official FSF build (Docker library image
`gcc:16.1.0`, aarch64-linux-gnu):

```
$ g++ -std=c++26 -fmodules -c repro-include.cc
In file included from repro-include.cc:2:
q.h:2:22: internal compiler error: Segmentation fault
    2 | inline constexpr int q = 1;
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
Please submit a full bug report, with preprocessed source (by using -freport-bug).
Please include the complete backtrace with any bug report.
See <https://gcc.gnu.org/bugs/> for instructions.
```

There are no warnings before the ICE. The crash is in
`transfer_defining_module`. It is reached from
`duplicate_decls`, when the inline definition is pushed over the earlier
non-inline declaration. The identical ICE reproduces on x86_64-linux-gnu
with the same official image. The frames are the same; only the
addresses differ (for example, `transfer_defining_module` at 0x945a64).
The ICE also reproduces on aarch64-apple-darwin24, with a GCC 16.1.0
built from the FSF release tarball plus the darwin-arm64 port series
(upstream has no aarch64-darwin target). That build prints no backtrace:
it is built with `--enable-checking=release` and its cc1plus is
stripped. Under a debugger, the crash there is EXC_BAD_ACCESS at
address 0x0.

This is a regression. The same `q.h` + `repro-include.cc` pair compiles
successfully with the official `gcc:15.2.0` image on aarch64-linux-gnu:

```
$ g++ -std=c++20 -fmodules -c repro-include.cc
# exit 0, no diagnostics
```

PR 122551 is related but does not describe this testcase. Its r16-5213 fix
introduced `transfer_defining_module` and calls it from `duplicate_decls`.
This ICE reaches that path while the inline definition is merged with the
earlier declaration. I have not claimed r16-5213 as the first bad revision
without a completed bisection; PR 122551 is included as See Also.

For reference, hand-inlining the declarations produces this four-line
secondary reduction:

```cpp
module;
extern int const q;
inline constexpr int q = 1;
export module m;
```

That form additionally receives `-Wglobal-module`, because GMF contents
normally arrive through preprocessing inclusion, but reaches the same
backtrace. `-Wno-global-module` does not affect the ICE.

The code is valid. The first declaration is not a definition. The
definition adds `inline`, and no use precedes the inline declaration, so
[dcl.inline] is satisfied. The variable keeps external linkage from the
`extern const` declaration ([basic.link]). The same two lines compile
fine in a non-module TU.

Triage matrix. Each variant was re-verified individually on 16.1.0. The
matrix ran on the darwin build (same front end). The two transcript
variants above were additionally confirmed on the official Linux builds:

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

So the trigger is exactly this: a *variable*, a non-inline declaration,
then an inline definition, in the global module fragment.

Real-world impact: libraries use this declare-then-define
`inline constexpr` object idiom to define customization point objects.
NVIDIA stdexec's `execution.hpp` declares `forwarding_query` before it
defines the type and the object. `repro-stdexec.cc` in this report is
the 12-line reduction of that. As a result, any module unit whose GMF
includes stdexec ICEs the compiler.

Expected behavior: the TU compiles, and `q` is usable from the GMF as in
a non-module TU.

Environment (primary — official FSF release build, Docker library image
`gcc:16.1.0`):
- g++ (GCC) 16.1.0
- Target: aarch64-linux-gnu (also reproduced: x86_64-linux-gnu, same image family)
- Configured with: /usr/src/gcc/configure --build=aarch64-linux-gnu
  --disable-multilib --enable-languages=c,c++,fortran,go

Known good (official FSF release build, Docker library image `gcc:15.2.0`):
- g++ (GCC) 15.2.0
- Target: aarch64-linux-gnu
- Command: `g++ -std=c++20 -fmodules -c repro-include.cc`
- Result: exit 0, no diagnostics

Also reproduced (corroboration): aarch64-apple-darwin24, with GCC 16.1.0
built from the FSF release tarball plus the darwin-arm64 port series.
That is the configuration the macOS package managers ship. We note it
because upstream trunk has no aarch64-darwin target. Configured with:
../gcc-16.1.0/configure --prefix=$HOME/.gcc/versions/16.1.0
--enable-languages=c,c++ --disable-nls --enable-checking=release
--program-suffix=-16 --with-system-zlib --build=aarch64-apple-darwin24
--with-sysroot=<Xcode MacOSX.sdk>

Trunk status: **still fails on master** — 17.0.0 20260810
(experimental), revision a1ba7736cfb4a5c7d97116934bd010de1207d002,
aarch64-linux-gnu. The warning-free include testcase was rerun against the
compiler from a full default-language bootstrap configured with
`--enable-checking=release`:

```
In file included from repro-include.cc:2:
q.h:2:22: internal compiler error: Segmentation fault
0x9855f8 transfer_defining_module(tree_node*, tree_node*)
	gcc/cp/module.cc:22422
0x8e9d4f duplicate_decls(tree_node*, tree_node*, bool, bool)
	gcc/cp/decl.cc:2721
0x9a45a7 pushdecl(tree_node*, bool)
	gcc/cp/name-lookup.cc:4089
0x8ff993 start_decl(cp_declarator const*, cp_decl_specifier_seq*, int, tree_node*, tree_node*, tree_node**)
	gcc/cp/decl.cc:6801
0xa06a0b cp_parser_init_declarator
	gcc/cp/parser.cc:26372
(... ordinary parser frames ...)
```

`duplicate_decls` merges the inline definition over the earlier non-inline
declaration. `transfer_defining_module` sees language-specific data on the
new declaration but not the old one. Current source has
`gcc_checking_assert (DECL_LANG_SPECIFIC (old_inner))` at module.cc:22418;
without that checking assertion, the write through
`DECL_MODULE_IMPORT_P (old_inner)` segfaults at line 22422. A
default-checking trunk build from the preceding day reached that assertion,
which is the same violated invariant rather than a different failure.
