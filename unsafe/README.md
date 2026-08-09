# unsafe/

Quarantine for machine-level primitives that the dialect forbids elsewhere:
compiler intrinsics, syscalls, atomics and locks used to implement
higher-level concurrency primitives, pointer arithmetic required by an ABI or
memory primitive, and `reinterpret_cast` required by a boundary.

Rules:

- Every primitive must export a safe module partition (or narrow ABI)
  upward.
- `std::mutex`, `std::atomic`, explicit memory orders, and friends are only
  legal here, and only to implement an approved abstraction (actor,
  serialized executor, arena, ...).
- Do not move ordinary application code here to escape a rule.
