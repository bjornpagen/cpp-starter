# NOT YET REDUCED — do not file as-is

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

## Reduction attempts in this directory (all compile CLEAN — none reproduce)

- `p.cc`/`m.cc`/`u.cc`: partition + `<expected>` in GMF + non-template
  member with monadic body; re-exported; imported. No corruption.
- `q.cc`/`p2.cc`: same, with the `expected` payload type coming from a
  separately imported module (closer to the real shape). No corruption.

The real trigger plausibly needs `import std` (not textual `<expected>`)
and/or the full foreign-module context. Next reduction steps: reproduce
inside a scratch CMake project with `import std` + a two-module graph
mirroring `bumbledb`/`bumbledb_foreign`, then shrink from there
(creduce-style, keeping the three-file structure).

File to GCC Bugzilla (component `c++`, modules) only once a standalone
repro exists — "Bad file data" reports without one are hard to action.
