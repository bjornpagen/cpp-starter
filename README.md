# cpp-starter

A starter template for a deliberately small C++26 dialect: modules-only,
exception-free, RTTI-free, structural, value-oriented, reflection-driven.

**`AGENTS.md` is the normative document** for humans and coding agents. When
this repository offers one mechanism for a concept, the alternatives are
forbidden — read it before writing any code.

## Requirements

Bring your own toolchain; install it however you like. The configure step
rejects anything that doesn't meet the pin.

| Tool | Version | Used for |
|---|---|---|
| GCC | 16.1+ | production compiler, found as `g++-16` |
| CMake | 4.2+ | build description |
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
| `lint` | reflection-free Clang graph; clang-tidy runs during the build and any warning is an error |

## Layout

```text
src/        dialect code (modules only, all rules apply)
tests/      dialect tests (module-native minimal harness, no macro frameworks)
meta/       GCC-only C++26 reflection modules (excluded from the lint graph)
foreign/    quarantined external-interface adaptation (headers allowed)
unsafe/     quarantined machine primitives (atomics, intrinsics, casts)
tools/      repository tooling (policy checker)
```

## Checks

```sh
python3 tools/check_policy.py   # AGENTS.md §34 source-policy checks (also runs in CI)
cmake --build --preset lint     # clang-tidy over the Clang-readable graph
```

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
