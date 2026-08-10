# GMF variable redeclaration ICE

File this report in GCC Bugzilla.

GMF means global module fragment. ICE means internal compiler error.

## Step 1: set the Bugzilla fields

Open [GCC Bugzilla](https://gcc.gnu.org/bugzilla/enter_bug.cgi?product=gcc).

Set these fields:

| Field | Value |
|---|---|
| Product | `gcc` |
| Component | `c++` |
| Version | `16.1.0` |
| Known to fail | `16.1.0, 17.0` |
| Known to work | `15.2.0` |
| Keywords | `ice-on-valid-code` |
| See Also | `https://gcc.gnu.org/bugzilla/show_bug.cgi?id=122551` |

Attach this file:

```text
gmf-extern-inline-repro.tar.gz
```

The archive contains only `q.h` and `repro-include.cc`. Both files also appear in the report body.

## Step 2: paste the title

```text
[16/17 Regression] [modules] ICE when a GMF variable is later defined inline
```

## Step 3: paste the report body

```text
This valid testcase declares a variable and later defines it as inline. The declarations enter a global module fragment through a header.

// q.h
extern int const q;
inline constexpr int q = 1;

// repro-include.cc
module;
#include "q.h"
export module m;

Command:

g++ -std=c++26 -fmodules -c repro-include.cc

GCC 16.1.0 produces this internal compiler error:

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

The compiler gives no warning before the ICE.

The same files compile with the official GCC 15.2.0 image:

g++ -std=c++20 -fmodules -c repro-include.cc

That command exits with status 0 and gives no diagnostic.

The code is valid. The first declaration is not a definition. The later declaration adds inline before any use. The first extern declaration gives the const variable external linkage.

The same declarations compile in a normal translation unit. They also compile in the module purview.

The trigger requires all three conditions:

1. The entity is a variable.
2. A non-inline declaration appears first.
3. An inline definition appears later in the global module fragment.

Verified variants on GCC 16.1.0:

| Variant | Result |
|---|---|
| extern int const q; then inline constexpr int q = 1; | ICE |
| extern int const q; then inline int const q = 1; | ICE |
| extern int q; then inline int q = 1; | ICE |
| The same variable forms with -std=c++20 | ICE |
| One inline variable definition without the first declaration | Compiles |
| A non-inline variable definition after the declaration | Compiles |
| The same declarations in the module purview | Compiles |
| The same declarations in a normal translation unit | Compiles |
| A function declaration followed by an inline definition | Compiles |

PR c++/122551 is related. Its fix introduced transfer_defining_module and calls it from duplicate_decls. This testcase reaches that path while GCC merges the variable declarations.

I did not identify r16-5213 as the first bad revision. I did not complete a bisection.

This bug affects libraries that define customization-point objects. NVIDIA stdexec uses this declare-then-define form for forwarding_query and other objects. A module unit that includes the affected header in its global module fragment crashes GCC.

Primary environment:

- Official Docker gcc:16.1.0 image
- Target: aarch64-linux-gnu
- Also reproduced with the official x86_64-linux-gnu image
- Configure command: /usr/src/gcc/configure --build=aarch64-linux-gnu --disable-multilib --enable-languages=c,c++,fortran,go

Known-good environment:

- Official Docker gcc:15.2.0 image
- Target: aarch64-linux-gnu
- Command: g++ -std=c++20 -fmodules -c repro-include.cc

Current trunk also fails:

- GCC 17.0.0 20260810 experimental
- Revision: a1ba7736cfb4a5c7d97116934bd010de1207d002
- Target: aarch64-linux-gnu
- Build type: full default-language bootstrap with --enable-checking=release

The trunk crash occurs in transfer_defining_module at gcc/cp/module.cc:22422. duplicate_decls calls it while merging the inline definition.

The bug also occurs with the Darwin arm64 port. That result is supporting evidence only. The official Linux GCC builds are the primary evidence.

Expected result:

GCC must compile the translation unit. The variable must remain usable through the global module fragment.
```

## Duplicate check

The Bugzilla search found no report for this exact variable redeclaration crash.

These reports describe different bugs:

- PR122551 covers an imported friend-function instantiation.
- PR122514 covers an extern local used in a template argument.
- PR126192 covers duplicate unnamed-enum entities from two modules.
- PR125704 covers suppression of `-Wglobal-module`.

Do not identify a regression commit without a completed bisection.

## Local files

- `q.h` and `repro-include.cc` are the primary reproduction.
- `repro.cc` is a secondary four-line reduction.
- `repro.cc` also receives the expected `-Wglobal-module` warning.
- `repro-stdexec.cc` records the original stdexec reduction.
