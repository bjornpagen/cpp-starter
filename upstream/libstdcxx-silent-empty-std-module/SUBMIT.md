# libstdc++: std module build failure silently installs an empty `bits/std.cc`

- **Where:** GCC Bugzilla, component `libstdc++`
- **Kind:** bug report (build-system behavior; no code patch attached)
- **Verified:** GCC 16.1.0 built from source on aarch64-apple-darwin24 (2026-08-08)

## Title

```
libstdc++: failed std module compile installs 1-byte bits/std.cc with exit 0
```

## Body (paste)

`libstdc++-v3/src/c++23/Makefile.am` contains a deliberate fallback: when
compiling the generated `std.cc` (the `import std` module source) fails,
the recipe replaces it with an empty file and recompiles that instead:

```make
  echo > std.cc.tmp && mv std.cc.tmp std.cc && \
  <recompile the now-empty std.cc>
```

The build then completes with exit 0, `make install` installs the 1-byte
`bits/std.cc` / `bits/std.compat.cc`, and `libstdc++.modules.json` points
at them. Every consumer discovers the breakage much later, with a
confusing failure: CMake's import-std support reports the scanned module
sources "do not provide a module interface unit", or user code simply
finds no `std` module to import. Nothing at build or install time says
the std module is broken.

We hit this on aarch64-apple-darwin24, where the module compile fails for
an unrelated SDK-header reason (fixincludes report filed separately): the
toolchain built and installed green, and `import std;` was broken until we
diffed the installed `bits/std.cc` (1 byte) against the build tree.

Requested behavior — any of:

1. failing the build loudly when the std module does not compile (best), or
2. gating the fallback behind an explicit configure option, or
3. at minimum, not installing the empty sources and the manifest entry —
   an absent std module diagnoses far better than a present-but-empty one.

A build that installs a broken `import std` with exit 0 is a trap for
every platform where the module compile regresses.
