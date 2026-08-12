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

- symptoms, both confined to pinned stdexec internals: Clang's stack-escape
  analyzer diagnoses intentional operation-state self-references as addresses
  of temporaries, while `performance-move-const-arg` asks generic sender code
  to stop expressing a semantically required move
- sites: `foreign/CMakeLists.txt` — both exclusions are scoped to
  `starter_exec_backend`; the checks remain enabled everywhere else
- workaround: preserve the sender lifetime/move protocol and disable only the
  two named diagnostics on the one vendor boundary
- retire: re-enable each item independently when a pinned Clang or stdexec bump
  compiles and analyzes this boundary without it
- upstream: none filed; these are interactions inside the pinned vendor
  expression implementation. The exported combinator behavior remains covered
  by the GCC tests, UBSan/ASan, and the Clang boundary graph.

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
- upstream: filed 2026-08-10 as PR c++/126783 ("[16/17 Regression] [modules]
  ICE when a GMF variable is later defined inline"). Bugzilla marks it
  ice-on-valid-code, a 15.2→16 regression (See Also PR 122551, whose fix
  introduced the crashing transfer_defining_module path), blocking the
  c++-modules meta-bug, milestone 16.3. The reproduction directory was
  removed from the tree after filing — recover from git history before
  commit `e0a9d27` if needed

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
- upstream: filed 2026-08-10 as PR target/126782 (See Also PR 116827); the
  silent empty-module fallback is filed separately as PR 126786
  (corroboration Homebrew/homebrew-core#289142). Maintainer feedback on the
  PR rejects the fixincludes approach for Darwin; the replacement patch adds
  `__need_rsize_t` support to GCC's `<stddef.h>` — plan and superseded patch
  in `upstream/gcc-fixincludes-darwin-rsize-t/`. The live local fixinclude
  remains `fixed-header.h` in that directory until the upstream fix lands

## gcc-darwin-lto-debug-dsymutil

- symptom: any `-flto -g` link on the pinned Darwin toolchain emits objects
  with invalid `__DWARF` sections (dangling DIE references, out-of-bounds
  `DW_AT_stmt_list`); archive members bypass the plugin-less LTO recompile,
  and the driver-run Apple dsymutil then grows without bound on the debug
  map (67 GB RSS in 13 s; one unguarded run kernel-panicked the host)
- sites: CMakeLists.txt — `check_ipo_supported` plus
  `CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON`: whole-program
  optimization is Release-only, and Release carries no `-g`, so no
  configuration links LTO objects while dsymutil runs
- workaround: never combine `-flto` with `-g` on this target; unsupported
  IPO is a configure failure, not a silent degrade
- retire: when the pinned GCC emits `__DWARF` sections that pass
  `dwarfdump --verify` under `-flto -g` (re-test the minimal repro in the
  upstream entry on every toolchain bump), and dsymutil survives the full
  dev-preset IPO link under a memory guard
- upstream: `upstream/gcc-lto-modules-debug-oom/` — 4-file guarded repro,
  trigger matrix, and growth-curve evidence; the Linux control is done
  (ELF fat LTO objects verify clean, so the corruption is Mach-O-only —
  file against the Darwin target side), then a follow-up Apple Feedback
  for dsymutil's unbounded warning loop

## gcc-darwin-fhardened

- symptom: `-fhardened` on aarch64-apple-darwin24 warns `'-fhardened' not
  supported for this target` with warning class 0 — under the profile's
  `-Werror` this is a hard error that `-Wno-hardened` and
  `-Wno-error=hardened` cannot demote — and the umbrella half-applies
  anyway (stack-protector-strong and trivial-auto-var-init=zero engage;
  stack-clash, `_FORTIFY_SOURCE`, and `_GLIBCXX_ASSERTIONS` are silently
  dropped with no `-Whardened` report)
- sites: CMakeLists.txt — the "Hardened codegen is unconditional" block on
  `starter_language_profile`
- workaround: never spell `-fhardened`; enable the working constituents
  individually (`-ftrivial-auto-var-init=zero`, `-fstack-protector-strong`,
  `-fzero-call-used-regs=used-gpr`, GNU-scoped `-fstack-clash-protection`)
- retire: replace the individual flags with `-fhardened` when the pinned GCC
  both classifies the unsupported-target warning under `-Whardened` and
  enables the umbrella (or accurate per-constituent reporting) on Darwin;
  re-test the `-E -dM` macro set on every toolchain bump
- upstream: `upstream/gcc-darwin-fhardened-coverage/` — one-line repro,
  constituent evidence, triage of configure.ac:7982 / toplev.cc:1642 /
  opts.cc / c-opts.cc:1747 verified; status: hold for the Linux control
  check, then Bugzilla `middle-end` plus a configure.ac Darwin-enablement
  patch to gcc-patches

## cmake-ipo-probe-ordering

- symptom: CMake's `check_ipo_supported` probe project inherits
  `CMAKE_CXX_FLAGS` but not `CMAKE_CXX_STANDARD`, and the pinned cc1plus
  rejects `-freflection` outside `-std=c++26`/`-std=gnu++26` — so an IPO
  check placed after the `-freflection` append reports the toolchain
  unsupported and the configure gate FATAL_ERRORs on a working compiler
- sites: CMakeLists.txt — `check_ipo_supported(LANGUAGES CXX)` deliberately
  precedes the `-freflection` `CMAKE_CXX_FLAGS` append (the in-tree comment
  states the constraint)
- workaround: keep the IPO probe ahead of every flag that is valid only at
  the project's language-standard level
- retire: when the pinned CMake's IPO probe honors `CMAKE_CXX_STANDARD` (or
  the pinned GCC accepts `-freflection` at any standard level); re-test by
  reordering the probe on every CMake or GCC bump
- upstream: none filed — documented CMake probe semantics interacting with a
  standard-gated GCC flag; no reduction produced
