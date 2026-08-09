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

## The net backend's concurrency model

`net.cc` exports the dialect-clean `starter:net` surface; every socket and
kevent syscall and every sender composition lives in `net.backend.cc`, a
plain (non-module) TU reached through an `extern "C++"` narrow ABI — the
I/O half of the stdexec swap boundary (the combinator half is
`foreign/exec.backend.cc`; why the boundary is two plain TUs: PINS.md
`gcc-gmf-stdexec-ice`). What crosses the ABI is concrete: an opaque Server
handle plus scalar-and-fn-pointer entry points.

The model is thread-per-core, share-nothing:

- One worker per pool thread — the pool has exactly one thread per worker,
  so this is thread-per-core, not oversubscription — and each worker owns
  its OWN kqueue reactor. No mutable state is shared between workers, and
  no lock is visible in or above the backend TU.
- ONE shared nonblocking loopback listener is armed in every worker's
  kqueue and the workers race `accept(2)`: a lost race is just EAGAIN,
  which re-arms. Per-worker SO_REUSEPORT listeners are deliberately NOT
  used — Darwin does not load-balance them (PINS.md
  `darwin-so-reuseport-no-lb`). Loopback keeps the example and tests off
  the host firewall.
- A worker's connection chain is a straight-line sender composition —

      async_accept | let_value( async_read | let_value( handler; async_write ))

  — restarted after every completion: accept, read the request head, run
  the handler into the response buffer, write the response, close (the
  connection is RAII-owned by the chain's operation state). The handler
  runs on the owning worker's thread.
- The reactor is a single-waiter scheduler by construction: the
  straight-line chain suspends on at most one fd at a time, so one
  armed-waiter slot is the entire scheduler state. Registrations are
  oneshot — the kernel deletes the event on delivery, so normal operation
  leaves no stale registrations behind — and on stop an armed waiter is
  completed through the stopped channel before its operation state is
  destroyed.
- The only cross-thread entries are `Ctx::request_stop` (a kevent
  NOTE_TRIGGER on the worker's own kqueue, thread-safe by the kevent
  contract) and the stop/join latch.
