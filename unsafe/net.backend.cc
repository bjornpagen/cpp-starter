// net.backend.cc — the I/O half of the sender/receiver swap boundary: the
// kqueue-backed io-context whose senders complete on readiness. Together
// with foreign/exec.backend.cc this is the entire stdexec spelling surface
// of the repository; when the pinned toolchain ships __cpp_lib_senders (see
// the tombstone in tests/conformance.test.cc) the sender vocabulary below
// re-binds to std::execution and everything upward is untouched.
//
// Why a plain (non-module) TU: GCC 16.1 ICEs whenever a stdexec header is
// textually included in ANY module unit (global-module-fragment CPO pattern,
// pinned in foreign/exec.backend.cc), so senders cannot cross the module
// boundary on this toolchain. The :net partition (unsafe/net.cc) reaches
// this machinery through the same extern "C++" narrow ABI the :simd and
// :exec partitions use, and what crosses is concrete: an opaque Server
// handle plus scalar-and-fn-pointer entry points.
//
// Concurrency model: thread-per-core share-nothing. Each worker owns its
// OWN kqueue reactor (Ctx); the ONE shared nonblocking listener is armed in
// every worker's kqueue and the workers race accept(2) — a lost race is just
// EAGAIN, which re-arms (the pattern AcceptOp already implements). Per-worker
// SO_REUSEPORT listeners are deliberately NOT used: Darwin has no
// SO_REUSEPORT load balancing for TCP (that is Linux behavior, FreeBSD's is
// the separate SO_REUSEPORT_LB) — it delivers every connection to the
// last-bound socket, verified empirically on this host (all requests landed
// on the last worker). A worker's connection chain is a straight-line sender
// composition —
//
//   async_accept | let_value( async_read | let_value( handler; async_write ))
//
// — restarted after every completion. No state is shared between workers;
// the only cross-thread entries are Ctx::request_stop (a kevent NOTE_TRIGGER
// on the worker's kqueue, thread-safe by the kevent contract) and the
// stop/join latch. No lock is visible above this TU.
#include <algorithm>
#include <array>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <exception>
#include <latch>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <thread>
#include <utility>

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>

#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

// The vocabulary namespace, mirroring foreign/exec.backend.cc: this TU
// composes with exactly these combinators; the eventual swap re-points them
// at std::execution.
namespace ex {

using stdexec::just;
using stdexec::let_value;
using stdexec::starts_on;
using stdexec::then;

using exec::static_thread_pool;

} // namespace ex

namespace starter::net_backend {

using RawHandler = std::size_t (*)(std::uint32_t worker, char const* data, std::size_t size, char* out, std::size_t capacity);

namespace {

inline constexpr std::uint32_t max_workers = 4;
inline constexpr std::size_t buffer_capacity = 8192;
inline constexpr int listen_backlog = 256;
inline constexpr std::uintptr_t stop_ident = 1;

// Failure stages, mirrored by starter::NetStage in unsafe/net.cc — the two
// lists must stay in sync (narrow scalar ABI).
inline constexpr std::int32_t stage_socket = 1;
inline constexpr std::int32_t stage_option = 2;
inline constexpr std::int32_t stage_bind = 3;
inline constexpr std::int32_t stage_listen = 4;
inline constexpr std::int32_t stage_nonblock = 5;
inline constexpr std::int32_t stage_queue = 6;
inline constexpr std::int32_t stage_resolve = 7;
inline constexpr std::int32_t stage_accept = 8;
inline constexpr std::int32_t stage_read = 9;
inline constexpr std::int32_t stage_write = 10;

// The typed error of every I/O sender below: which syscall stage failed and
// its errno. Flows through the sender error channel as a value, never as an
// exception_ptr (AGENTS.md §11).
struct NetErr {
	std::int32_t stage;
	std::int32_t code;
};

// EV_SET is a field-assignment macro whose implicit int conversions trip
// -Wconversion at the expansion site; this helper is the same thing with the
// conversions spelled.
auto make_event(std::uintptr_t ident, std::int16_t filter, std::uint16_t flags, std::uint32_t fflags, void* udata) noexcept
    -> struct ::kevent {
	struct ::kevent ev{};
	ev.ident = ident;
	ev.filter = filter;
	ev.flags = flags;
	ev.fflags = fflags;
	ev.data = 0;
	ev.udata = udata;
	return ev;

}

auto set_nonblocking(int fd) noexcept -> std::int32_t {
	auto const flags = ::fcntl(fd, F_GETFL, 0);
	if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
		return errno;
	}
	return 0;
}

// The readiness continuation a suspended operation parks in the reactor.
// SAFETY: `self` is the operation state, whose address is stable from
// connect until completion; the reactor invokes `fn` at most once, on the
// worker's own thread, either on kevent delivery or on cancellation.
struct Waiter {
	void (*fn)(void* self, bool canceled);
	void* self;
};

// One worker's kqueue reactor. Single-threaded by construction: everything
// except request_stop happens on the owning worker thread, and a worker's
// straight-line connection chain suspends on at most one fd at a time, so a
// single armed-waiter slot is the whole scheduler state.
class Ctx {
public:
	Ctx() = default;
	Ctx(Ctx const&) = delete;
	auto operator=(Ctx const&) -> Ctx& = delete;
	Ctx(Ctx&&) = delete;
	auto operator=(Ctx&&) -> Ctx& = delete;

	~Ctx() {
		if (kq_ >= 0) {
			::close(kq_);
		}
	}

	[[nodiscard]] auto init() noexcept -> std::int32_t {
		kq_ = ::kqueue();
		if (kq_ < 0) {
			return errno;
		}
		auto ev = make_event(stop_ident, EVFILT_USER, EV_ADD | EV_CLEAR, 0, nullptr);
		if (::kevent(kq_, &ev, 1, nullptr, 0, nullptr) < 0) {
			return errno;
		}
		return 0;
	}

	// Parks `waiter` until `fd` is ready for `filter` (oneshot: the kernel
	// deletes the event on delivery, so normal operation leaves no stale
	// registrations behind).
	[[nodiscard]] auto arm(int fd, std::int16_t filter, Waiter* waiter) noexcept -> std::int32_t {
		auto ev = make_event(static_cast<std::uintptr_t>(fd), filter, EV_ADD | EV_ONESHOT, 0, waiter);
		if (::kevent(kq_, &ev, 1, nullptr, 0, nullptr) < 0) {
			return errno;
		}
		armed_ = waiter;
		return 0;
	}

	// Blocks for one kevent batch and dispatches it. The stop event only
	// flips `stopping`; the run loop in run_worker owns the consequences.
	auto run_one() noexcept -> void {
		dispatch(nullptr);
	}

	// Non-blocking dispatch: lets a worker that keeps completing chains
	// synchronously (a saturated accept queue) still observe a pending stop.
	auto poll() noexcept -> void {
		auto const zero = ::timespec{.tv_sec = 0, .tv_nsec = 0};
		dispatch(&zero);
	}

	// Completes the suspended operation through its stopped channel so the
	// operation state is quiescent before it is destroyed (destroying a
	// started-but-uncompleted operation state is undefined in sender-land).
	auto cancel_armed() noexcept -> void {
		if (armed_ == nullptr) {
			return;
		}
		auto* waiter = std::exchange(armed_, nullptr);
		waiter->fn(waiter->self, true);
	}

	[[nodiscard]] auto has_armed() const noexcept -> bool {
		return armed_ != nullptr;
	}

	// SAFETY: the one cross-thread entry point. kevent(2) on a shared kqueue
	// descriptor is thread-safe; NOTE_TRIGGER wakes the owning worker, which
	// does all actual stopping on its own thread.
	auto request_stop() noexcept -> void {
		auto ev = make_event(stop_ident, EVFILT_USER, 0, NOTE_TRIGGER, nullptr);
		static_cast<void>(::kevent(kq_, &ev, 1, nullptr, 0, nullptr));
	}

	bool stopping = false;

private:
	auto dispatch(::timespec const* timeout) noexcept -> void {
		auto events = std::array<struct ::kevent, 4>{};
		auto const count = ::kevent(kq_, nullptr, 0, events.data(), static_cast<int>(events.size()), timeout);
		if (count < 0) {
			if (errno != EINTR) {
				stopping = true; // the reactor itself is broken: quiesce
			}
			return;
		}
		for (auto const& ev : std::span{events}.first(static_cast<std::size_t>(count))) {
			if (ev.filter == EVFILT_USER) {
				stopping = true;
				continue;
			}
			auto* waiter = static_cast<Waiter*>(ev.udata);
			if (waiter == nullptr || waiter != armed_) {
				continue; // stale oneshot from an already-canceled operation
			}
			armed_ = nullptr;
			waiter->fn(waiter->self, false);
		}
	}

	int kq_ = -1;
	Waiter* armed_ = nullptr;
};

// An accepted connection: the fd plus its request/response buffers. Owned by
// the connection chain (it lives in let_value's operation state), closed by
// RAII when the chain's operation state is destroyed.
class Conn {
public:
	Conn() = default;

	explicit Conn(int fd) noexcept : fd_{fd} {}

	Conn(Conn&& other) noexcept : in{other.in}, out{other.out}, fd_{std::exchange(other.fd_, -1)} {}

	auto operator=(Conn&& other) noexcept -> Conn& {
		if (this != &other) {
			close_fd();
			in = other.in;
			out = other.out;
			fd_ = std::exchange(other.fd_, -1);
		}
		return *this;
	}

	Conn(Conn const&) = delete;
	auto operator=(Conn const&) -> Conn& = delete;

	~Conn() {
		close_fd();
	}

	[[nodiscard]] auto fd() const noexcept -> int {
		return fd_;
	}

	std::array<char, buffer_capacity> in{};
	std::array<char, buffer_capacity> out{};

private:
	auto close_fd() noexcept -> void {
		if (fd_ >= 0) {
			::close(std::exchange(fd_, -1));
		}
	}

	int fd_ = -1;
};

// --- async_accept(Ctx&, listener) -> sender of Conn ------------------------

template<class Rcvr>
struct AcceptOp {
	Rcvr rcvr;
	Ctx* ctx;
	int lfd;
	Waiter waiter{};

	auto start() & noexcept -> void {
		attempt();
	}

	auto attempt() noexcept -> void {
		for (;;) {
			auto const fd = ::accept(lfd, nullptr, nullptr);
			if (fd >= 0) {
				if (auto const rc = set_nonblocking(fd); rc != 0) {
					::close(fd);
					stdexec::set_error(std::move(rcvr), NetErr{stage_nonblock, rc});
					return;
				}
				auto const one = 1;
				static_cast<void>(::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one));
				stdexec::set_value(std::move(rcvr), Conn{fd});
				return;
			}
			if (errno == EINTR || errno == ECONNABORTED) {
				continue;
			}
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				waiter = Waiter{&AcceptOp::ready, this};
				if (auto const rc = ctx->arm(lfd, EVFILT_READ, &waiter); rc != 0) {
					stdexec::set_error(std::move(rcvr), NetErr{stage_queue, rc});
				}
				return;
			}
			stdexec::set_error(std::move(rcvr), NetErr{stage_accept, errno});
			return;
		}
	}

	static auto ready(void* self, bool canceled) noexcept -> void {
		auto& op = *static_cast<AcceptOp*>(self);
		if (canceled) {
			stdexec::set_stopped(std::move(op.rcvr));
			return;
		}
		op.attempt();
	}
};

struct AcceptSender {
	using sender_concept = stdexec::sender_t;
	using completion_signatures =
	    stdexec::completion_signatures<stdexec::set_value_t(Conn), stdexec::set_error_t(NetErr), stdexec::set_stopped_t()>;

	Ctx* ctx;
	int lfd;

	template<class Rcvr>
	auto connect(Rcvr rcvr) const noexcept -> AcceptOp<Rcvr> {
		return {std::move(rcvr), ctx, lfd};
	}
};

auto async_accept(Ctx& ctx, int listener_fd) noexcept -> AcceptSender {
	return {&ctx, listener_fd};
}

// --- async_read(Ctx&, fd, span) -> sender of size ---------------------------

// Some: complete after the first successful read (the generic form).
// HttpHead: keep reading until the request head terminator (CRLF CRLF) has
// arrived, the buffer is full, or the peer half-closed — the server's torn-
// buffer loop, folded into the operation so no dynamic sender recursion is
// needed. Either way the completion value is the total byte count (0 = EOF
// before any data).
enum class ReadUntil : std::uint8_t {
	Some,
	HttpHead,
};

template<class Rcvr>
struct ReadOp {
	Rcvr rcvr;
	Ctx* ctx;
	int fd;
	std::span<char> buffer;
	ReadUntil until;
	std::size_t received = 0;
	Waiter waiter{};

	auto start() & noexcept -> void {
		attempt();
	}

	auto attempt() noexcept -> void {
		for (;;) {
			if (received == buffer.size()) {
				stdexec::set_value(std::move(rcvr), received);
				return;
			}
			auto const count = ::read(fd, buffer.data() + received, buffer.size() - received);
			if (count > 0) {
				received += static_cast<std::size_t>(count);
				if (until == ReadUntil::Some || received == buffer.size() || head_complete()) {
					stdexec::set_value(std::move(rcvr), received);
					return;
				}
				continue;
			}
			if (count == 0) {
				stdexec::set_value(std::move(rcvr), received); // EOF
				return;
			}
			if (errno == EINTR) {
				continue;
			}
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				waiter = Waiter{&ReadOp::ready, this};
				if (auto const rc = ctx->arm(fd, EVFILT_READ, &waiter); rc != 0) {
					stdexec::set_error(std::move(rcvr), NetErr{stage_queue, rc});
				}
				return;
			}
			stdexec::set_error(std::move(rcvr), NetErr{stage_read, errno});
			return;
		}
	}

	[[nodiscard]] auto head_complete() const noexcept -> bool {
		return std::string_view{buffer.data(), received}.find("\r\n\r\n") != std::string_view::npos;
	}

	static auto ready(void* self, bool canceled) noexcept -> void {
		auto& op = *static_cast<ReadOp*>(self);
		if (canceled) {
			stdexec::set_stopped(std::move(op.rcvr));
			return;
		}
		op.attempt();
	}
};

struct ReadSender {
	using sender_concept = stdexec::sender_t;
	using completion_signatures =
	    stdexec::completion_signatures<stdexec::set_value_t(std::size_t), stdexec::set_error_t(NetErr), stdexec::set_stopped_t()>;

	Ctx* ctx;
	int fd;
	std::span<char> buffer;
	ReadUntil until;

	template<class Rcvr>
	auto connect(Rcvr rcvr) const noexcept -> ReadOp<Rcvr> {
		return {std::move(rcvr), ctx, fd, buffer, until};
	}
};

auto async_read(Ctx& ctx, int fd, std::span<char> buffer, ReadUntil until) noexcept -> ReadSender {
	return {&ctx, fd, buffer, until};
}

// --- async_write(Ctx&, fd, span) -> sender of size ---------------------------

// Writes the whole span (partial writes and EAGAIN are folded into the
// operation), completing with the total byte count.
template<class Rcvr>
struct WriteOp {
	Rcvr rcvr;
	Ctx* ctx;
	int fd;
	std::span<char const> data;
	std::size_t sent = 0;
	Waiter waiter{};

	auto start() & noexcept -> void {
		attempt();
	}

	auto attempt() noexcept -> void {
		for (;;) {
			if (sent == data.size()) {
				stdexec::set_value(std::move(rcvr), sent);
				return;
			}
			auto const count = ::write(fd, data.data() + sent, data.size() - sent);
			if (count >= 0) {
				sent += static_cast<std::size_t>(count);
				continue;
			}
			if (errno == EINTR) {
				continue;
			}
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				waiter = Waiter{&WriteOp::ready, this};
				if (auto const rc = ctx->arm(fd, EVFILT_WRITE, &waiter); rc != 0) {
					stdexec::set_error(std::move(rcvr), NetErr{stage_queue, rc});
				}
				return;
			}
			stdexec::set_error(std::move(rcvr), NetErr{stage_write, errno});
			return;
		}
	}

	static auto ready(void* self, bool canceled) noexcept -> void {
		auto& op = *static_cast<WriteOp*>(self);
		if (canceled) {
			stdexec::set_stopped(std::move(op.rcvr));
			return;
		}
		op.attempt();
	}
};

struct WriteSender {
	using sender_concept = stdexec::sender_t;
	using completion_signatures =
	    stdexec::completion_signatures<stdexec::set_value_t(std::size_t), stdexec::set_error_t(NetErr), stdexec::set_stopped_t()>;

	Ctx* ctx;
	int fd;
	std::span<char const> data;

	template<class Rcvr>
	auto connect(Rcvr rcvr) const noexcept -> WriteOp<Rcvr> {
		return {std::move(rcvr), ctx, fd, data};
	}
};

auto async_write(Ctx& ctx, int fd, std::span<char const> data) noexcept -> WriteSender {
	return {&ctx, fd, data};
}

// --- the worker: one reactor, the shared listener, one restarting chain ----

struct WorkerCore {
	Ctx ctx;
	int lfd = -1;
	std::uint32_t id = 0;
	RawHandler handler = nullptr;
	bool restart = false;
	bool finished = false;
};

// One full connection: accept -> read the request head -> run the handler
// into the response buffer -> write the response -> close (Conn RAII, when
// the chain's operation state is destroyed on restart). The handler runs on
// this worker's thread; `c` lives in let_value's operation state, which
// outlives every inner sender borrowing it.
auto make_chain(WorkerCore& w) {
	return async_accept(w.ctx, w.lfd) | ex::let_value([&w](Conn& c) {
		       return async_read(w.ctx, c.fd(), std::span<char>{c.in}, ReadUntil::HttpHead) | ex::let_value([&w, &c](std::size_t received) {
			              auto const size =
			                  received == 0 ? std::size_t{0} : w.handler(w.id, c.in.data(), received, c.out.data(), c.out.size());
			              return async_write(w.ctx, c.fd(), std::span<char const>{c.out.data(), std::min(size, c.out.size())});
		              }) |
		              ex::then([](std::size_t) {});
	       });
}

struct ChainReceiver {
	using receiver_concept = stdexec::receiver_t;

	WorkerCore* w;

	auto set_value() && noexcept -> void {
		w->restart = true;
	}

	// A failed connection (reset, EPIPE, ...) never stops the worker: note it
	// and serve the next client. A permanently failing listener would spin
	// here; acceptable for this slice's scope.
	auto set_error(NetErr) && noexcept -> void {
		w->restart = true;
	}

	// Unreachable under -fno-exceptions (STDEXEC_TRY degrades to a plain
	// block); reaching it would be an unrecoverable process failure.
	auto set_error(std::exception_ptr) && noexcept -> void {
		std::terminate();
	}

	auto set_stopped() && noexcept -> void {
		w->finished = true;
	}

	[[nodiscard]] auto get_env() const noexcept -> stdexec::env<> {
		return {};
	}
};

using ChainOp = stdexec::connect_result_t<decltype(make_chain(std::declval<WorkerCore&>())), ChainReceiver>;

// Operation states are immovable; this factory materializes one in place
// inside std::optional::emplace via the conversion operator (guaranteed
// copy elision).
struct ChainFactory {
	WorkerCore& w;

	operator ChainOp() && {
		return stdexec::connect(make_chain(w), ChainReceiver{&w});
	}
};

struct Worker {
	WorkerCore core;
	std::optional<ChainOp> op;
};

// The worker's run loop: (re)start the chain, then pump the reactor. Every
// async operation either completes synchronously inside start() or parks
// exactly one waiter, so at any loop iteration the chain is either finished,
// waiting for a restart, or armed — and on stop the armed case is completed
// through the stopped channel before the operation state is destroyed.
auto run_worker(Worker& worker) noexcept -> void {
	auto& w = worker.core;
	w.restart = true;
	while (!w.finished) {
		if (w.ctx.stopping) {
			if (w.ctx.has_armed()) {
				w.ctx.cancel_armed(); // completes stopped -> finished
			} else {
				w.finished = true;
			}
			continue;
		}
		if (w.restart) {
			w.ctx.poll(); // observe a pending stop even if chains keep completing synchronously
			if (w.ctx.stopping) {
				continue;
			}
			w.restart = false;
			worker.op.reset();
			worker.op.emplace(ChainFactory{w});
			stdexec::start(*worker.op);
			continue;
		}
		w.ctx.run_one();
	}
	worker.op.reset();
}

// --- listener factory --------------------------------------------------------

// The one nonblocking loopback listener, shared read-only by every worker
// (accept(2) and kevent registration are thread-safe on a shared fd).
// Loopback keeps the example and tests off the host firewall.
class Listener {
public:
	Listener() = default;

	explicit Listener(int fd) noexcept : fd_{fd} {}

	Listener(Listener&& other) noexcept : fd_{std::exchange(other.fd_, -1)} {}

	auto operator=(Listener&& other) noexcept -> Listener& {
		if (this != &other) {
			close_fd();
			fd_ = std::exchange(other.fd_, -1);
		}
		return *this;
	}

	Listener(Listener const&) = delete;
	auto operator=(Listener const&) -> Listener& = delete;

	~Listener() {
		close_fd();
	}

	[[nodiscard]] auto fd() const noexcept -> int {
		return fd_;
	}

private:
	auto close_fd() noexcept -> void {
		if (fd_ >= 0) {
			::close(std::exchange(fd_, -1));
		}
	}

	int fd_ = -1;
};

auto make_listener(std::uint16_t port, std::int32_t& err_stage, std::int32_t& err_code) noexcept -> Listener {
	auto fail = [&](std::int32_t stage, int fd) -> Listener {
		err_stage = stage;
		err_code = errno;
		if (fd >= 0) {
			::close(fd);
		}
		return Listener{};
	};

	auto const fd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		return fail(stage_socket, -1);
	}
	auto const one = 1;
	if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one) < 0) {
		return fail(stage_option, fd);
	}
	auto addr = ::sockaddr_in{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	// SAFETY: sockaddr_in -> sockaddr is the bind(2) ABI's own aliasing rule.
	if (::bind(fd, reinterpret_cast<::sockaddr const*>(&addr), sizeof addr) < 0) {
		return fail(stage_bind, fd);
	}
	if (auto const rc = set_nonblocking(fd); rc != 0) {
		errno = rc;
		return fail(stage_nonblock, fd);
	}
	if (::listen(fd, listen_backlog) < 0) {
		return fail(stage_listen, fd);
	}
	return Listener{fd};
}

auto bound_port(int fd, std::int32_t& err_stage, std::int32_t& err_code) noexcept -> std::uint16_t {
	auto addr = ::sockaddr_in{};
	auto len = ::socklen_t{sizeof addr};
	// SAFETY: same sockaddr ABI aliasing as bind above.
	if (::getsockname(fd, reinterpret_cast<::sockaddr*>(&addr), &len) < 0) {
		err_stage = stage_resolve;
		err_code = errno;
		return 0;
	}
	return ntohs(addr.sin_port);
}

// --- server: N share-nothing workers on a static_thread_pool ----------------

struct TaskReceiver {
	using receiver_concept = stdexec::receiver_t;

	std::latch* done;

	auto set_value() && noexcept -> void {
		done->count_down();
	}

	auto set_stopped() && noexcept -> void {
		done->count_down();
	}

	auto set_error(std::exception_ptr) && noexcept -> void {
		std::terminate();
	}

	[[nodiscard]] auto get_env() const noexcept -> stdexec::env<> {
		return {};
	}
};

using PoolScheduler = decltype(std::declval<ex::static_thread_pool&>().get_scheduler());

// One worker task: pin run_worker to a pool thread. The pool has exactly one
// thread per worker, so this is thread-per-core, not oversubscription.
auto make_task(PoolScheduler scheduler, Worker* worker) {
	return ex::starts_on(scheduler, ex::just() | ex::then([worker] {
		                                run_worker(*worker);
	                                }));
}

using TaskOp = stdexec::connect_result_t<decltype(make_task(std::declval<PoolScheduler>(), nullptr)), TaskReceiver>;

struct TaskFactory {
	PoolScheduler scheduler;
	Worker* worker;
	std::latch* done;

	operator TaskOp() && {
		return stdexec::connect(make_task(scheduler, worker), TaskReceiver{done});
	}
};

} // namespace

struct Server {
	Server(std::uint32_t worker_count, RawHandler request_handler)
	    : count{worker_count}, handler{request_handler}, done{static_cast<std::ptrdiff_t>(worker_count)}, pool{worker_count} {}

	std::uint32_t count;
	RawHandler handler;
	std::uint16_t port = 0;
	std::latch done;
	Listener listener{};
	std::array<Worker, max_workers> workers{};
	std::array<std::optional<TaskOp>, max_workers> tasks{};
	// Declared after the task operation states on purpose: destruction joins
	// the pool threads BEFORE the operation states they may still be
	// unwinding through are destroyed.
	ex::static_thread_pool pool;
	bool started = false;
	bool stopped = false;
};

auto server_start(std::uint16_t port, std::uint32_t workers, RawHandler handler, std::int32_t& err_stage, std::int32_t& err_code) noexcept
    -> Server* {
	err_stage = 0;
	err_code = 0;
	auto const count = std::clamp(workers, std::uint32_t{1}, max_workers);
	auto server = std::unique_ptr<Server>{new Server{count, handler}};

	// The one shared listener (see the header comment: Darwin SO_REUSEPORT
	// does not load-balance, so per-worker listeners would starve all but
	// the last-bound worker). Workers race nonblocking accepts on this fd.
	server->listener = make_listener(port, err_stage, err_code);
	if (server->listener.fd() < 0) {
		return nullptr;
	}
	server->port = bound_port(server->listener.fd(), err_stage, err_code);
	if (server->port == 0) {
		return nullptr;
	}

	for (auto i = std::uint32_t{0}; i < count; ++i) {
		auto& core = server->workers[i].core;
		if (auto const rc = core.ctx.init(); rc != 0) {
			err_stage = stage_queue;
			err_code = rc;
			return nullptr;
		}
		core.lfd = server->listener.fd();
		core.id = i;
		core.handler = handler;
	}

	auto const scheduler = server->pool.get_scheduler();
	for (auto i = std::uint32_t{0}; i < count; ++i) {
		server->tasks[i].emplace(TaskFactory{scheduler, &server->workers[i], &server->done});
		stdexec::start(*server->tasks[i]);
	}
	server->started = true;
	return server.release();
}

auto server_port(Server const& server) noexcept -> std::uint16_t {
	return server.port;
}

// Idempotent; single external owner (the :net partition's HttpServer).
// Returns after every worker has quiesced.
auto server_stop(Server& server) noexcept -> void {
	if (!server.started || server.stopped) {
		return;
	}
	server.stopped = true;
	for (auto i = std::uint32_t{0}; i < server.count; ++i) {
		server.workers[i].core.ctx.request_stop();
	}
	server.done.wait();
}

auto server_destroy(Server* server) noexcept -> void {
	if (server == nullptr) {
		return;
	}
	server_stop(*server);
	delete server; // pool destructor joins the (now idle) worker threads
}

auto hardware_worker_count() noexcept -> std::uint32_t {
	auto const count = std::thread::hardware_concurrency();
	return count == 0 ? 1 : count;
}

// Blocks until SIGINT or SIGTERM. The dispositions are set to SIG_IGN so
// default delivery cannot kill the process first; EVFILT_SIGNAL still
// records ignored signals (kqueue contract), so the kevent wait observes
// them. This is the entire signal-handling machinery — no handler runs any
// user code.
auto interrupt_wait() noexcept -> void {
	static_cast<void>(std::signal(SIGINT, SIG_IGN));
	static_cast<void>(std::signal(SIGTERM, SIG_IGN));
	auto const kq = ::kqueue();
	if (kq < 0) {
		return;
	}
	auto changes =
	    std::array{make_event(SIGINT, EVFILT_SIGNAL, EV_ADD, 0, nullptr), make_event(SIGTERM, EVFILT_SIGNAL, EV_ADD, 0, nullptr)};
	if (::kevent(kq, changes.data(), static_cast<int>(changes.size()), nullptr, 0, nullptr) == 0) {
		struct ::kevent event{};
		while (::kevent(kq, nullptr, 0, &event, 1, nullptr) < 0 && errno == EINTR) {
		}
	}
	::close(kq);
}

// stdout is fully buffered when piped; the example prints its resolved port
// before blocking on interrupt_wait, so the write must reach the pipe now.
auto output_flush() noexcept -> void {
	static_cast<void>(std::fflush(nullptr));
}

} // namespace starter::net_backend
