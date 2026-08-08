# meta/

Toolchain quarantine, not an architectural layer. Code lands here for exactly
one reason: it uses C++26 reflection syntax (reflection expressions, splicing,
`std::meta`, expansion statements, annotations) that the pinned Clang frontend
cannot parse yet, so it must be excluded from the `clang-lint` graph.

The code inside is ordinary `starter::` functionality — `starter.enums`
exports `starter::enum_name`, not a `starter::meta` namespace, and nothing
about it is "meta" from the caller's point of view. Every AGENTS.md rule
applies; the only concession is that these translation units are checked by
GCC diagnostics plus code review instead of clang-tidy (AGENTS.md §34).
When Clang learns to parse reflection, the contents move back next to their
callers and this directory disappears.

Module units live here as `.cppm`/`.cpp`, explicitly listed in a
`FILE_SET CXX_MODULES` — never globbed. The top-level `CMakeLists.txt`
excludes this directory only from the lint graph. Tests that import these
modules are likewise excluded from the lint graph in `tests/CMakeLists.txt`.

Known GCC 16.1 quirk: expansion statements (`template for`) re-declare the
loop variable per iteration in a nested scope, tripping `-Wshadow` when the
expanded range has more than one element; affected instantiating TUs carry a
source-level `-Wno-shadow` in their CMakeLists with a pinned comment.
