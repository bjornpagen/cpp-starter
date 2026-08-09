/* PIN(gcc-gmf-stdexec-ice): plain TU — with foreign/exec.backend.cc, the repository's entire stdexec spelling surface; see PINS.md */
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

namespace ex {

using stdexec::just;
using stdexec::let_value;
using stdexec::starts_on;
using stdexec::then;

using exec::static_thread_pool;

}

namespace starter::net_backend {

using RawHandler = std::size_t (*)(std::uint32_t worker, char const* data, std::size_t size, char* out, std::size_t capacity);

namespace {

inline constexpr std::uint32_t max_workers = 4;
inline constexpr std::size_t buffer_capacity = 8192;
inline constexpr int listen_backlog = 256;
inline constexpr std::uintptr_t stop_ident = 1;

/**
 * Mirrored by starter::NetStage in unsafe/net.cc (narrow scalar ABI;
 * keep in sync).
 */
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

struct NetErr {
	std::int32_t stage;
	std::int32_t code;
};

/**
 * EV_SET spelled as a function: the macro's implicit int conversions
 * trip -Wconversion at the expansion site.
 */
[[nodiscard]] auto make_event(std::uintptr_t ident, std::int16_t filter, std::uint16_t flags, std::uint32_t fflags, void* udata) noexcept
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

[[nodiscard]] auto
set_nonblocking(int fd) noexcept -> std::int32_t {
	auto const flags = ::fcntl(fd, F_GETFL, 0);
	if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
		return errno;
	}
	return 0;
}

/* SAFETY: self is the operation state, address-stable from connect until completion; the reactor invokes fn at most once, on the worker's own thread */
struct Waiter {
	void (*fn)(void* self, bool canceled);
	void* self;
};

/**
 * One worker's kqueue reactor, single-threaded by construction: only
 * request_stop crosses threads. A worker's straight-line connection
 * chain suspends on at most one fd at a time, so a single armed-waiter
 * slot is the whole scheduler state.
 */
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

	/**
	 * Parks `waiter` until `fd` is ready for `filter`. Oneshot: the
	 * kernel deletes the event on delivery, so normal operation leaves
	 * no stale registrations behind.
	 */
	[[nodiscard]] auto arm(int fd, std::int16_t filter, Waiter* waiter) noexcept -> std::int32_t {
		auto ev = make_event(static_cast<std::uintptr_t>(fd), filter, EV_ADD | EV_ONESHOT, 0, waiter);
		if (::kevent(kq_, &ev, 1, nullptr, 0, nullptr) < 0) {
			return errno;
		}
		armed_ = waiter;
		return 0;
	}

	/** The stop event only flips `stopping`; run_worker owns the consequences. */
	auto run_one() noexcept -> void {
		dispatch(nullptr);
	}

	/**
	 * Non-blocking dispatch: a worker whose chains keep completing
	 * synchronously still observes a pending stop.
	 */
	auto poll() noexcept -> void {
		auto const zero = ::timespec{.tv_sec = 0, .tv_nsec = 0};
		dispatch(&zero);
	}

	/**
	 * Completes the suspended operation through its stopped channel:
	 * destroying a started-but-uncompleted operation state is undefined
	 * in sender-land.
	 */
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

	/* SAFETY: the one cross-thread entry — kevent(2) on a shared kqueue fd is thread-safe; NOTE_TRIGGER wakes the owning worker, which stops on its own thread */
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
				stopping = true;
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
				continue;
			}
			armed_ = nullptr;
			waiter->fn(waiter->self, false);
		}
	}

	int kq_ = -1;
	Waiter* armed_ = nullptr;
};

/**
 * An accepted connection: lives in let_value's operation state and is
 * closed by RAII when that state is destroyed.
 */
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
	[[nodiscard]] auto connect(Rcvr rcvr) const noexcept -> AcceptOp<Rcvr> {
		return {std::move(rcvr), ctx, lfd};
	}
};

[[nodiscard]] auto async_accept(Ctx& ctx, int listener_fd) noexcept -> AcceptSender {
	return {&ctx, listener_fd};
}

/**
 * Some: complete after the first successful read. HttpHead: read until
 * the request head terminator (CRLF CRLF), a full buffer, or peer
 * half-close. Either way the completion value is the total byte count;
 * 0 is EOF before any data.
 */
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
				stdexec::set_value(std::move(rcvr), received);
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
	[[nodiscard]] auto connect(Rcvr rcvr) const noexcept -> ReadOp<Rcvr> {
		return {std::move(rcvr), ctx, fd, buffer, until};
	}
};

[[nodiscard]] auto async_read(Ctx& ctx, int fd, std::span<char> buffer, ReadUntil until) noexcept -> ReadSender {
	return {&ctx, fd, buffer, until};
}

/** Writes the whole span; partial writes and EAGAIN fold into the operation. */
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
	[[nodiscard]] auto connect(Rcvr rcvr) const noexcept -> WriteOp<Rcvr> {
		return {std::move(rcvr), ctx, fd, data};
	}
};

[[nodiscard]] auto async_write(Ctx& ctx, int fd, std::span<char const> data) noexcept -> WriteSender {
	return {&ctx, fd, data};
}

struct WorkerCore {
	Ctx ctx;
	int lfd = -1;
	std::uint32_t id = 0;
	RawHandler handler = nullptr;
	bool restart = false;
	bool finished = false;
};

/**
 * One full connection: the handler runs on this worker's thread, and
 * `c` lives in let_value's operation state, which outlives every inner
 * sender borrowing it.
 */
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

	auto set_error(NetErr) && noexcept -> void {
		w->restart = true;
	}

	/** Unreachable under -fno-exceptions; reaching it is process failure. */
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

/**
 * Operation states are immovable; the conversion operator materializes
 * one in place inside std::optional::emplace (guaranteed copy elision).
 */
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

/**
 * At every iteration the chain is finished, waiting for a restart, or
 * armed; on stop the armed case completes through the stopped channel
 * before its operation state is destroyed.
 */
auto run_worker(Worker& worker) noexcept -> void {
	auto& w = worker.core;
	w.restart = true;
	while (!w.finished) {
		if (w.ctx.stopping) {
			if (w.ctx.has_armed()) {
				w.ctx.cancel_armed();
			} else {
				w.finished = true;
			}
			continue;
		}
		if (w.restart) {
			w.ctx.poll();
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

/**
 * The one loopback listener, shared read-only by every worker —
 * accept(2) and kevent registration are thread-safe on a shared fd.
 * Loopback keeps the example and tests off the host firewall.
 */
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

/* PIN(darwin-so-reuseport-no-lb): one shared raced listener, not per-worker SO_REUSEPORT — see PINS.md */
[[nodiscard]] auto make_listener(std::uint16_t port, std::int32_t& err_stage, std::int32_t& err_code) noexcept -> Listener {
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
	/* SAFETY: sockaddr_in -> sockaddr is the bind(2) ABI's own aliasing rule */
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

[[nodiscard]] auto bound_port(int fd, std::int32_t& err_stage, std::int32_t& err_code) noexcept -> std::uint16_t {
	auto addr = ::sockaddr_in{};
	auto len = ::socklen_t{sizeof addr};
	/* SAFETY: same sockaddr ABI aliasing as bind above */
	if (::getsockname(fd, reinterpret_cast<::sockaddr*>(&addr), &len) < 0) {
		err_stage = stage_resolve;
		err_code = errno;
		return 0;
	}
	return ntohs(addr.sin_port);
}

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

}

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
	/**
	 * Declared after the task operation states: destruction joins the
	 * pool threads before the operation states they may still be
	 * unwinding through are destroyed.
	 */
	ex::static_thread_pool pool;
	bool started = false;
	bool stopped = false;
};

[[nodiscard]] auto server_start(std::uint16_t port, std::uint32_t workers, RawHandler handler, std::int32_t& err_stage,
                                std::int32_t& err_code) noexcept -> Server* {
	err_stage = 0;
	err_code = 0;
	auto const count = std::clamp(workers, std::uint32_t{1}, max_workers);
	auto server = std::unique_ptr<Server>{new Server{count, handler}};

	/* PIN(darwin-so-reuseport-no-lb): workers race accepts on this one shared listener — see PINS.md */
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

[[nodiscard]] auto server_port(Server const& server) noexcept -> std::uint16_t {
	return server.port;
}

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
	delete server;
}

[[nodiscard]] auto hardware_worker_count() noexcept -> std::uint32_t {
	auto const count = std::thread::hardware_concurrency();
	return count == 0 ? 1 : count;
}

/**
 * Dispositions are set to SIG_IGN so default delivery cannot kill the
 * process first; EVFILT_SIGNAL still records ignored signals (kqueue
 * contract), so the kevent wait observes them. No handler runs user
 * code.
 */
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

auto output_flush() noexcept -> void {
	static_cast<void>(std::fflush(nullptr));
}

}
