# upstream/

Everything this project should send upstream, one directory per item.
Each ready item carries its repro (verified against the pinned toolchain:
g++-16 = GCC 16.1.0, aarch64-apple-darwin24, 2026-08-09) and a `SUBMIT.md`
with paste-ready title/body. Review and submit independently.

| Item | Kind | Target | Status | Summary |
|---|---|---|---|---|
| `stdexec-arm64-duplicate-inline` | patch (PR from fork `fix/arm64-duplicate-inline`) | NVIDIA/stdexec PR | **ready** (fix in production via the fork pin; hard GCC error verified at HEAD, clean fixed; clang warn-only explains their green CI) | ARM branch of `__spin_loop_pause.hpp` expands to `inline inline` since f0e8ae6f; 1-line fix |
| `stdexec-sync-wait-no-exceptions` | patch (PR from fork `fix/sync-wait-no-exceptions`) | NVIDIA/stdexec PR | **ready** (fix in production via the fork pin; both-modes verification matrix in the commit) | under `-fno-exceptions`, typed errors reach `sync_wait` as null `exception_ptr` and return `nullopt` — errors misreported as stopped; fix terminates per the library's own no-exceptions convention |
| `gcc-fixincludes-darwin-rsize-t` | patch | gcc-patches (fixincludes) | **ready** (fixed text in production since 2026-08-08; `__has_feature(modules)` trigger verified) | SDK `_rsize_t.h` assumes modules⇒clang stddef protocol; rsize_t never defines under GCC `-fmodules` |
| `libstdcxx-silent-empty-std-module` | bug report | GCC Bugzilla `libstdc++` | **ready** (reproduced during toolchain build) | Failed std-module compile silently installs 1-byte `bits/std.cc` with exit 0 |
| `gcc-ice-modules-defaulted-friend-eq` | bug report | GCC Bugzilla `c++` (ice-on-valid-code) | **ready** (2-file repro verified, segfault) | Importer ICEs using a class with defaulted hidden-friend `operator==` from a named module |
| `gcc-ice-gmf-consteval-redecl` | bug report | GCC Bugzilla `c++` (ice-on-valid-code) | **ready** (12-line include-free repro verified, segfault) | GMF declare-then-define + `inline constexpr` object of consteval-operator type ICEs; blocks stdexec in any GMF |
| `gcc-wshadow-expansion-statement` | bug report | GCC Bugzilla `c++` (diagnostic) | **ready** (repro verified; fires N−1 times for N-element ranges) | `template for` loop variable reported as shadowing itself |
| `libstdcxx-inplace-vector-try-push-back` | conformance | GCC Bugzilla `libstdc++` | **ready** (repro verified; actual type `optional<int&>`) | `try_push_back` returns `optional<T&>`, [inplace.vector.modifiers] says `pointer` |
| `gcc-modules-partition-bmi-expected` | bug report | GCC Bugzilla `c++` | **NOT READY** — reduction needed (see NOTES.md; 5 clean attempts preserved) | Non-template member body instantiating `std::expected` corrupts partition BMI for re-export ("Bad file data") |
| `gcc-asan-constexpr-string` | bug report | GCC Bugzilla `sanitizer` | **NOT READY** — reduction needed (see NOTES.md; 3 clean attempts preserved) | `std::string(ptr, size)` fails to constant-fold under ASan; iterator-pair ctor folds |

## Checked, conforming, not filed

- **`^^` on names introduced by using-declarations** (`^^std::uint64_t`
  → "cannot be applied to a using-declaration"): P2996R13 §4.1 makes
  reflect-expressions ill-formed "when the operand names a
  using-declarator" — GCC is conforming. Verified workarounds:
  `^^::uint64_t` (the underlying typedef), a local alias
  (`using u64 = std::uint64_t; ^^u64`), or resolution through a template
  parameter. This is what the in-tree pins already do.

## Not upstream material

- The `db_impl.cc` implementation-unit split and the scoped `-Wno-shadow`
  are workarounds for the two NOT-READY items above; they get deleted
  when the fixes land.
- The stdexec `sync_wait` error/stopped conflation graduated from
  "discussion material" to a real fix: `stdexec-sync-wait-no-exceptions`
  above, implemented on the fork following the library's own
  `STDEXEC_THROW -> __terminate()` no-exceptions convention, verified
  byte-identical with exceptions enabled. The fork
  (github.com/bjornpagen/stdexec, branch `integration`, policy in FORK.md)
  is the pinned source for this repository: every fork commit has a
  pending upstream PR, and the fork's success condition is its own
  emptiness.
