# cpp-starter

A starter template for a deliberately small C++26 systems dialect: modules
only, exception-free, RTTI-free, structural, value-oriented,
reflection-driven, ownership-explicit, and bounded at every wire boundary.
The current machine boundary is intentionally Darwin-only.

[`AGENTS.md`](AGENTS.md) is normative. The build enforces the pinned
toolchain tuple; this document deliberately does not duplicate its versions.

## Memory and I/O model

The project adopts the skalibs ownership and event-loop shape in modern C++:

- every allocation and resource has exactly one RAII owner,
- public parsers return owning values,
- views are synchronous and call-scoped,
- readiness is returned as data into caller-owned storage,
- one loop owns and mutates every registered resource,
- kernel user data contains integer slot generations, never addresses,
- connection count, request size, response size, and deadlines are bounded.

The HTTP server is a single-threaded bounded reactor. It multiplexes up to the
named `max_connection_count` with `kevent64`, owns every descriptor and buffer
in a slot, and uses `{slot, generation}` tokens to reject stale events. SIGINT
and SIGTERM are events in the same loop. There are no worker threads, callback
waiters, opaque self pointers, or shared mutable state.

Application handlers are compile-time-selected stateless callables: they
receive `Request const&` and return `std::expected<Response, E>`, with an
application-specific error. This intentionally avoids storing an erased
callback or an application-state borrow in the reactor. A handler runs inline
on the owner loop and must be bounded and nonblocking. Requests that declare
a message body are rejected with 501: the template does not read bodies. The
public writer
returns owned bounded bytes and derives HTTP framing; the quarantine trampoline
alone copies that value into the fixed reactor buffer.

## Build and run

Bring the pinned tools on `PATH`; configure rejects every other tuple. Local
paths belong in a gitignored `CMakeUserPresets.json`.

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
./build/dev/examples/starter_httpd
```

The server prints `listening <port>` and blocks in its owner-driven loop:

```sh
curl http://127.0.0.1:<port>/hello
```

Send SIGINT or SIGTERM for a clean shutdown. The first signal stops accepting
and drains in-flight connections within the configured deadlines; a second
signal exits immediately.

| Preset | Purpose |
|---|---|
| `dev` | warnings-as-errors development build |
| `release` | optimized production build |
| `asan-ubsan` | AddressSanitizer + UndefinedBehaviorSanitizer |
| `lint` | Clang/clang-tidy over the reflection-free quarantine graph |

A ThreadSanitizer preset is intentionally absent: the supported server graph
is single-threaded and the pinned macOS GCC has no usable arm64 TSan runtime.
The policy forbids advertising an empty or unbuildable check.

The release graph is whole-program-optimized and the whole production graph
is hardened by default; none of it is configurable. Every configuration
builds with `_GLIBCXX_ASSERTIONS`, `-ftrivial-auto-var-init=zero`,
`-fstack-protector-strong`, `-fstack-clash-protection` (GCC graph only; the
lint Clang rejects the flag on this target), and
`-fzero-call-used-regs=used-gpr`. The `release` preset additionally links
with GCC LTO and compiles with trap-mode UBSan (`-fsanitize=undefined
-fsanitize-trap=all`), which needs no runtime library. LTO applies to the
`release` preset only: a `-flto -g` link has unbounded memory growth on the
pinned toolchain (`upstream/gcc-lto-modules-debug-oom/`; registry entry
`gcc-darwin-lto-debug-dsymutil` in `PINS.md`).

Deliberately absent, under the same no-empty-check policy:

- `-D_FORTIFY_SOURCE=3`: Apple's SDK excludes every C++ translation unit
  from the fortify wrappers, so the flag is byte-for-byte inert here.
- `-mbranch-protection=standard`: PAC/BTI execute as NOPs in plain-arm64
  Darwin processes, conferring zero mitigation.
- `-fhardened`: not supported on this target — it errors under `-Werror`
  with no classifiable warning and silently drops most constituents; the
  working constituents are forced explicitly above (registry entry
  `gcc-darwin-fhardened` in `PINS.md`;
  `upstream/gcc-darwin-fhardened-coverage/`).
- `-fanalyzer`: it bails out on exactly the reactor code it would need to
  analyze, so its silence is not a check (its two defects found while
  probing are packaged under `upstream/`).

## Layout

```text
src/        the starter module and pure dialect partitions
tests/      module-native tests and the HTTP integration smoke test
foreign/    pinned external-library adaptation; the stdexec boundary
unsafe/     Darwin syscall adaptation; the owner-driven kqueue reactor
examples/   the blocking bounded HTTP server
upstream/   GCC/libstdc++ reproductions and submission material
```

One named module, `starter`, is the public surface. Its partitions are
internal and explicitly listed in CMake. Dialect code contains no headers or
preprocessor directives.

## Sender/receiver

`std::execution` is the application async algebra. Until the pinned libstdc++
ships it, the reference implementation is pinned immutably and quarantined in
exactly one plain translation unit: `foreign/exec.backend.cc`.

The kernel reactor deliberately does not use generic readiness senders. A
syscall boundary reports event values; the owning loop advances a closed state
variant. This keeps kernel lifetime safety independent of sender operation
state. On every toolchain bump, the `PINS.md` ritual checks for native senders;
when they arrive, the one vendor boundary is rewritten over `std::execution`.

## Verification

- every GCC build treats warnings as errors,
- `lint` compiles and analyzes the actual `unsafe/` and `foreign/` plain TUs,
- `asan-ubsan` runs the full unit and HTTP smoke suite,
- parser and writer tests pin ownership, bounds, and failure transparency,
- the HTTP smoke test exercises concurrent clients, malformed input, typed
  handler failure, body rejection, absolute-deadline expiry, draining
  signal-driven shutdown, and recovery from accept pressure.

## Known macOS toolchain issues

The pinned GCC `import std` path needs two upstream fixes represented under
`upstream/`: Darwin's `_rsize_t` fixincludes interaction and libstdc++'s
silent empty `std` module fallback. `PINS.md` is the registry for active local
workarounds; upstream submission material records the external fixes.
