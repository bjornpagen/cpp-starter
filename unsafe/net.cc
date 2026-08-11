export module starter:net;

import std;
import :http;

extern "C++" {
namespace starter::net_backend {

struct Server;

struct ServerDeleter {
	auto operator()(Server* server) const noexcept -> void;
};

using ServerOwner = std::unique_ptr<Server, ServerDeleter>;
using WireError = std::array<std::int32_t, 2>;
using RawHandler = std::optional<std::size_t> (*)(std::string_view request, std::span<char> out) noexcept;

[[nodiscard]] auto err_transient(std::int32_t code) noexcept -> bool;

[[nodiscard]] auto buffer_capacity() noexcept -> std::size_t;

[[nodiscard]] auto connection_capacity() noexcept -> std::size_t;

[[nodiscard]] auto timeout_capacity() noexcept -> std::chrono::milliseconds;

[[nodiscard]] auto server_open(std::uint16_t port, std::size_t connection_count, std::chrono::milliseconds request_timeout,
                               std::chrono::milliseconds response_timeout) noexcept -> std::expected<ServerOwner, WireError>;

[[nodiscard]] auto server_run(Server& server, RawHandler handler) noexcept -> std::expected<void, WireError>;

[[nodiscard]] auto server_port(Server const& server) noexcept -> std::uint16_t;

auto output_flush() noexcept -> void;

}
}

namespace starter {

export enum class NetStage : std::int32_t {
	Configuration = 1,
	SocketCreate = 2,
	SocketOption = 3,
	SocketBind = 4,
	SocketListen = 5,
	SocketNonblock = 6,
	QueueCreate = 7,
	SignalSetup = 8,
	PortResolve = 9,
	Accept = 10,
};

export struct [[nodiscard]] NetError {
	NetStage stage;
	std::int32_t code;

	/** Could the same operation plausibly succeed if retried later? */
	[[nodiscard]] auto is_transient() const -> bool {
		return net_backend::err_transient(code);
	}

	[[nodiscard]] constexpr auto operator==(NetError const&) const -> bool = default;
};

export inline constexpr std::size_t max_connection_count = 128;
export inline constexpr auto max_server_timeout = std::chrono::hours{24};

export struct HttpServerConfig {
	/** 0 requests a kernel-chosen ephemeral port. */
	std::uint16_t port;
	std::size_t max_connections;
	std::chrono::milliseconds request_timeout;
	std::chrono::milliseconds response_timeout;
};

export template<class T>
concept ResponseResult = requires(T result) {
	typename T::value_type;
	typename T::error_type;
	requires std::same_as<typename T::value_type, Response>;
	{ result.has_value() } -> std::same_as<bool>;
	{ *result } -> std::same_as<Response&>;
};

export template<class F>
concept RequestHandler =
    std::is_empty_v<F> && std::default_initializable<F> && ResponseResult<std::invoke_result_t<F const&, Request const&>>;

namespace net_detail {

[[nodiscard]] auto lift_error(net_backend::WireError error) -> NetError {
	return NetError{.stage = static_cast<NetStage>(error[0]), .code = error[1]};
}

[[nodiscard]] auto write_to(Response const& response, std::span<char> out) -> std::optional<std::size_t> {
	auto const encoded = write_response(response);
	if (!encoded || encoded->size() > out.size()) {
		return std::nullopt;
	}
	std::ignore = std::ranges::copy(*encoded, out.begin());
	return encoded->size();
}

[[nodiscard]] auto error_response(std::uint16_t status, std::string reason, std::string_view body, std::span<char> out)
    -> std::optional<std::size_t> {
	return write_to(Response{.status = status, .reason = std::move(reason), .headers = {}, .body = std::string{body}}, out);
}

}

/**
 * A unique server capability. The first SIGINT or SIGTERM stops accepting
 * and drains in-flight connections within the configured deadlines; a
 * second signal returns immediately; destruction releases all remaining
 * connections.
 */
export class [[nodiscard]] HttpServer {
public:
	HttpServer(HttpServer&&) noexcept = default;
	auto operator=(HttpServer&&) noexcept -> HttpServer& = default;

	HttpServer(HttpServer const&) = delete;
	auto operator=(HttpServer const&) -> HttpServer& = delete;

	~HttpServer() = default;

	[[nodiscard]] auto port() const -> std::uint16_t {
		contract_assert(impl_ != nullptr);
		return net_backend::server_port(*impl_);
	}

	/** The stateless handler runs inline on the owner loop and must not block. */
	template<RequestHandler F>
	[[nodiscard]] auto run() -> std::expected<void, NetError> {
		contract_assert(impl_ != nullptr);
		auto const result =
		    net_backend::server_run(*impl_, [](std::string_view input, std::span<char> out) noexcept -> std::optional<std::size_t> {
			    auto const request = parse_request(input);
			    if (!request) {
				    if (request.error().kind == HttpErrorKind::MessageTooLarge || request.error().kind == HttpErrorKind::TooManyHeaders) {
					    return net_detail::error_response(431, "Request Header Fields Too Large", "request headers too large\n", out);
				    }
				    return net_detail::error_response(400, "Bad Request", "bad request\n", out);
			    }
			    if (has_declared_body(*request)) {
				    return net_detail::error_response(501, "Not Implemented", "request bodies are not supported\n", out);
			    }
			    auto const response = F{}(*request);
			    if (!response) {
				    return net_detail::error_response(500, "Internal Server Error", "internal server error\n", out);
			    }
			    auto const written = net_detail::write_to(*response, out);
			    if (!written) {
				    return net_detail::error_response(500, "Internal Server Error", "internal server error\n", out);
			    }
			    return *written;
		    });
		if (!result) {
			return std::unexpected(net_detail::lift_error(result.error()));
		}
		return {};
	}

	/** Usable only by open_http: importers cannot name the backend owner. */
	explicit HttpServer(net_backend::ServerOwner owner) : impl_{std::move(owner)} {}

private:
	net_backend::ServerOwner impl_;
};

/** Opens a bounded loopback server without starting its event loop. */
export [[nodiscard]] auto open_http(HttpServerConfig config) -> std::expected<HttpServer, NetError> {
	contract_assert(net_backend::buffer_capacity() == max_request_bytes);
	contract_assert(net_backend::buffer_capacity() == max_response_bytes);
	contract_assert(net_backend::connection_capacity() == max_connection_count);
	contract_assert(net_backend::timeout_capacity() == std::chrono::duration_cast<std::chrono::milliseconds>(max_server_timeout));
	auto opened = net_backend::server_open(config.port, config.max_connections, config.request_timeout, config.response_timeout);
	if (!opened) {
		return std::unexpected(net_detail::lift_error(opened.error()));
	}
	return HttpServer{std::move(*opened)};
}

/** Flushes C stdout before entering the blocking server loop. */
export auto flush_output() -> void {
	net_backend::output_flush();
}

}
