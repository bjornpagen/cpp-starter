# GCC ICE: defaulted hidden-friend `operator==` streamed across module import

- **Where:** GCC Bugzilla, component `c++`, Version `16.1.0`, keywords `ice-on-valid-code`; "[modules]" in the summary
- **Kind:** bug report with verified 2-file repro (`a.cc`, `b.cc`)
- **Verified:** g++-16 (GCC) 16.1.0, aarch64-apple-darwin24 — re-verified cold 2026-08-09, commands exactly as below
- **Verdict:** SEND

## Duplicate search (GCC Bugzilla, 2026-08-09)

Quicksearch queries run, all with zero matching results:
`summary:defaulted summary:module`, `summary:"operator==" summary:module`,
`summary:comparison summary:module`, `summary:defaulted summary:import`,
`summary:friend summary:module`, `summary:hidden summary:friend summary:ICE`,
`summary:spaceship summary:module`, `summary:segfault summary:import`,
`summary:module summary:segfault`, and all-text
`modules "defaulted" "friend" ICE segfault`. **No duplicate found.**
Nearest neighbor, not a duplicate: PR 122822 ("[modules] Attachment of
non-temploid friend declarations") is about attachment semantics of friend
redeclarations in importing TUs, no ICE — worth a "See Also" link when filing.

## Title

```
[modules] ICE (segfault) using an imported class with defaulted hidden-friend operator==
```

## Body (paste)

A named module exporting a class whose `operator==` is a defaulted hidden
friend ICEs the *importer* when the operator is used. The two files below
are the complete testcase (no includes; they are their own preprocessed
source):

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

```
$ g++-16 -std=c++26 -fmodules -c a.cc      # OK, exit 0
$ g++-16 -std=c++26 -fmodules -c b.cc
In module A, imported at b.cc:1:
a.cc:5:31: internal compiler error: Segmentation fault: 11
    5 |         friend constexpr bool operator==(Mask const&, Mask const&) = default;
      |                               ^~~~~~~~
/Users/bjorn/.gcc/versions/16.1.0/libexec/gcc/aarch64-apple-darwin24/16.1.0/cc1plus -quiet -D__DYNAMIC__ b.cc -fPIC -quiet -dumpbase b.cc -dumpbase-ext .cc -mmacosx-version-min=15.0.0 -mcpu=apple-m1 -mlittle-endian -mabi=lp64 -std=c++26 -fmodules -o /var/folders/.../cciU2rXF.s
Please submit a full bug report, with preprocessed source (by using -freport-bug).
See <https://gcc.gnu.org/bugs/> for instructions.
```

No further backtrace is printed (compiler built with
`--enable-checking=release`). Under a debugger the crash is
EXC_BAD_ACCESS reading address 0x74 (a near-null dereference) in
cc1plus; no symbolic frames are available from the installed stripped
binary.

The code is valid: a defaulted comparison operator function may be a
non-member friend of the class ([class.compare.default]/1), and the
class is exported from a named module interface unit in the ordinary way.
Expected behavior: `b.cc` compiles and `main` returns 0.

Additional data points (each re-verified on 16.1.0):

- Same ICE with `-std=c++20` (both files compiled with `-std=c++20 -fmodules`).
- Same ICE when the class lives in a module partition
  (`export module A:part;`) re-exported by the primary interface
  (`export module A; export import :part;`).
- A defaulted *member* `constexpr bool operator==(Mask const&) const = default;`
  is unaffected — importer compiles and runs correctly (that is the
  workaround we ship).

Environment:
- g++-16 (GCC) 16.1.0
- Build/host/target: aarch64-apple-darwin24 (native)
- Configured with: ../gcc-16.1.0/configure --prefix=$HOME/.gcc/versions/16.1.0
  --enable-languages=c,c++ --disable-nls --enable-checking=release
  --program-suffix=-16 --with-system-zlib --build=aarch64-apple-darwin24
  --with-sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
