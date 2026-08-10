# DO NOT FILE — duplicate of GCC PR 126223

The defaulted hidden-friend `operator==` importer ICE is already fully covered
by GCC PR c++/126223, “[15/16/17 Regression] Compiler fails with message
internal compiler error: Segmentation fault.”

Patrick Palka's comment 2 reduces that report to the same `mangle_module`
assertion at `cp/module.cc:16805`. Comment 3 then supplies a header-free C++20
defaulted hidden-friend `operator==` reproducer and records the same scope:

- by-value and `const&` hidden-friend forms ICE;
- `constexpr` is not required;
- C++20, C++23, and C++26 all fail;
- the defaulted member form works;
- a hand-written friend body works.

That is our testcase and triage matrix. A new report or another comment would
add no evidence. Keep `a.cc` and `b.cc` only as the local workaround's
provenance, and track PR 126223 for the upstream fix.

Checked against the GCC Bugzilla public mail archive on 2026-08-10. PR 126223
is confirmed `NEW`, has keyword `ice-on-valid-code`, targets 15.4, and is
known to fail on 15.3.0, 16.1.0, and 17 trunk.
