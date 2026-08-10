# libstdc++: failed module fallback installs invalid interface metadata

- **Where:** GCC Bugzilla, component `libstdc++`, Version `17.0`
- **Known to fail:** `16.1.0`, `17.0`. Leave Known to work empty; the
  fallback was introduced during GCC 16 development and no earlier release
  has the same installation path.
- **Kind:** build/install consistency report; no patch attached
- **Verified:** the fallback and unconditional install rules are still present
  on fetched upstream master @ 8412da1ce39 (2026-08-10), at
  `libstdc++-v3/src/c++23/Makefile.am:33-35,103-130`.
- **Prior art:** PR 124268 introduced the fallback deliberately in
  r16-8714-g9d02b118ee1. Its commit message calls it a temporary kluge to
  remove during GCC 17 stage 1. PR 124554 comments 13-17 confirm that a
  successful bootstrap after `std.cc` fails is intentional. This report does
  not ask to regress that supported-target behavior; it covers the invalid
  artifacts installed afterward.
- **Duplicate search (2026-08-10):** nearest reports are PR 124268 (module
  initialization symbols), PR 124554 (the target failure that motivated the
  fallback), PR 125460 (a separate way `std.cc` can fail), PR 124714 (a
  `std.cc` compile failure), and PR 119266 (a manifest path error). None
  covers empty installed interface units still advertised by the manifest.
- **Independent user impact:** Homebrew/homebrew-core#289142 documents a
  shipped GCC 16.1.0 package with one-byte `std.cc` and `std.compat.cc` files
  referenced by `libstdc++.modules.json`; CMake rejects them because they do
  not provide module interface units.
- **Verdict:** SEND, after the fixincludes PR so the concrete Darwin trigger
  can be cross-referenced.

## Title

```
libstdc++: module fallback installs empty interface units referenced by manifest
```

## Body (paste)

The module-object fallback added for PR 124268 deliberately permits a GCC
bootstrap to continue when `std.cc` or `std.compat.cc` cannot be compiled.
That behavior was needed for targets affected by PR 124554, and comments
13-17 on that PR confirm that successful bootstrap is intentional.

The fallback has a separate install-time consequence. On current trunk
(8412da1ce39, 2026-08-10), each failure recipe overwrites the generated
module interface with an empty file before compiling the fallback object:

```make
std.lo: std.cc
	if ! $(LTCXXCOMPILE) $(MODULES_FLAGS) -c $< ; then \
	  echo "Cannot compile std module" >&2; \
	  echo "Module initialization function will be missing" >&2; \
	  echo > $<.tmp && mv $<.tmp $< && \
	  $(LTCXXCOMPILE) $(MODULES_FLAGS) -c $< ; \
	fi
```

The same pattern appears in the `std.o`, `std.compat.lo`, and
`std.compat.o` rules at `libstdc++-v3/src/c++23/Makefile.am:103-130`.
The install lists are unconditional:

```make
toolexeclib_DATA = libstdc++.modules.json
includebits_DATA = std.cc std.compat.cc
```

Consequently a handled module compile failure leaves a one-byte `std.cc`
and/or `std.compat.cc`, exits successfully, installs those empty files, and
also installs a manifest that advertises them as C++ module sources. The
fallback therefore preserves bootstrap, but the resulting installation
claims to provide module interface units that do not exist.

This is not hypothetical downstream behavior. The GCC 16.1.0 Homebrew
package for Apple Silicon shipped one-byte `bits/std.cc` and
`bits/std.compat.cc` files referenced by `libstdc++.modules.json`
(Homebrew/homebrew-core#289142). CMake later rejected the files with:

```
is of type CXX_MODULES but does not provide a module interface unit or partition
```

The Darwin module failure that triggered that package state is reported
separately as PR target/NNNNN. The install inconsistency is generic: any
handled failure in these four recipes produces the same artifacts. PR 124554
also exercised this fallback on GCC-supported offload targets; its discussion
shows why failing the whole bootstrap is not necessarily the right remedy.

The r16-8714-g9d02b118ee1 commit message said: "Fixing PR124554 and removing
this kluge should be done for GCC 17 in stage 1." The fallback is still
present on current GCC 17 trunk even though PR 124554 was fixed.

Expected behavior: retaining a successful bootstrap must not result in an
installation that advertises empty files as module interfaces. Suitable
outcomes include removing the temporary fallback now that its motivating
target bug is fixed, or preserving the fallback object while omitting the
unavailable interface units and their manifest entries from installation.
Compiling a distinct empty temporary translation unit would also avoid
destroying the generated module source, but the manifest must only advertise
interfaces the installation actually provides.

Observed environment that exposed the installed state:

- GCC 16.1.0 release sources plus the documented out-of-tree Darwin arm64
  port, native aarch64-apple-darwin24
- macOS 26.2 SDK, Xcode 26.3
- configured with `../gcc-16.1.0/configure
  --prefix=$HOME/.gcc/versions/16.1.0 --enable-languages=c,c++
  --disable-nls --enable-checking=release --program-suffix=-16
  --with-system-zlib --build=aarch64-apple-darwin24
  --with-sysroot=<Xcode MacOSX.sdk>`
- installed `bits/std.cc` and `bits/std.compat.cc`: one byte each
- installed `libstdc++.modules.json`: references both empty files

The report is based primarily on the target-independent upstream make rules
and their documented GCC-supported use in PR 124554. The Darwin build and
Homebrew issue are corroborating examples of the bad installed state, not a
request to support an unofficial GCC target.
