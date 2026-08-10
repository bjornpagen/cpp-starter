# unsafe/

Quarantine for machine interfaces the dialect cannot express directly. It may
include OS headers and use transient ABI pointers, but it does not relax the
ownership model.

Rules:

- every descriptor and allocation has one RAII owner,
- no raw pointer owns storage,
- no pointer to application or operation state is stored asynchronously,
- no kernel user-data field contains an address,
- syscall buffers are borrowed only for the duration of the syscall,
- readiness is returned as a typed fact to caller-owned state,
- every exported surface is owning, bounded, and typed.

## Network backend

`net.backend.cc` is the Darwin syscall adapter. `net.cc` exports the safe
`starter:net` partition.

The server has one owner thread and one `kevent64` loop. A fixed-capacity slot
array owns all active connections; configuration selects an active prefix.
Each slot contains a closed state variant, its descriptor, fixed
request/response buffers, progress counters, an absolute deadline, and a
generation. Kernel events contain only an encoded
`{slot, generation}` integer. Closing or expiring a slot advances its
generation, so a queued stale event cannot name the new occupant.

The loop processes signal events before ordinary readiness, expires absolute
deadlines, and performs accept/read/write transitions directly. It has no
worker pool, locks, callbacks, opaque self pointers, intrusive waiter nodes,
or sender operation-state borrows.

The narrow module/backend boundary carries an opaque uniquely owned server,
standard owning/error values, and one stateless synchronous handler function.
Views crossing that call are valid only for the call and are never retained.
The public handler above that trampoline sees no buffer: it consumes an owning
request and returns an owning response value. It executes inline on the owner
loop, so it must be bounded and nonblocking.
