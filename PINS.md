# PINS.md — the pinned-quirk registry

One entry per pinned workaround: code or build configuration that is
deliberately wrong by dialect law because the pinned toolchain or platform
requires it. Each `PIN(name)` site in the tree points at its entry
here; the essay lives here, once.

Tombstone ritual: on every toolchain bump, read this file top to bottom,
re-test every retire condition, and delete what upstream fixed — one file,
one sweep.

The accepted pinned toolchain release series and generator live only in the
top-level CMake configure gate. The stdexec revision lives only in the
top-level CMake dependency declaration.

## clang-contracts

- symptom: the pinned Clang lint frontend does not parse the C++26
  `contract_assert` statement accepted by the production GCC graph
- sites: `unsafe/net.backend.cc` — one fail-stop `invariant` helper used by
  the Clang-readable kqueue boundary
- workaround: implement that one boundary helper with `std::terminate`; do
  not proliferate an alternate assertion vocabulary into dialect code
- retire: replace the helper and its call sites with `contract_assert` when
  the pinned Clang parses the production contract syntax
- upstream: [Clang C++ status](https://clang.llvm.org/cxx_status.html) lists
  P2900 contracts as unsupported

## stdexec-tooling-carveouts

- symptoms, all confined to pinned stdexec internals:
  - Clang's stack-escape analyzer diagnoses intentional operation-state
    self-references as addresses of temporaries, while
    `performance-move-const-arg` asks generic sender code to stop expressing a
    semantically required move;
  - UBSan's `null`, `nonnull-attribute`, and `returns-nonnull-attribute`
    instrumentation prevents stdexec's consteval completion-signature
    machinery from compiling;
  - GCC's optimized ASan/UBSan build diagnoses a null dereference in stdexec's
    intrusive-queue splice even though the ordinary optimized build accepts
    the same code with `-Wnull-dereference` and the queue is vendor-owned.
- sites: `foreign/CMakeLists.txt` — every carve-out is scoped to
  `starter_exec_backend` (and the warning override to its one source); the
  analyzer, sanitizer, and warning remain enabled everywhere else
- workaround: preserve the sender lifetime/move protocol and disable only the
  named diagnostics or instrumentation sub-checks on the one vendor boundary;
  the GCC warning exception applies only to the `RelWithDebInfo` sanitizer
  personality
- retire: re-enable each item independently when a pinned Clang or stdexec bump
  compiles and analyzes this boundary without it
- upstream: none filed; these are interactions inside the pinned vendor
  expression implementation. The exported combinator behavior remains covered
  by the GCC tests, the rest of UBSan/ASan, and the Clang boundary graph.

## gcc-partition-bmi-inplace-vector

- symptom: exporting a product with
  `std::inplace_vector<Header, max_header_count>` as a data member writes a BMI
  that the next partition cannot read (`failed to read compiled module cluster:
  Bad file data`), reproduced after a clean build
- sites: `Request::headers` and `Response::headers` in `src/http.cc`
- workaround: use owning `std::vector<Header>`, reserve the parser's named
  maximum, check the bound before every parser insertion and response
  serialization, and keep all returned data owning
- retire: replace both members with `std::inplace_vector` and run a clean full
  graph on every pinned-toolchain bump
- upstream: no standalone reduction yet; [GCC PR
  99426](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=99426) tracks the general
  module-cluster `Bad file data` family, but this exact C++26 library-type
  trigger has not been established as a duplicate. Do not comment or file it
  without first producing and testing a minimal reduction.

## gcc-gmf-stdexec-ice

- symptom: cc1plus segfaults whenever `stdexec/execution.hpp` is textually
  included in ANY module unit — interface partition, primary, or
  implementation unit, with or without `import std`. Root cause is a
  global-module-fragment variable declared `extern T const x;` and then
  defined `inline constexpr T x{};` — the stdexec CPO pattern; the CPO
  headers contain 54+ such load-bearing pairs, so it is not patchable.
  Consequence: sender composition cannot cross the module boundary on this
  toolchain — only concrete function surfaces do.
- sites: `foreign/exec.backend.cc` is the one plain vendor boundary; the
  `:exec` partition reaches it through an `extern "C++"` narrow ABI
- workaround: every stdexec include stays in that one plain backend TU; no
  other file in the repository may include a stdexec header or spell
  `stdexec::`/`exec::`
- retire: re-verify the pinned micro-repro and check the standard library for
  native senders on every toolchain bump; once available, the backend re-binds
  `namespace ex` to `std::execution`, the FetchContent pin is deleted, and
  everything upward is untouched
- upstream: `upstream/gcc-ice-gmf-consteval-redecl/` — 4-line repro (no
  consteval needed; the directory name is historical), triage matrix
  verified, dupe search clean; status SEND

## gcc-template-for-wshadow

- symptom: expansion statements (`template for`) re-declare the loop
  variable in a nested scope per iteration, so any expansion over a range
  with more than one element trips `-Wshadow` on compiler-generated
  scoping (fires once per element) — a build failure under `-Werror`
- sites: src/enums.cc, tests/enums.test.cc, and tests/conformance.test.cc, via
  scoped `set_source_files_properties(... COMPILE_OPTIONS -Wno-shadow)` in
  their CMakeLists files (a source-level property so it lands after the
  language profile's `-Wshadow` on the command line)
- workaround: `-Wno-shadow` only for the TUs that instantiate expansion
  statements; `-Wshadow` stays on everywhere else
- retire: delete the scoped suppressions when the fix for GCC PR 124197
  ships in the pinned toolchain; re-test on every toolchain bump
- upstream: GCC PR 124197 — CC + comment with our evidence (fires once per
  element on the pinned compiler's reflection variant) per upstream/README.md; no
  directory of our own

## cmake-ld-link-order

- symptom: the P2900 contracts runtime (`handle_contract_violation`) lives
  in libstdc++exp, and a `-lstdc++exp` spelled in `CMAKE_EXE_LINKER_FLAGS`
  satisfies nothing on Linux: GNU ld resolves left-to-right and the linker
  flags precede the object files on the link line (macOS ld64 masked the
  bug)
- sites: CMakeLists.txt —
  `target_link_libraries(starter_language_profile INTERFACE stdc++exp)`:
  interface-library linkage lands the library AFTER every consumer's
  object files
- workaround: link the contracts runtime through the interface target,
  never through global linker flags
- retire: when the pinned toolchain folds the contracts runtime into
  default libstdc++ linkage (no explicit `stdc++exp` needed); re-check on
  every toolchain bump
- upstream: none — documented GNU ld semantics, nothing to file

## cmake-import-std-uuid

- symptom: `import std` is experimental in CMake, gated by
  `CMAKE_EXPERIMENTAL_CXX_IMPORT_STD`, and the accepted UUID value changes
  per CMake feature series — a stale UUID silently disables the feature
- sites: CMakeLists.txt — the hard configure gate on the pinned CMake series
  plus `set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD
  "d0edc3af-4c50-42ea-a356-e2862fe7a444")` (the pinned-series value)
- workaround: pin the CMake series and the UUID together; on any CMake
  bump re-read `Help/dev/experimental.rst`, update the UUID, and move the
  version gate to the new series
- retire: when CMake ships `import std` as a stable (non-experimental)
  feature and the UUID gate disappears
- upstream: none — CMake's deliberate experimental-feature mechanism
  (`Help/dev/experimental.rst`), nothing to file

## macos-rsize-t-fixinclude

- symptom: the macOS SDK's `sys/_types/_rsize_t.h` assumes
  `__has_feature(modules)` implies clang and uses a clang-only `stddef.h`
  protocol, so under GCC `-fmodules` `rsize_t` never defines and the
  libstdc++ `std` module fails to compile — and libstdc++ silently
  installs a 1-byte `bits/std.cc` fallback (plus its modules.json entry)
  with exit 0
- sites: toolchain acquisition, not repository code — README.md "Known
  macOS toolchain issues": drop a plain-typedef copy of `_rsize_t.h` into
  GCC's `include-fixed/sys/_types/` and rebuild libstdc++; verify
  `include/c++/<ver>/bits/std.cc` is ~113 KB, not 1 byte
- workaround: the local fixinclude above (`upstream/
  gcc-fixincludes-darwin-rsize-t/fixed-header.h` is the production copy;
  the upstream patch rewrites the guard instead)
- retire: when the fixincludes patch lands in the pinned GCC (and the
  silent-empty-fallback report is resolved, so a failed std-module build
  can no longer masquerade as success)
- upstream: `upstream/gcc-fixincludes-darwin-rsize-t/` (format-patch + DCO,
  applies clean to trunk; final paired regression comparison in progress) and
  `upstream/libstdcxx-silent-empty-std-module/` (Bugzilla report, status
  SEND; corroboration Homebrew/homebrew-core#289142)
