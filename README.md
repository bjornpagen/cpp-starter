# cpp-starter

A starter template for a deliberately small C++26 dialect — modules-only,
exception-free, RTTI-free, structural, value-oriented, reflection-driven —
whose flagship is a working **multicore HTTP server built on sender/receiver
(`std::execution`)**, years before the standard library ships it.

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

## The server

```sh
cmake --preset dev
cmake --build --preset dev
./build/dev/examples/starter_httpd    # binds an ephemeral port, prints "listening <port>"
curl localhost:<port>/hello           # responses carry the worker id — watch it
curl localhost:<port>/hello           # spread across cores under concurrent load
```

Thread-per-core, share-nothing: N workers, each owning its own kqueue
io-context and its own `SO_REUSEPORT` listener; per-connection accept →
read → parse → respond, composed as senders; zero cross-worker state, zero
locks outside the vendored thread pool; clean SIGINT shutdown. The HTTP/1.1
parser is pure dialect (`string_view` in, `expected` out).

## Build and test

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev
```

| Preset | Purpose |
|---|---|
| `dev` | debug development build |
| `release` | optimized production build |
| `asan-ubsan` | AddressSanitizer + UndefinedBehaviorSanitizer (the httpd smoke runs under it) |
| `tsan` | ThreadSanitizer (never combined with ASan); Linux only — GCC ships no TSan runtime on arm64 macOS |
| `lint` | Clang graph over the Clang-parseable remainder (the starter module is GCC-only as a unit); clang-tidy runs during the build and any warning is an error |

## Layout

One named module (`starter`) per component; internals are partitions, one
`.cc` file per concern. The primary interface (`src/starter.cc`) is the only
export surface — partitions cannot be imported from outside the module.

```text
src/        the starter module: primary interface + dialect partitions
            (:core, :enums, :http — the HTTP/1.1 parser/writer)
tests/      dialect tests (module-native minimal harness, no macro frameworks)
foreign/    quarantined external-interface adaptation (headers allowed);
            holds the :exec partition and the combinator half of the
            stdexec swap boundary (exec.backend.cc)
unsafe/     quarantined machine primitives; holds the :net partition and
            the I/O half of the swap boundary (net.backend.cc — the kqueue
            io-context)
examples/   dialect example executables (httpd: thread-per-core
            share-nothing HTTP server over :net + :http)
```

## Async

Sender/receiver (`std::execution`, P2300) is the only async algebra, active
today via the reference implementation (NVIDIA stdexec) pinned by SHA at
configure time — consumed from the maintained fork
(github.com/bjornpagen/stdexec), whose every commit is an
individually-submitted upstream fix (currently NVIDIA/stdexec#2167 and
NVIDIA/stdexec#2168) — and quarantined behind exactly one swap boundary
spelled across two plain TUs: `foreign/exec.backend.cc` (combinator half)
re-exports the verified combinator subset as `namespace ex` plus an
expected-erroring `wait` (never `sync_wait`: the dialect's errors are
values, not termination), and `unsafe/net.backend.cc` (I/O half) composes
the kqueue io-context's readiness senders. The `starter:exec` and
`starter:net` partitions export the dialect-clean surfaces over a narrow
ABI (GCC 16.1 ICEs on stdexec headers in any module unit, so senders never
cross the module boundary). No other file may touch stdexec; when libstdc++
ships `__cpp_lib_senders`, the vendor is deleted and only that boundary is
rewritten (AGENTS.md §15).

## Checks

Enforcement lives in the compiler and the build, not in scripts:

- **the compiler** — exceptions off, RTTI off, reflection on are hard
  configure-time flags; a toolchain outside the pin never configures
- **the build graph** — `cmake --build --preset dev` (any warning is an error)
- **clang-tidy** — `cmake --build --preset lint` runs it over the
  Clang-readable graph during the build
- **ctest** — `ctest --preset dev` runs the unit tests, the httpd smoke,
  plus the toolchain-conformance tests

## Known macOS toolchain issues

Documented, not automated — fixing your toolchain is your business:

- **GCC's `import std` silently breaks against the macOS SDK.** The SDK's
  `sys/_types/_rsize_t.h` assumes `__has_feature(modules)` implies clang and
  uses a clang-only `stddef.h` protocol, so the libstdc++ `std` module fails
  to compile — and libstdc++ **installs an empty `bits/std.cc` as a
  fallback**. Fix: drop a plain-typedef copy of `_rsize_t.h` into GCC's
  `include-fixed/sys/_types/` and rebuild libstdc++. Verify
  `include/c++/<ver>/bits/std.cc` is ~113 KB, not 1 byte. (Both halves are
  ready to submit upstream; see `upstream/`.)
- **MacPorts' `/opt/local/bin` clang wrappers break `clang-scan-deps`**
  (toolchain-root inference). Point a user preset at the real binaries in
  `/opt/local/libexec/llvm-22/bin` and set `CMAKE_CXX_STDLIB_MODULES_JSON`
  to `.../llvm-22/lib/libc++/libc++.modules.json`.
