// Networking surface over the kqueue io-context in net.backend.cc. The
// exported interface is dialect-clean; every socket/kevent syscall and every
// sender composition lives in the plain backend TU and is reached through
// the extern "C++" narrow ABI below (same mechanism and rationale as the
// :simd kernels and the :exec chains — the pinned GCC 16.1 ICEs when a
// stdexec header appears in any module unit, so senders cannot cross the
// module boundary; what crosses is the executable spec of the boundary).
//
// What the backend implements per worker: an owned kqueue reactor
// (IoContext with run/stop) and readiness-completing senders async_accept /
// async_read / async_write composed into an accept -> read -> handle ->
// write -> close chain. One shared nonblocking loopback listener is raced
// by all workers (Darwin SO_REUSEPORT does not load-balance; rationale
// pinned in net.backend.cc). Thread-per-core and share-nothing otherwise:
// no mutable state is shared between workers and no lock is visible here
// or above.
export module starter:net;

import std;

// Narrow ABI to the backend TU: declarations attached to the global module,
// matching the plain definitions in net.backend.cc.
extern "C++" {
namespace starter::net_backend {

struct Server;

using RawHandler = std::size_t (*)(std::uint32_t worker, char const* data, std::size_t size, char* out, std::size_t capacity);

auto server_start(std::uint16_t port, std::uint32_t workers, RawHandler handler, std::int32_t& err_stage, std::int32_t& err_code) noexcept
    -> Server*;

auto server_port(Server const& server) noexcept -> std::uint16_t;

auto server_stop(Server& server) noexcept -> void;

auto server_destroy(Server* server) noexcept -> void;

auto hardware_worker_count() noexcept -> std::uint32_t;

auto interrupt_wait() noexcept -> void;

auto output_flush() noexcept -> void;

} // namespace starter::net_backend
}

namespace starter {

// Which stage of bringing up or running the server failed. Values mirror
// the stage_* constants in net.backend.cc (narrow scalar ABI; keep in sync).
export enum class NetStage : std::int32_t {
	SocketCreate = 1,
	SocketOption = 2,
	SocketBind = 3,
	SocketListen = 4,
	SocketNonblock = 5,
	QueueCreate = 6,
	PortResolve = 7,
	Accept = 8,
	Read = 9,
	Write = 10,
};

// The typed network error: the failing stage plus its errno payload. Sender
// error channels inside the boundary carry this as a value (AGENTS.md §11);
// synchronous startup failure carries it through std::expected.
export struct NetError {
	NetStage stage;
	std::int32_t code;

	// Spelled out, not `= default`: GCC 16.1 pinned quirk — streaming a
	// defaulted comparison of an exported partition type through the BMI
	// ICEs the importer. Re-try `= default` on the next toolchain bump.
	[[nodiscard]] constexpr auto operator==(NetError const& other) const -> bool {
		return stage == other.stage && code == other.code;
	}
};

export struct HttpServerConfig {
	std::uint16_t port;    // 0 requests a kernel-chosen ephemeral port
	std::uint32_t workers; // clamped to [1, 4] by the backend
};

// A stateless per-request handler: worker id + request bytes in, response
// bytes into `out`, returning the response size (0 = close without
// replying). Statelessness is structural (empty, default-initializable), so
// the handler can cross the narrow ABI as a plain trampoline with zero
// captured state — which is also exactly the share-nothing worker contract.
export template<class F>
concept RequestHandler = std::is_empty_v<F> && std::default_initializable<F> &&
                         requires(F const handler, std::uint32_t worker, std::string_view request, std::span<char> out) {
	                         { handler(worker, request, out) } -> std::same_as<std::size_t>;
                         };

// A running share-nothing HTTP server: a unique capability over the
// backend's workers. stop() is idempotent and returns once every worker has
// quiesced; destruction stops and joins.
export class HttpServer {
public:
	HttpServer(HttpServer&& other) noexcept : impl_{std::exchange(other.impl_, nullptr)} {}

	auto operator=(HttpServer&& other) noexcept -> HttpServer& {
		if (this != &other) {
			reset();
			impl_ = std::exchange(other.impl_, nullptr);
		}
		return *this;
	}

	HttpServer(HttpServer const&) = delete;
	auto operator=(HttpServer const&) -> HttpServer& = delete;

	~HttpServer() {
		reset();
	}

	// The port actually bound (resolves a config.port == 0 request).
	[[nodiscard]] auto port() const -> std::uint16_t {
		contract_assert(impl_ != nullptr);
		return net_backend::server_port(*impl_);
	}

	auto stop() -> void {
		if (impl_ != nullptr) {
			net_backend::server_stop(*impl_);
		}
	}

	// Used by serve_http; not callable from importers (they cannot name
	// net_backend::Server).
	explicit HttpServer(net_backend::Server& impl) : impl_{&impl} {}

private:
	auto reset() -> void {
		if (impl_ != nullptr) {
			net_backend::server_destroy(std::exchange(impl_, nullptr));
		}
	}

	net_backend::Server* impl_;
};

// Starts config.workers share-nothing workers, each with its own io-context,
// racing nonblocking accepts on one shared loopback listener.
export template<RequestHandler F>
auto serve_http(HttpServerConfig config, F) -> std::expected<HttpServer, NetError> {
	auto stage = std::int32_t{0};
	auto code = std::int32_t{0};
	auto* impl = net_backend::server_start(
	    config.port, config.workers,
	    [](std::uint32_t worker, char const* data, std::size_t size, char* out, std::size_t capacity) -> std::size_t {
		    return F{}(worker, std::string_view{data, size}, std::span<char>{out, capacity});
	    },
	    stage, code);
	if (impl == nullptr) {
		return std::unexpected(NetError{.stage = static_cast<NetStage>(stage), .code = code});
	}
	return HttpServer{*impl};
}

// One worker per hardware thread is the share-nothing default; callers cap
// it (the example uses std::min(hardware_workers(), 4)).
export auto hardware_workers() -> std::uint32_t {
	return net_backend::hardware_worker_count();
}

// Blocks until SIGINT/SIGTERM. The signal machinery (dispositions plus a
// kqueue EVFILT_SIGNAL wait) lives entirely in the backend; no handler runs
// user code.
export auto wait_for_interrupt() -> void {
	net_backend::interrupt_wait();
}

// Flushes buffered standard output — needed before blocking, because stdout
// is fully buffered when piped (the smoke test reads the printed port).
export auto flush_output() -> void {
	net_backend::output_flush();
}

} // namespace starter
