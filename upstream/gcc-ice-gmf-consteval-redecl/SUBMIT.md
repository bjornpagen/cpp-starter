# GCC ICE: GMF declaration/definition pattern from stdexec segfaults cc1plus

- **Where:** GCC Bugzilla, component `c++`, keywords `ice-on-valid-code` (validity note below); mention "modules" in the summary
- **Kind:** bug report with verified 12-line repro (`repro.cc`), reduced from `#include <stdexec/execution.hpp>` in a module unit's global module fragment
- **Verified:** g++-16 (GCC) 16.1.0, aarch64-apple-darwin24

## Title

```
[modules] ICE (segfault) on GMF extern declaration followed by definition + inline constexpr object of consteval-operator type
```

## Body (paste)

Reduced from putting NVIDIA stdexec's `execution.hpp` in the global module
fragment of a module unit (the header declares `forwarding_query` before
defining its type):

```cpp
module;
namespace stdexec {
  struct forwarding_query_t;
  extern forwarding_query_t const forwarding_query;
}
namespace stdexec {
  struct forwarding_query_t {
    consteval auto operator()(int) const noexcept -> bool { return true; }
  };
  inline constexpr forwarding_query_t forwarding_query{};
}
export module m;
```

```
$ g++ -std=c++26 -fmodules -c repro.cc
repro.cc:10:39: internal compiler error: Segmentation fault: 11
    10 |     inline constexpr forwarding_query_t forwarding_query{};
```

Notes for triage:

- The same TU compiles fine as a non-module TU (delete the `module;` /
  `export module m;` lines).
- The reduction used hand-written declarations in the GMF for minimality;
  the original trigger is an ordinary `#include` in the GMF (GCC emits
  `-Wglobal-module` for the hand-written form, then ICEs regardless).
- The `consteval` call operator is load-bearing in the reduction; a plain
  member function does not ICE.
- Real-world impact: any module unit whose GMF includes stdexec (or a
  header with the same declare-then-define + `inline constexpr` object
  idiom, common for customization point objects) ICEs the compiler.

Environment: GCC 16.1.0, aarch64-apple-darwin24. Repro has no includes.
