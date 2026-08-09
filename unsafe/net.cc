/**
 * Thread-per-core share-nothing networking: no mutable state is shared
 * between workers and no lock is visible at or above this surface
 * (concurrency model: unsafe/README.md).
 */
export module starter:net;

import std;

/* PIN(gcc-gmf-stdexec-ice): narrow ABI to the plain backend TU — senders cannot cross the module boundary; see PINS.md */
extern "C++" {
namespace starter::net_backend {

struct Server;

using RawHandler = std::size_t (*)(std::uint32_t worker, char const* data, std::size_t size, char* out, std::size_t capacity);

[[nodiscard]] auto server_start(std::uint16_t port, std::uint32_t workers, RawHandler handler, std::int32_t& err_stage,
                                std::int32_t& err_code) noexcept -> Server*;

[[nodiscard]] auto server_port(Server const& server) noexcept -> std::uint16_t;

auto server_stop(Server& server) noexcept -> void;

auto server_destroy(Server* server) noexcept -> void;

[[nodiscard]] auto hardware_worker_count() noexcept -> std::uint32_t;

auto interrupt_wait() noexcept -> void;

auto output_flush() noexcept -> void;

}
}

namespace starter {

/**
 * Which stage of bringing up or running the server failed. Values mirror
 * the stage_* constants in net.backend.cc (narrow scalar ABI; keep in
 * sync).
 */
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

/**
 * The typed network error: the failing stage plus its errno payload.
 * Flows as a value through sender error channels and std::expected.
 */
export struct [[nodiscard]] NetError {
	NetStage stage;
	std::int32_t code;

	/* PIN(gcc-modules-defaulted-eq): spelled out, not = default — see PINS.md */
	[[nodiscard]] constexpr auto operator==(NetError const& other) const -> bool {
		return stage == other.stage && code == other.code;
	}
};

export struct HttpServerConfig {
	/** 0 requests a kernel-chosen ephemeral port. */
	std::uint16_t port;
	/** Clamped to [1, 4] by the backend. */
	std::uint32_t workers;
};

/**
 * Statelessness is structural — emptiness and default-initializability
 * let the handler cross the narrow ABI as a plain trampoline with zero
 * captured state.
 */
export template<class F>
concept RequestHandler = std::is_empty_v<F> && std::default_initializable<F> &&
                         requires(F const handler, std::uint32_t worker, std::string_view request, std::span<char> out) {
	                         { handler(worker, request, out) } -> std::same_as<std::size_t>;
                         };

/**
 * A unique capability over the backend's workers: stop() is idempotent
 * and returns once every worker has quiesced; destruction stops and
 * joins.
 */
export class [[nodiscard]] HttpServer {
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

	/** The port actually bound (resolves a config.port == 0 request). */
	[[nodiscard]] auto port() const -> std::uint16_t {
		contract_assert(impl_ != nullptr);
		return net_backend::server_port(*impl_);
	}

	auto stop() -> void {
		if (impl_ != nullptr) {
			net_backend::server_stop(*impl_);
		}
	}

	/** Usable only by serve_http: importers cannot name net_backend::Server. */
	explicit HttpServer(net_backend::Server& impl) : impl_{&impl} {}

private:
	auto reset() -> void {
		if (impl_ != nullptr) {
			net_backend::server_destroy(std::exchange(impl_, nullptr));
		}
	}

	net_backend::Server* impl_;
};

/**
 * Starts config.workers share-nothing workers (clamped to [1, 4]), each
 * owning its own io-context, racing accepts on one shared loopback
 * listener. On failure no server exists — nothing to clean up; the
 * NetError's stage and errno say what refused. Loopback-only by design:
 * not a public-interface listener.
 */
export template<RequestHandler F>
[[nodiscard]] auto serve_http(HttpServerConfig config, F) -> std::expected<HttpServer, NetError> {
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

export [[nodiscard]] auto hardware_workers() -> std::uint32_t {
	return net_backend::hardware_worker_count();
}

/** Blocks until SIGINT or SIGTERM; no signal handler runs user code. */
export auto wait_for_interrupt() -> void {
	net_backend::interrupt_wait();
}

/** Call before blocking: stdout is fully buffered when piped. */
export auto flush_output() -> void {
	net_backend::output_flush();
}

}
