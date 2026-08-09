# cpp-starter

A starter template for a deliberately small C++26 dialect: modules-only,
exception-free, RTTI-free, structural, value-oriented, reflection-driven.

**`AGENTS.md` is the normative document** for humans and coding agents. When
this repository offers one mechanism for a concept, the alternatives are
forbidden — read it before writing any code.

## Requirements

Bring your own toolchain; install it however you like. Versions are enforced
at configure time; the table is informative.

| Tool | Version | Used for |
|---|---|---|
| GCC | 16.1.x | production compiler, found as `g++-16` |
| CMake | 4.2.x | build description |
| Ninja | 1.13+ | build execution |
| LLVM (clang++, clang-tidy, clang-scan-deps) | 22 | `lint` preset only |

Tools are referenced by bare name and found on `PATH`. If yours live
elsewhere, write a gitignored `CMakeUserPresets.json` that inherits a preset
and overrides `CMAKE_CXX_COMPILER` (CMake merges it automatically).

## Build

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
./build/dev/src/starter
```

| Preset | Purpose |
|---|---|
| `dev` | debug development build |
| `release` | optimized production build |
| `asan-ubsan` | AddressSanitizer + UndefinedBehaviorSanitizer |
| `tsan` | ThreadSanitizer (never combined with ASan); Linux only — GCC ships no TSan runtime on arm64 macOS |
| `lint` | Clang graph over the Clang-parseable remainder (the starter module is GCC-only as a unit); clang-tidy runs during the build and any warning is an error |

## Layout

One named module (`starter`) per component; internals are partitions, one
`.cc` file per concern. The primary interface (`src/starter.cc`) is the only
export surface — partitions cannot be imported from outside the module.

```text
src/        the starter module: primary interface + dialect partitions
tests/      dialect tests (module-native minimal harness, no macro frameworks)
foreign/    quarantined external-interface adaptation (headers allowed)
unsafe/     quarantined machine primitives; holds the :simd partition and
            the intrinsic kernel TUs
benchmarks/ dialect benchmark executables (GCC graph only, never in CI gates)
```

## Checks

Enforcement lives in the compiler and the build, not in scripts:

- **the compiler** — exceptions off, RTTI off, reflection on are hard
  configure-time flags; a toolchain outside the pin never configures
- **the build graph** — `cmake --build --preset dev` (any warning is an error)
- **clang-tidy** — `cmake --build --preset lint` runs it over the
  Clang-readable graph during the build
- **ctest** — `ctest --preset dev` runs the unit tests plus the
  toolchain-conformance tests

## Benchmarks

`starter_particles_bench` times four integration kernels over the same
reflection-derived SoA storage (the `:particles` partition in
`src/particles.cc` derives the layout and the access from `Particle` via
`define_aggregate`):

```sh
cmake --preset release
cmake --build --preset release
./build/release/benchmarks/starter_particles_bench
```

Kernel availability follows the build graph, not preprocessor conditionals:
the NEON kernel is selected for aarch64 targets only, and the SVE kernel is
built only with `-DSTARTER_SVE=ON` on a target that actually executes SVE —
no supported development host does (Apple silicon has no SVE; CI is x86_64),
so by default a stub reports it unavailable.

Reference numbers, Apple M2 Max, GCC 16.1 release preset, 1024 particles:

| Kernel | ns/step | relative |
|---|---|---|
| NEON x4 dense slab | ~178 | 1.00x |
| plain loop (autovectorized) | ~284 | 1.60x |
| NEON intrinsics (fused across axes) | ~365 | 2.05x |
| `std::experimental::simd` | ~367 | 2.06x |
| SVE intrinsics | not runnable on this hardware | — |

The slab kernel wins by exploiting a compile-time law of the derived
storage: each half's axis arrays are laid end to end, so a full-capacity
view is one contiguous run and all axes stream through a single x4-unrolled
pass (~3.7 of the core's 4 load/store slots per cycle — the L1 bandwidth
ceiling for this access mix). Among the per-axis kernels the autovectorizer
beats the hand-fused ones: its one-axis-at-a-time passes unroll into more
independent NEON chains than one 4-lane operation per axis per iteration.

## Known macOS toolchain issues

Documented, not automated — fixing your toolchain is your business:

- **GCC's `import std` silently breaks against the macOS SDK.** The SDK's
  `sys/_types/_rsize_t.h` assumes `__has_feature(modules)` implies clang and
  uses a clang-only `stddef.h` protocol, so the libstdc++ `std` module fails
  to compile — and libstdc++ **installs an empty `bits/std.cc` as a
  fallback**. Fix: drop a plain-typedef copy of `_rsize_t.h` into GCC's
  `include-fixed/sys/_types/` and rebuild libstdc++. Verify
  `include/c++/<ver>/bits/std.cc` is ~113 KB, not 1 byte.
- **MacPorts' `/opt/local/bin` clang wrappers break `clang-scan-deps`**
  (toolchain-root inference). Point a user preset at the real binaries in
  `/opt/local/libexec/llvm-22/bin` and set `CMAKE_CXX_STDLIB_MODULES_JSON`
  to `.../llvm-22/lib/libc++/libc++.modules.json`.
