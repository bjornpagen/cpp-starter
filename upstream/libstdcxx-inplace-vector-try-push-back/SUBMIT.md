# libstdc++: `inplace_vector<T, N>::try_push_back` returns `optional<T&>`, standard says `pointer`

- **Where:** GCC Bugzilla, component `libstdc++`
- **Kind:** conformance report with verified repro (`repro.cc`)
- **Verified:** g++-16 (GCC) 16.1.0; `__cpp_lib_inplace_vector == 202603L`

## Title

```
inplace_vector::try_push_back returns optional<T&> instead of pointer
```

## Body (paste)

P0843 ([inplace.vector.modifiers]) specifies:

```cpp
constexpr pointer try_push_back(const T& x);
constexpr pointer try_push_back(T&& x);
// Returns: nullptr if size() == capacity(), otherwise addressof(back()).
```

libstdc++'s implementation returns `std::optional<T&>` instead:

```cpp
#include <inplace_vector>
#include <type_traits>

int main() {
	std::inplace_vector<int, 2> v;
	static_assert(std::is_same_v<decltype(v.try_push_back(1)), int*>);  // fails
}
```

```
$ g++ -std=c++26 -c repro.cc
error: static assertion failed
note: the actual type is 'std::optional<int&>'
```

`try_emplace_back` and `try_append_range` should be checked for the same
divergence. `optional<T&>` is arguably the nicer API, but it is not what
[inplace.vector.modifiers] says, and it breaks portable code that tests
the result against `nullptr` (that is how we found it — code written to
the working draft failed to compile). If this tracks a paper we missed
that changed the return type post-P0843, apologies — a pointer to it
would be appreciated; we could not find one.

Environment: GCC 16.1.0, `-std=c++26`, feature macro 202603L.
