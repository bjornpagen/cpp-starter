# GCC ICE: defaulted hidden-friend `operator==` streamed across module import

- **Where:** GCC Bugzilla, component `c++`, keywords `ice-on-valid-code`; mention "modules" in the summary
- **Kind:** bug report with verified 2-file repro (`a.cc`, `b.cc`)
- **Verified:** g++-16 (GCC) 16.1.0, aarch64-apple-darwin24

## Title

```
[modules] ICE (segfault) using an imported class with defaulted hidden-friend operator==
```

## Body (paste)

A named module exporting a class whose `operator==` is a defaulted hidden
friend ICEs the *importer* when the operator is used:

```cpp
// a.cc
export module A;

export struct Mask {
	unsigned bits;
	friend constexpr bool operator==(Mask const&, Mask const&) = default;
};

// b.cc
import A;

int main() {
	return Mask{1} == Mask{1} ? 0 : 1;
}
```

```
$ g++ -std=c++26 -fmodules -c a.cc      # OK
$ g++ -std=c++26 -fmodules -c b.cc
In module A, imported at b.cc:1:
a.cc:5:31: internal compiler error: Segmentation fault: 11
    5 |         friend constexpr bool operator==(Mask const&, Mask const&) = default;
```

A defaulted *member* `operator==(Mask const&) const = default;` is
unaffected (that is the workaround we ship). Also reproduces with the
class in a module partition re-exported by a primary interface.

Environment: GCC 16.1.0, target aarch64-apple-darwin24; also reproduced
with `-std=c++20`. Preprocessed source is the repro itself (no includes).
