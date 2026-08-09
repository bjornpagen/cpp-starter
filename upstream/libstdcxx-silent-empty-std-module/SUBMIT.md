# libstdc++: std module build failure silently installs an empty `bits/std.cc`

- **Where:** GCC Bugzilla, component `libstdc++`
- **Kind:** bug report (build-system behavior; no code patch attached)
- **Verified:** GCC 16.1.0 built from source on aarch64-apple-darwin24
  (2026-08-08); fallback confirmed still present on trunk master
  @ 0621cf67366 (2026-08-09), line numbers below are trunk's.
- **Duplicate search (GCC Bugzilla, 2026-08-09):** searched via the
  Bugzilla REST quicksearch for `summary:"std module"`, `summary:std.cc`,
  `summary:modules.json`, `summary:"import std"`, `summary:"std module"
  empty`, `summary:"module initialization"`, and full-text
  `"Cannot compile std module"`. Nearest hits: PR 125460 (std.cc fails to
  compile with -ffreestanding — a compile failure that would trigger this
  fallback, but the report is about the failure, not the fallback/install
  behavior), PR 124714 (std.cc long double), PR 119266 (modules.json wrong
  path). **No duplicate found as of 2026-08-09.**
- **Verdict:** SEND
- **Independent corroboration (cite in the report):**
  Homebrew/homebrew-core issue #289142 (2026-06-21): the gcc 16.1.0
  bottle for Apple Silicon ships 1-byte `std.cc`/`std.compat.cc`
  referenced by `libstdc++.modules.json`; CMake fails with "is of type
  CXX_MODULES but does not provide a module interface unit or partition".
  Closed "not planned" as needing an upstream report; none was filed.

## Title

```
libstdc++: failed std module compile installs 1-byte bits/std.cc with exit 0
```

## Body (paste)

`libstdc++-v3/src/c++23/Makefile.am` contains a deliberate fallback: when
compiling the generated `std.cc` / `std.compat.cc` (the `import std`
module sources) fails, the recipe empties the source file and recompiles
that instead. On current master (0621cf67366, 2026-08-09) this is the
`std.lo` rule at Makefile.am:103-109, repeated for `std.o`,
`std.compat.lo`, `std.compat.o` at lines 110-130 (the `echo > $<.tmp`
fallback lines are 107, 114, 121, 128):

```make
std.lo: std.cc
	if ! $(LTCXXCOMPILE) $(MODULES_FLAGS) -c $< ; then \
	  echo "Cannot compile std module" >&2; \
	  echo "Module initialization function will be missing" >&2; \
	  echo > $<.tmp && mv $<.tmp $< && \
	  $(LTCXXCOMPILE) $(MODULES_FLAGS) -c $< ; \
	fi
```

The recipe does print "Cannot compile std module" to stderr, but the
build then completes with exit 0, so nothing fails and in a parallel
build log those two lines scroll past. Because the fallback empties the
file in the build tree, `make install` then installs the 1-byte
`bits/std.cc` / `bits/std.compat.cc` (`includebits_DATA`, Makefile.am:35)
and installs `libstdc++.modules.json` pointing at them
(`toolexeclib_DATA`, Makefile.am:33). Every consumer discovers the
breakage much later, with a confusing failure: CMake's import-std support
reports the scanned module sources do not provide a module interface
unit, or user code simply finds no `std` module to import. Nothing at
install time — and nothing machine-checkable at build time — says the
std module is broken.

We hit this on aarch64-apple-darwin24, where the module compile fails for
an unrelated SDK-header reason (fixincludes patch sent separately): the
toolchain built and installed green, and `import std;` was broken until we
diffed the installed `bits/std.cc` (1 byte) against the build tree. The
same failure mode shipped to users in the Homebrew gcc 16.1.0 bottle for
Apple Silicon (Homebrew/homebrew-core#289142): 1-byte module sources
referenced by the installed `libstdc++.modules.json`, closed there as
needing an upstream report.

How to reproduce (aarch64-apple-darwin24, macOS 15 SDK):

1. Build GCC 16.1.0 from release sources on darwin, where the std module
   compile fails for an unrelated SDK-header reason (fixincludes patch
   posted separately; any platform where `std.cc` fails to compile
   reproduces the fallback the same way — PR 125460's -ffreestanding
   failure is another trigger):
   `../gcc-16.1.0/configure --prefix=$PREFIX --enable-languages=c,c++
   --disable-nls --enable-checking=release --program-suffix=-16
   --with-system-zlib --build=aarch64-apple-darwin24
   --with-sysroot=<Xcode MacOSX.sdk>` then `make && make install`.
2. The libstdc++ build prints "Cannot compile std module" twice to
   stderr, mid-scroll in the parallel log, and completes with exit 0.
3. `wc -c $PREFIX/.../c++/16.1.0/bits/std.cc` → 1 byte, and it is
   referenced by the installed `libstdc++.modules.json`; compiling it
   per the manifest (`g++-16 -std=c++26 -fmodules -fsearch-include-path
   bits/std.cc`) produces no usable std module.

Environment of the affected build:
- g++-16 (GCC) 16.1.0
- Build/host/target: aarch64-apple-darwin24 (native)
- Configured with: ../gcc-16.1.0/configure --prefix=$HOME/.gcc/versions/16.1.0
  --enable-languages=c,c++ --disable-nls --enable-checking=release
  --program-suffix=-16 --with-system-zlib --build=aarch64-apple-darwin24
  --with-sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
- Bugzilla Version field: 16.1.0 (Makefile.am line numbers cited above are
  trunk's @ 0621cf67366, re-verified 2026-08-09).

Requested behavior — any of:

1. failing the build loudly when the std module does not compile (best), or
2. gating the fallback behind an explicit configure option, or
3. at minimum, not installing the empty sources and the manifest entry —
   an absent std module diagnoses far better than a present-but-empty one.

A build that installs a broken `import std` with exit 0 is a trap for
every platform where the module compile regresses.
