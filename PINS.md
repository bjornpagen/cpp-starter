# PINS.md — the pinned-quirk registry

One entry per pinned workaround: code or build configuration that is
deliberately wrong by dialect law because the pinned toolchain or platform
requires it. Each `/* PIN(name) */` site in the tree points at its entry
here; the essay lives here, once.

Tombstone ritual: on every toolchain bump, read this file top to bottom,
re-test every retire condition, and delete what upstream fixed — one file,
one sweep.

Pinned toolchain: GCC 16.1.0 (g++-16, aarch64-apple-darwin24), CMake 4.2.x,
Clang 22.x (lint graph), stdexec consumed from the maintained fork pinned
in the top-level CMakeLists.txt.

## gcc-modules-defaulted-eq

- symptom: the importer ICEs (cc1plus segfault, at the operator's
  declaration) streaming a defaulted (friend or member) comparison of an
  exported partition type through the BMI
- sites: `starter::NetError::operator==` (unsafe/net.cc),
  `starter::ExecError::operator==` (foreign/exec.cc)
- workaround: spell the comparison out member-by-member instead of
  `= default`
- retire: re-try `= default` on the next toolchain bump
- upstream: `upstream/gcc-ice-modules-defaulted-friend-eq/` — 2-file repro
  re-verified, dupe search clean (GCC PR 122822 is see-also, not dup);
  status SEND (Bugzilla ref once filed)

## gcc-partition-bmi-inplace-vector

- symptom: a `std::inplace_vector` member of an exported partition type
  streams a BMI the importer rejects ("failed to read compiled module
  cluster: Bad file data")
- sites: `starter::RequestView` (src/http.cc) — the header list is spelled
  `std::array<HeaderView, max_header_count>` plus a `header_count` field
  instead of the blessed `std::inplace_vector<HeaderView,
  max_header_count>`; only `headers[0 .. header_count)` are meaningful
- workaround: array + count
- retire: re-try `inplace_vector` on the next toolchain bump
- upstream: no standalone repro yet; nearest tracked relative of the
  partition-BMI-corruption family is
  `upstream/gcc-modules-partition-bmi-expected/` (not yet filed —
  reduction attempts and plan in its NOTES.md; related GCC PR 125595,
  125144, 125356)

## gcc-gmf-stdexec-ice

- symptom: cc1plus segfaults whenever `stdexec/execution.hpp` is textually
  included in ANY module unit — interface partition, primary, or
  implementation unit, with or without `import std`. Root cause is a
  global-module-fragment variable declared `extern T const x;` and then
  defined `inline constexpr T x{};` — the stdexec CPO pattern; the CPO
  headers contain 54+ such load-bearing pairs, so it is not patchable.
  Consequence: sender composition cannot cross the module boundary on this
  toolchain — only concrete function surfaces do.
- sites: the whole two-TU swap boundary — `foreign/exec.backend.cc` (the
  combinator half) and `unsafe/net.backend.cc` (the I/O half) exist as
  plain (non-module) TUs; the `:exec` (foreign/exec.cc) and `:net`
  (unsafe/net.cc) partitions reach them through `extern "C++"` narrow ABIs
  (opaque handles, scalars, function pointers)
- workaround: every stdexec include stays in the two plain backend TUs; no
  other file in the repository may include a stdexec header or spell
  `stdexec::`/`exec::`
- retire: re-verify the pinned micro-repro on every toolchain bump; the
  boundary itself dissolves when the pinned toolchain ships
  `__cpp_lib_senders` (tombstone in tests/conformance.test.cc) — the two
  backend TUs re-bind `namespace ex` to `std::execution`, the FetchContent
  pin is deleted, and everything upward is untouched
- upstream: `upstream/gcc-ice-gmf-consteval-redecl/` — 4-line repro (no
  consteval needed; the directory name is historical), triage matrix
  verified, dupe search clean; status SEND

## gcc-template-for-wshadow

- symptom: expansion statements (`template for`) re-declare the loop
  variable in a nested scope per iteration, so any expansion over a range
  with more than one element trips `-Wshadow` on compiler-generated
  scoping (fires once per element) — a build failure under `-Werror`
- sites: tests/enums.test.cc and tests/conformance.test.cc, via scoped
  `set_source_files_properties(... COMPILE_OPTIONS -Wno-shadow)` in
  tests/CMakeLists.txt (a source-level property so it lands after the
  language profile's `-Wshadow` on the command line)
- workaround: `-Wno-shadow` only for the TUs that instantiate expansion
  statements; `-Wshadow` stays on everywhere else
- retire: delete the scoped suppressions when the fix for GCC PR 124197
  ships in the pinned toolchain; re-test on every toolchain bump
- upstream: GCC PR 124197 — CC + comment with our evidence (fires once per
  element; 16.1.0 + reflection variant) per upstream/README.md; no
  directory of our own

## darwin-so-reuseport-no-lb

- symptom: Darwin has no SO_REUSEPORT load balancing for TCP — that is
  Linux behavior, and FreeBSD's is the separate SO_REUSEPORT_LB. Darwin
  delivers every connection to the last-bound socket; verified empirically
  on this host (all requests landed on the last worker).
- sites: unsafe/net.backend.cc (`make_listener`, `server_start`) — ONE
  shared nonblocking loopback listener is armed in every worker's kqueue
  and the workers race `accept(2)` (a lost race is just EAGAIN, which
  re-arms) instead of per-worker SO_REUSEPORT listeners
- workaround: the shared raced listener
- retire: platform behavior, not a toolchain defect — re-test only if the
  deployment target changes or Darwin gains TCP reuseport load balancing;
  re-verify empirically before ever switching to per-worker listeners
- upstream: none — OS semantics, nothing to file

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
- sites: CMakeLists.txt — the hard configure gate on the CMake 4.2 series
  plus `set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD
  "d0edc3af-4c50-42ea-a356-e2862fe7a444")` (the 4.2-series value)
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
  status SEND — applies clean to trunk) and
  `upstream/libstdcxx-silent-empty-std-module/` (Bugzilla report, status
  SEND; corroboration Homebrew/homebrew-core#289142)
