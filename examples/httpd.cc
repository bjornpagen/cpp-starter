// httpd — thread-per-core share-nothing HTTP/1.1 example over the exported
// :net and :http surfaces. N workers (hardware concurrency, capped at 4),
// each owning its OWN kqueue io-context, racing accepts on one shared
// loopback listener; per connection: accept -> read -> parse -> respond 200
// text/plain with the worker id and request path -> close. Zero cross-worker
// state: the handler is structurally stateless (RequestHandler concept) and
// each response is a pure function of (worker id, request bytes).
//
// The server binds an ephemeral port (config.port = 0) and prints the
// resolved port on stdout; SIGINT/SIGTERM shuts it down cleanly.
import std;
import starter;

namespace {

constexpr auto text_plain = std::array{starter::HeaderView{.name = "Content-Type", .value = "text/plain"}};

struct WorkerHandler {
	[[nodiscard]] auto operator()(std::uint32_t worker, std::string_view request, std::span<char> out) const -> std::size_t {
		auto const parsed = starter::parse_request(request);
		if (!parsed) {
			return starter::write_response(starter::ResponseHead{.status = 400, .reason = "Bad Request"}, text_plain, "bad request\n", out)
			    .value_or(0);
		}
		auto body_buffer = std::array<char, 256>{};
		auto const body_span = std::span{body_buffer};
		auto const formatted = std::format_to_n(body_span.begin(), std::ssize(body_span), "worker {} path {}\n", worker, parsed->target);
		auto const body = std::string_view{body_span.begin(), formatted.out};
		return starter::write_response(starter::ResponseHead{.status = 200, .reason = "OK"}, text_plain, body, out).value_or(0);
	}
};

} // namespace

auto main() -> int {
	auto const workers = std::min(starter::hardware_workers(), std::uint32_t{4});
	auto server = starter::serve_http(starter::HttpServerConfig{.port = 0, .workers = workers}, WorkerHandler{});
	if (!server) {
		std::println("httpd: start failed: stage {} errno {}", std::to_underlying(server.error().stage), server.error().code);
		return 1;
	}

	std::println("listening {}", server->port());
	starter::flush_output();

	starter::wait_for_interrupt();
	server->stop();
	std::println("stopped");
	return 0;
}
