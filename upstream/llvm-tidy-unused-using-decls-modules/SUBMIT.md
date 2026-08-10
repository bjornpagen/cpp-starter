# clang-tidy: `misc-unused-using-decls` false positive on `export using` in module interface units

- **Where:** github.com/llvm/llvm-project issues (label `clang-tidy` — triagers add it if you can't self-label)
- **Kind:** bug report — **already fixed on main**; the only thing left to file is an optional release-backport request (paste-ready below)
- **Verified:** clang-tidy 22.1.8 (`/opt/local/libexec/llvm-22/bin/clang-tidy`, "LLVM version 22.1.8, Optimized build") reproduces all four false positives; trunk (clang-tidy-24 nightly, apt.llvm.org, "Debian LLVM version 24.0.0") does **not** — checked empirically 2026-08-09 in a short-lived container, with a still-warning true-positive control proving the silence is a genuine negative
- **Verdict:** **DO NOT FILE the bug** — exact duplicate exists and is closed-as-completed: [llvm/llvm-project#162619](https://github.com/llvm/llvm-project/issues/162619) "`misc-unused-using-decls` false positive on exported usings" (reporter had `export using ::IDirectMusic;`, byte-for-byte our `bridge.cc` pattern; maintainer ChuanqiXu9: "We shouldn't treat any exported decl as unused"). Fixed by merged PR [llvm/llvm-project#183638](https://github.com/llvm/llvm-project/pull/183638) "[clang-tidy] Teach misc-unused-using-decls that exported using-decls aren't unused" (author localspook), commit `ce6a3d98cc3e208e8a4014b7812515951bf048ce`, merged 2026-02-28
- **No fix authored, no fork branch:** upstream main already carries the fix, so there is no patch in this packet and nothing was pushed to bjornpagen/llvm-project
- **Local pin this retires:** bumbledb `cpp/PINS.md` entry `llvm22-unused-using-decls` (per-target `--checks=-misc-unused-using-decls` on the `bumbledb_foreign` lint target) — delete the disable when the pinned clang-tidy contains `ce6a3d9`

## Fix on main (source-level verification)

`clang-tools-extra/clang-tidy/misc/UnusedUsingDeclsCheck.cpp` on main handles
module export semantics in `UnusedUsingDeclsCheck::check()` where the "using"
node is registered (lines 103–107):

```cpp
// Ignore exported using-decls.
if (Using->hasOwningModule() &&
    Using->getModuleOwnershipKind() <=
        Decl::ModuleOwnershipKind::VisibleWhenImported)
  return;
```

The early return fires before `UsingDeclContext` creation, so exported
using-decls never enter `Contexts`/`UsingTargetDeclsCache` — no diagnostic
**and** no removal fix-it (the `FixItHint::CreateRemoval` in
`onEndOfTranslationUnit` only runs for registered contexts), so the
API-deleting auto-fix hazard is gone too.

Root cause pre-fix: `check()` unconditionally registered every `UsingDecl`
matched by `usingDecl(isExpansionInMainFile())` and only unregistered targets
seen used inside the same TU; an exported using-decl's users are external by
definition, so it always survived to `onEndOfTranslationUnit` and was flagged.

## Duplicate search (gh search issues --repo llvm/llvm-project, open+closed, 2026-08-09)

1. `misc-unused-using-decls module` → 0 hits
2. `unused-using-decls export` → 0 hits
3. `unused-using-decls` → ~30 hits; best match **#162619** (CLOSED completed
   2026-02-28 = **our bug**). Near-misses that are NOT our bug: #109737 (UDL
   `operator""` template FP), #53444 (user-defined literals FP, closed),
   #34366 (extend to typedefs, feature request), #179443 (class-scope decls
   not reported), #46479 (function-scope not reported), #83579 (redundant-
   using check request), #72300 (perf)
4. `clang-tidy export using` → 0 hits
5. `unused using declaration module export` → 0 hits
6. `misc-unused-using-decls export` → 0 hits

(GitHub's issue search only matched on the bare check name; the module/export
word combos all returned empty.)

## If you want the fix in a release: backport request (paste-ready)

Pre-flight (don't skip): check whether the newest active release branch
already contains the fix — `git branch -r --contains ce6a3d98cc3e` in an
llvm-project clone, or check the release notes for
`misc-unused-using-decls`. If the current release already has it, file
nothing; just bump the clang-tidy pin and delete the local check-disable.
Otherwise, per LLVM's backport process (llvm.org/docs/GitHub.html), file the
issue below with the active release milestone, then comment
`/cherry-pick ce6a3d98cc3e208e8a4014b7812515951bf048ce` on it to trigger the
automation.

### Title

```
[clang-tidy] Backport ce6a3d98cc3e: misc-unused-using-decls false positive on exported using-decls (fix for #162619)
```

### Body (paste)

~~~
Requesting a release-branch backport of commit ce6a3d98cc3e208e8a4014b7812515951bf048ce ("[clang-tidy] Teach misc-unused-using-decls that exported using-decls aren't unused", #183638, fixes #162619).

Rationale: on every released clang-tidy up to and including 22.1.8, `misc-unused-using-decls` flags **every** `export using` declaration in a C++20/23 module interface unit as unused, and its fix-it deletes the module's exported API. Any project that re-exports names through a named module (the standard way to build a module facade over a C library) has to disable the whole check on those TUs.

Verified with:

```
$ /opt/local/libexec/llvm-22/bin/clang-tidy --version
LLVM (http://llvm.org/):
  LLVM version 22.1.8
  Optimized build.
```

Minimal repro (imports nothing, parses standalone — `clang++ -std=c++23 -fsyntax-only` succeeds; identical result under -std=c++20):

```cpp
// v1_export_using_function.cc
module;
extern "C" int bdb_open(int fd);
export module repro;
export using ::bdb_open;
```

```
$ clang-tidy v1_export_using_function.cc --checks='-*,misc-unused-using-decls' --quiet -- -std=c++23
v1_export_using_function.cc:5:16: warning: using decl 'bdb_open' is unused [misc-unused-using-decls]
v1_export_using_function.cc:5:16: note: remove the using
```

Variant matrix (clang-tidy 22.1.8, same command form throughout):

| # | Variant | Result |
|---|---|---|
| 1 | `export using ::f;` (function) | flagged — FALSE POSITIVE |
| 2 | `export { using ::f; }` (block form) | flagged — FALSE POSITIVE |
| 3 | `export using ::T;` (struct) / `export using ::v;` (variable) | both flagged — FALSE POSITIVES |
| 4 | plain non-exported `using ::f;` in module purview, unused | flagged — true positive (control; the fix on main preserves it) |
| 5 | `export using ::f;` where f is also called inside the module | not flagged (internal use suppresses; the "used" analysis ignored export status entirely) |
| 6 | unused `using bdb::bdb_open;` in a plain non-module TU | flagged — true-positive baseline |

Fix-it hazard (why this is worth a backport, not just an annoyance): applying the emitted fix (`--fix`, or the exported replacements — Offset 133, Length 18 in the fixes YAML) removes `using ::bdb_open;` but not the `export` keyword, leaving a bare `export` — the "fixed" file no longer parses. In the block form (variant 2) the result still compiles and silently strips the module's exported API.

Trunk is fixed: clang-tidy-24 nightly (apt.llvm.org, "Debian LLVM version 24.0.0") emits no diagnostic on the repro (exit 0), while the true-positive control (same file with plain `using ::f;` not exported) still warns:

```
/tmp/w/control.cc:4:9: warning: using decl 'f' is unused [misc-unused-using-decls]
```

confirming the check is functional and the silence is a genuine negative.
~~~

## Files in this packet

- `v1_export_using_function.cc` … `v6_plain_tu_unused.cc` — the six-variant
  characterization matrix, verbatim as run
- `v1_fixed.cc`, `v2_fixed.cc` — what clang-tidy 22.1.8 `--fix` actually
  produces (v1: bare dangling `export`, no longer parses; v2: compiles but
  the exported API is silently gone)
- `v1_fixes.yaml` — the exported replacements (`--export-fixes`) showing the
  removal at Offset 133, Length 18
