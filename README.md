# cpp-starter

A starter template for a deliberately small C++26 dialect: modules-only,
exception-free, RTTI-free, structural, value-oriented, and (once the compiler
catches up) reflection-driven.

**`AGENTS.md` is the normative document** for humans and coding agents. When
this repository offers one mechanism for a concept, the alternatives are
forbidden — read it before writing any code.

## Layout

```text
src/        dialect code (modules only, all rules apply)
tests/      dialect tests (module-native minimal harness, no macro frameworks)
meta/       GCC-only C++26 reflection modules (empty until GCC 16)
foreign/    quarantined external-interface adaptation (headers allowed)
unsafe/     quarantined machine primitives (atomics, intrinsics, casts)
tools/      repository tooling (policy checker)
```

## Toolchain

The production compiler is GCC 16.1.0 built from source with the Apple
Silicon patch from GCC's Darwin maintainer (the same diff Homebrew validates
on arm64 CI). It installs bun-style under `~/.gcc` — versioned prefix, stable
`~/.gcc/current` symlink, no sudo, removable with `rm -rf ~/.gcc`:

```sh
sh tools/install-gcc.sh          # ~1-2 h full bootstrap
```

Everything else comes from MacPorts:

```sh
sudo port install cmake-devel ninja clang-22
```

| Tool | Pinned | Notes |
|---|---|---|
| GCC | 16.1.0 (`~/.gcc/current/bin/g++-16`) | C++26 + experimental reflection (`-freflection`); vanilla upstream has no arm64-darwin support yet, hence the patched source build |
| CMake | cmake-devel 4.2.1 (485f11a7) | the `import std` experimental UUID in `CMakeLists.txt` is pinned to this exact snapshot |
| Ninja | 1.13.2 | generator only; never invoked directly |
| clang-tidy | 22 (`clang-tidy-mp-22`) | lint frontend only, runs `--experimental-custom-checks` over the Clang-readable graph |

Toolchain versions are part of the language implementation. Bumps are
deliberate: on a clang-tidy bump, re-review `.clang-tidy`, `-list-checks`,
and `--dump-config`; on a CMake bump, re-verify the `import std` UUID from
`Help/dev/experimental.rst`.

## Building

CMake presets are the only developer interface:

```sh
cmake --preset gcc-dev               # configure
cmake --build --preset gcc-dev      # build
ctest --preset gcc-dev              # test
./build/gcc-dev/src/starter         # run
```

Presets (each with its own build directory under `build/<preset>/`):

| Preset | Purpose |
|---|---|
| `gcc-dev` | debug development build |
| `gcc-release` | optimized production build |
| `gcc-asan-ubsan` | AddressSanitizer + UndefinedBehaviorSanitizer |
| `gcc-tsan` | ThreadSanitizer (never combined with ASan); **Linux/CI only** — GCC ships no TSan runtime on arm64 macOS |
| `clang-lint` | reflection-free Clang graph; clang-tidy runs during the build and any warning is an error |

## Checks

```sh
python3 tools/check_policy.py       # AGENTS.md §34 source-policy checks
cmake --build --preset clang-lint   # clang-tidy over the Clang-readable graph
```

The policy checker covers what clang-tidy cannot see (preprocessor use,
header units, forbidden extensions and tokens, lint suppressions) across all
dialect code including GCC-only files. CI runs it on every push; build/lint CI
jobs land once a pinned GCC 16 environment exists.
