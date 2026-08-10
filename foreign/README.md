# foreign/

Quarantine for pinned dependencies and external interfaces whose headers,
macros, or native APIs may not leak into dialect code.

The ownership law still applies: every project-owned allocation has one RAII
owner; raw pointers are transient ABI borrows; no raw ownership crosses the
module boundary.

Current residents:

- `exec.cc` exports the dialect-clean `starter:exec` partition.
- `exec.backend.cc` is the repository's one stdexec spelling surface. The
  pinned GCC cannot include stdexec in a module unit, so concrete conformance
  operations cross an `extern "C++"` boundary. No other file may include a
  stdexec header or spell `stdexec::`/`exec::`.

NVIDIA stdexec is pinned to an immutable upstream revision in the top-level
CMake file. The `PINS.md` toolchain-bump ritual requires checking for native
senders, then deleting the vendor and rebinding this one boundary to
`std::execution` as soon as they are available.

The Clang lint target keeps its boundary checks enabled here except for the two
stdexec-expression false positives registered in `PINS.md`. Those exclusions
are target-scoped; they do not weaken analysis of the project-owned reactor.
