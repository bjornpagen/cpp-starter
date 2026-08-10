import std;
import starter;

namespace {

enum class HandlerError : std::uint8_t {
	ResponseTooLarge,
};

struct Handler {
	static constexpr std::size_t max_body_bytes = 256;

	[[nodiscard]] auto operator()(starter::Request const& request) const -> std::expected<starter::Response, HandlerError> {
		if (request.target.size() > max_body_bytes - std::string_view{"path \n"}.size()) {
			return std::unexpected(HandlerError::ResponseTooLarge);
		}
		return starter::Response{
		    .status = 200,
		    .reason = "OK",
		    .headers = {starter::Header{.name = "Content-Type", .value = "text/plain"}},
		    .body = std::format("path {}\n", request.target),
		};
	}
};

}

auto main() -> int {
	auto server = starter::open_http(starter::HttpServerConfig{
	    .port = 0,
	    .max_connections = starter::max_connection_count,
	    .request_timeout = std::chrono::seconds{2},
	    .response_timeout = std::chrono::seconds{2},
	});
	if (!server) {
		std::println("httpd: open failed: stage {} errno {}", std::to_underlying(server.error().stage), server.error().code);
		return 1;
	}

	std::println("listening {}", server->port());
	starter::flush_output();

	auto const ran = server->run<Handler>();
	if (!ran) {
		std::println("httpd: run failed: stage {} errno {}", std::to_underlying(ran.error().stage), ran.error().code);
		return 1;
	}
	std::println("stopped");
	return 0;
}
