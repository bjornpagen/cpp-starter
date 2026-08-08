# foreign/

Quarantine for unavoidable external-interface adaptation: OS and libc headers,
vendor C APIs, and pinned dependencies whose native shape (headers, macros,
exceptions, shared ownership) may not leak into dialect code.

Rules:

- Code here may use headers and the preprocessor when the external interface
  requires them.
- Every adapter must export a safe named module (or narrow ABI) upward:
  `std::expected` errors, RAII ownership, spans/views — never raw pointers,
  error codes with out-params, or macro configuration.
- No dialect module may include a foreign header or depend on preprocessor
  state transitively.
- Do not move ordinary application code here to escape a rule.
