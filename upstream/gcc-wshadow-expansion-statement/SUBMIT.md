# GCC: -Wshadow false positive on expansion statements (`template for`)

- **Where:** GCC Bugzilla, component `c++`, keyword `diagnostic`
- **Kind:** bug report with verified repro (`repro.cc`)
- **Verified:** g++-16 (GCC) 16.1.0, aarch64-apple-darwin24

## Title

```
-Wshadow fires on the expansion-statement loop variable shadowing itself
```

## Body (paste)

For any expansion statement whose range has more than one element, the
per-iteration re-instantiation of the loop variable trips `-Wshadow` —
the variable is reported as shadowing *itself*, at its own source
location, once per extra element:

```cpp
#include <meta>

enum class E { A, B };

consteval int count() {
	int total = 0;
	template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^E))) {
		total += 1;
	}
	return total;
}

static_assert(count() == 2);

int main() {}
```

```
$ g++ -std=c++26 -freflection -Wshadow -c repro.cc
repro.cc:7:38: warning: declaration of 'e' shadows a previous local [-Wshadow]
repro.cc:7:38: note: shadowed declaration is here     (same location)
repro.cc:7:38: warning: declaration of 'e' shadows a previous local [-Wshadow]
repro.cc:7:38: note: shadowed declaration is here
```

With a 1-element range there is no warning. The user wrote one
declaration; the "shadowing" is between compiler-generated per-iteration
scopes, which the user cannot rename or restructure away. Under `-Werror`
this makes `-Wshadow` and expansion statements mutually exclusive —
reflection-heavy codebases must blanket-disable `-Wshadow` for every TU
that instantiates a `template for` (our production workaround).

Expected: expansion-statement instantiations should not shadow-check the
loop variable against its own siblings from other iterations.

Environment: GCC 16.1.0, aarch64-apple-darwin24, `-std=c++26 -freflection`.
