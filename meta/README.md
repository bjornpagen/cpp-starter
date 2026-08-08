# meta/

GCC-only C++26 reflection modules live here: reflection expressions, splicing,
`std::meta`, expansion statements, and annotations.

This is still dialect code — every AGENTS.md rule applies except that the
pinned Clang frontend cannot parse it yet, so it is excluded from the
`clang-lint` graph and checked by GCC diagnostics plus the repository policy
checker.

Module units live here as `.cppm`/`.cpp`, explicitly listed in a
`FILE_SET CXX_MODULES` — never globbed. The top-level `CMakeLists.txt` adds
this directory only when the compiler supports `-freflection` and the build
is not the lint graph. Tests that import these modules are likewise excluded
from the lint graph in `tests/CMakeLists.txt`.
