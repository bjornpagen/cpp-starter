# NOT YET REDUCED — do not file as-is (status re-confirmed 2026-08-09)

## Observed behavior (real, in production tree)

GCC 16.1.0: a NON-template member function *definition* in a module
partition, whose body instantiates `std::expected` APIs over types from
an imported module, corrupts that partition's BMI for re-export. The
primary interface's `export import :db;` then fails with:

```
failed to read compiled module cluster N: Bad file data
```

Template members are unaffected. Moving the five affected bodies to a
module implementation unit (which produces no BMI) works around it.

## Where it lives

- Pin + workaround: `bumbledb` repo, `cpp/src/db/db.cc` (comment at the
  declarations) and `cpp/src/db/db_impl.cc` (the sanctioned
  implementation unit, full rationale in its header comment).
- Faithful reproduction: in the bumbledb tree, move the bodies of
  `Db::admit` / pre-schema `create`/`open`/`ephemeral` / `Db::fingerprint`
  from `db_impl.cc` back into `db.cc` and build the `dev` preset — the
  primary interface compile fails with the cluster error.

## Reduction attempts in this directory (none reproduce; re-run 2026-08-09)

- `p.cc`/`m.cc`/`u.cc`: partition + `<expected>` in GMF + non-template
  member with monadic body; re-exported; imported. No corruption.
- `q.cc`/`p2.cc`/`m.cc`/`u2.cc`: same, with the `expected` payload type
  coming from a separately imported module (closer to the real shape).
  No corruption.
- NEW (2026-08-09): same shape with `import std` instead of textual
  `<expected>` (partition purview `import std`; driver `import std` +
  `import M`; std module built via
  `g++ -std=c++26 -fmodules -fmodule-only -fsearch-include-path bits/std.cc`).
  Also clean — so `import std` alone is not the missing ingredient.

Harness corrections made 2026-08-09 (previous "all compile CLEAN" was
only true modulo driver defects, which masked nothing but were sloppy):
`u.cc` was missing `#include <expected>` (GMF includes are correctly not
re-exported, so the driver failed with an ordinary name-lookup error);
`u2.cc` added because `u.cc` passed `expected<int,int>` where the p2.cc
variant takes `expected<q::handle,int>`. With those fixed, every attempt
compiles and the BMIs read back fine — the bug still does not reproduce
outside the production tree.

Build commands (each attempt from a clean dir, gcm.cache removed
between attempts): `g++ -std=c++26 -fmodules -c <files in dependency
order>`, g++-16 16.1.0, aarch64-apple-darwin24.

## Related upstream reports found 2026-08-09 (check before filing)

Bugzilla quicksearch `summary:"Bad file data"` and `summary:inplace_vector`:

- **PR 125595** — "Using inplace_vector across module boundaries causes
  corrupt gcm" (16.1.1, NEW): same user-visible `failed to read compiled
  module cluster N: Bad file data` on re-import; reduced upstream to a
  modules-streaming ICE on VLA-containing inline function
  (`std::start_lifetime_as_array`). Different library type than ours
  (`std::expected` has no VLA path), but the same "exporter silently
  writes a BMI the importer can't read" failure family — the eventual
  report should cite it.
- PR 125144, PR 125356 — further "Bad file data" cluster reports
  (UNCONFIRMED), 125356 reduced to a 4-partition template-instantiation
  chain. Same family, different shapes.

Given three open "Bad file data" reports with standalone repros already
in the queue, the bar for ours remains a standalone repro: "me too"
reports without one are not actionable.

## Next reduction steps (unchanged, minus the import-std hypothesis)

Reproduce inside a scratch CMake project mirroring the real two-module
graph (`bumbledb`/`bumbledb_foreign`) with the actual five member bodies,
then shrink (creduce-style, keeping the three-file structure). The
2026-08-09 session falsified the "`import std` is the missing ingredient"
hypothesis; the full foreign-module context is the remaining suspect.

File to GCC Bugzilla (component `c++`, modules) only once a standalone
repro exists.
