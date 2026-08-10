# GCC ICE: defaulted hidden-friend `operator==` streamed across module import

- **Where:** GCC Bugzilla, component `c++`, Version `16.1.0`, keywords `ice-on-valid-code`; "[modules]" in the summary
- **Kind:** bug report with verified 2-file repro (`a.cc`, `b.cc`)
- **Verified:** official FSF `gcc:16.1.0` images, on aarch64-linux-gnu
  AND x86_64-linux-gnu (2026-08-09, two independent environments: local
  container + CI). A symbolic backtrace was obtained. Also verified on
  the darwin-arm64 port build (re-verified cold, 2026-08-09).
- **Verdict:** SEND

## Duplicate search (GCC Bugzilla, 2026-08-09)

We ran these quicksearch queries. All returned zero matching results:
`summary:defaulted summary:module`, `summary:"operator==" summary:module`,
`summary:comparison summary:module`, `summary:defaulted summary:import`,
`summary:friend summary:module`, `summary:hidden summary:friend summary:ICE`,
`summary:spaceship summary:module`, `summary:segfault summary:import`,
`summary:module summary:segfault`, and all-text
`modules "defaulted" "friend" ICE segfault`. **No duplicate found.**
The nearest neighbor is PR 122822 ("[modules] Attachment of non-temploid
friend declarations"). It is not a duplicate: it covers attachment
semantics of friend redeclarations in importing TUs, with no ICE. Add it
as a "See Also" link when you file.

## Title

```
[modules] ICE (segfault) using an imported class with defaulted hidden-friend operator==
```

## Body (paste)

A named module exports a class. The class's `operator==` is a defaulted
hidden friend. When an importer uses the operator, cc1plus crashes in
the importer's compile. The two files below are the complete testcase.
They have no includes; they are their own preprocessed source:

```cpp
// a.cc
export module A;

export struct Mask {
	unsigned bits;
	friend constexpr bool operator==(Mask const&, Mask const&) = default;
};

// b.cc
import A;

int main() {
	return Mask{1} == Mask{1} ? 0 : 1;
}
```

Transcript from an official FSF build (Docker library image
`gcc:16.1.0`, aarch64-linux-gnu):

```
$ g++ -std=c++26 -fmodules -c a.cc      # OK, exit 0
$ g++ -std=c++26 -fmodules -c b.cc
In module A, imported at b.cc:1:
a.cc:5:31: internal compiler error: Segmentation fault
    5 |         friend constexpr bool operator==(Mask const&, Mask const&) = default;
      |                               ^~~~~~~~
0x20108cf diagnostics::context::diagnostic_impl(rich_location*, diagnostics::metadata const*, diagnostics::option_id, char const*, std::__va_list*, diagnostics::kind)
	???:0
0x2009a93 internal_error(char const*, ...)
	???:0
0x953490 module_state::mangle(bool)
	???:0
0x938673 mangle_decl(tree_node*)
	???:0
0x1394f17 decl_assembler_name(tree_node*)
	???:0
0xbde647 symbol_table::finalize_compilation_unit()
	???:0
/usr/local/libexec/gcc/aarch64-linux-gnu/16.1.0/cc1plus -quiet -imultiarch aarch64-linux-gnu -D_GNU_SOURCE b.cc -quiet -dumpbase b.cc -dumpbase-ext .cc -mlittle-endian -mabi=lp64 -std=c++26 -fmodules -o /tmp/ccucPVhV.s
Please submit a full bug report, with preprocessed source (by using -freport-bug).
Please include the complete backtrace with any bug report.
See <https://gcc.gnu.org/bugs/> for instructions.
```

The crash is in `module_state::mangle`, while the importer's compilation
unit is finalized. The identical ICE reproduces on x86_64-linux-gnu with
the same official image. The frames are the same; only the addresses
differ (for example, `module_state::mangle` at 0x9278d8). The ICE also
reproduces on aarch64-apple-darwin24, with a GCC 16.1.0 built from the
FSF release tarball plus the darwin-arm64 port series (upstream has no
aarch64-darwin target). That build prints no backtrace: it is built with
`--enable-checking=release` and its cc1plus is stripped. Under a
debugger, the crash there is EXC_BAD_ACCESS at address 0x74.

The code is valid. A defaulted comparison operator function may be a
non-member friend of the class ([class.compare.default]/1). The class is
exported from a named module interface unit in the ordinary way.
Expected behavior: `b.cc` compiles, and `main` returns 0.

Additional data points (each re-verified on 16.1.0):

- The same ICE occurs with `-std=c++20` (both files compiled with
  `-std=c++20 -fmodules`).
- The same ICE occurs when the class lives in a module partition
  (`export module A:part;`) re-exported by the primary interface
  (`export module A; export import :part;`).
- A defaulted *member* `constexpr bool operator==(Mask const&) const
  = default;` is unaffected. The importer compiles and runs correctly.
  That member form is the workaround we ship.

Environment (primary — official FSF release build, Docker library image
`gcc:16.1.0`):
- g++ (GCC) 16.1.0
- Target: aarch64-linux-gnu (also reproduced: x86_64-linux-gnu, same image family)
- Configured with: /usr/src/gcc/configure --build=aarch64-linux-gnu
  --disable-multilib --enable-languages=c,c++,fortran,go

Also reproduced (corroboration): aarch64-apple-darwin24, with GCC 16.1.0
built from the FSF release tarball plus the darwin-arm64 port series.
That is the configuration the macOS package managers ship. We note it
because upstream trunk has no aarch64-darwin target. Configured with:
../gcc-16.1.0/configure --prefix=$HOME/.gcc/versions/16.1.0
--enable-languages=c,c++ --disable-nls --enable-checking=release
--program-suffix=-16 --with-system-zlib --build=aarch64-apple-darwin24
--with-sysroot=<Xcode MacOSX.sdk>

Trunk status: **still fails on master** — 17.0.0 20260809
(experimental), gcc-mirror master cloned 2026-08-09, aarch64-linux-gnu,
`--disable-bootstrap` build with default (enabled) checking. Under
checking, the crash is this assertion:

```
In module A, imported at b.cc:1:
a.cc:5:31: internal compiler error: in mangle_module, at cp/module.cc:16805
0x867d1b fancy_abort(char const*, int, char const*)
0xa52783 mangle_module(int, bool)
	gcc/cp/module.cc:16805
0xa22ed7 write_unqualified_name
	gcc/cp/mangle.cc:1527
0xa1fe67 write_encoding
	gcc/cp/mangle.cc:945
0xa200cb write_mangled_name
	gcc/cp/mangle.cc:834
0xa25b9b mangle_decl_string
	gcc/cp/mangle.cc:4845
0xa25d6b get_mangled_id / mangle_decl(tree_node*)
	gcc/cp/mangle.cc:4861 / 4899
0x17f10bb decl_assembler_name(tree_node*)
	gcc/tree.cc:858
0xe2c027 symtab_node::get_comdat_group_id()
	gcc/cgraph.h:289
0xe2c027 analyze_functions
	gcc/cgraphunit.cc:1222
0xe2da37 symbol_table::finalize_compilation_unit()
	gcc/cgraphunit.cc:2593
```

The release-build segfault is this checking assert. The importer
computes the COMDAT group for the defaulted friend. Mangling then
demands the declaration's module, and `mangle_module` asserts at
module.cc:16805.
