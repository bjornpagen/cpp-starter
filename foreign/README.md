# foreign/

Quarantine for unavoidable external-interface adaptation: OS and libc headers,
vendor C APIs, and pinned dependencies whose native shape (headers, macros,
exceptions, shared ownership) may not leak into dialect code.

Rules:

- Code here may use headers and the preprocessor when the external interface
  requires them.
- Every adapter must export a safe module partition (or narrow ABI) upward:
  `std::expected` errors, RAII ownership, spans/views — never raw pointers,
  error codes with out-params, or macro configuration.
- No dialect module may include a foreign header or depend on preprocessor
  state transitively.
- Do not move ordinary application code here to escape a rule.

Current residents:

- `exec.cc` — the `starter:exec` partition: the dialect-clean sender/receiver
  surface (module code, no headers).
- `exec.backend.cc` — the combinator half of the stdexec swap boundary
  (AGENTS.md §15); the I/O half is `unsafe/net.backend.cc`, and together the
  two are the entire stdexec spelling surface of the repository — no other
  file may include a stdexec header or spell `stdexec::`/`exec::`. The
  boundary is two plain (non-module) TUs reached through `extern "C++"`
  narrow ABIs because the pinned GCC ICEs whenever a stdexec header appears
  in any module unit (PINS.md `gcc-gmf-stdexec-ice`), so sender composition
  cannot cross the module boundary — only concrete function surfaces do.
  When the toolchain ships `__cpp_lib_senders` (tombstone in
  tests/conformance.test.cc), each TU's `namespace ex` re-binds to
  `std::execution`, the FetchContent pin is deleted, and importers of the
  partitions are untouched.
- stdexec is consumed from the maintained fork (bjornpagen/stdexec, branch
  `integration`) pinned in the top-level CMakeLists: fixes are real fork
  commits with pending upstream PRs, never configure-time patches. Policy
  lives in the fork's FORK.md; submission drafts in `upstream/`.
