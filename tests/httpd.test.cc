import std;
import starter;
import starter_testkit;

namespace {

using Clock = std::chrono::steady_clock;

inline constexpr auto listen_budget = std::chrono::seconds{2};
inline constexpr auto hammer_budget = std::chrono::seconds{120};
inline constexpr auto stop_budget = std::chrono::seconds{5};
inline constexpr std::uint64_t bound_requests = 50'000;
inline constexpr std::uint64_t oversubscribe_requests = 10'000;
inline constexpr std::uint64_t recovery_requests = 5'000;
inline constexpr std::size_t oversubscribe_connections = 256;

struct Paths {
	std::string httpd;
	std::string bombardier;
};

[[nodiscard]] auto load_paths() -> std::optional<Paths> {
	auto in = std::ifstream{"httpd.paths.txt"};
	if (!in) {
		return std::nullopt;
	}
	auto paths = Paths{};
	if (!std::getline(in, paths.httpd) || !std::getline(in, paths.bombardier) || paths.httpd.empty() || paths.bombardier.empty()) {
		return std::nullopt;
	}
	return paths;
}

[[nodiscard]] auto parse_port(std::string_view line) -> std::optional<std::uint16_t> {
	constexpr auto prefix = std::string_view{"listening "};
	if (!line.starts_with(prefix)) {
		return std::nullopt;
	}
	auto const digits = line.substr(prefix.size());
	auto port = std::uint16_t{0};
	auto const parsed = std::from_chars(digits.data(), digits.data() + digits.size(), port);
	if (parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size() || port == 0) {
		return std::nullopt;
	}
	return port;
}

[[nodiscard]] auto json_u64(std::string_view text, std::string_view key) -> std::optional<std::uint64_t> {
	auto const needle = std::format("\"{}\":", key);
	auto const at = text.find(needle);
	if (at == std::string_view::npos) {
		return std::nullopt;
	}
	auto cursor = at + needle.size();
	while (cursor < text.size() && (text[cursor] == ' ' || text[cursor] == '\t')) {
		++cursor;
	}
	auto value = std::uint64_t{0};
	auto const parsed = std::from_chars(text.data() + cursor, text.data() + text.size(), value);
	if (parsed.ec != std::errc{}) {
		return std::nullopt;
	}
	return value;
}

[[nodiscard]] auto json_object_empty(std::string_view text, std::string_view key) -> bool {
	auto const needle = std::format("\"{}\":", key);
	auto const at = text.find(needle);
	if (at == std::string_view::npos) {
		return true;
	}
	auto cursor = at + needle.size();
	while (cursor < text.size() && (text[cursor] == ' ' || text[cursor] == '\t')) {
		++cursor;
	}
	return cursor < text.size() && text[cursor] == '{' && cursor + 1 < text.size() && text[cursor + 1] == '}';
}

struct HammerReport {
	std::uint64_t ok;
	std::uint64_t fail;
	std::uint64_t other;
	bool clean_errors;
	int status;
	std::string output;
};

[[nodiscard]] auto run_bombardier(std::string const& bombardier, std::string const& url, std::size_t connections,
                                  std::uint64_t requests) -> std::expected<HammerReport, starter::ChildError> {
	auto child = starter::Child::spawn(bombardier, {
	    "-c",
	    std::to_string(connections),
	    "-n",
	    std::to_string(requests),
	    "-t",
	    "5s",
	    "--http1",
	    "--disableKeepAlives",
	    "-p",
	    "r",
	    "-o",
	    "json",
	    url,
	});
	if (!child) {
		return std::unexpected(child.error());
	}
	auto const output = child->read_all_until(Clock::now() + hammer_budget);
	if (!output) {
		return std::unexpected(output.error());
	}
	auto const status = child->wait_until(Clock::now() + stop_budget);
	if (!status) {
		return std::unexpected(status.error());
	}
	return HammerReport{
	    .ok = json_u64(*output, "req2xx").value_or(0),
	    .fail = json_u64(*output, "req5xx").value_or(0),
	    .other = json_u64(*output, "others").value_or(0),
	    .clean_errors = json_object_empty(*output, "errorDist"),
	    .status = *status,
	    .output = *output,
	};
}

}

auto main() -> int {
	auto failures = 0;
	auto expect = [&](bool condition, std::string_view label) {
		if (!condition) {
			std::println("FAIL: {}", label);
			++failures;
		}
	};

	auto const paths = load_paths();
	if (!paths) {
		std::println("FAIL: missing httpd.paths.txt next to the test");
		return 1;
	}

	auto server = starter::Child::spawn(paths->httpd, {});
	if (!server) {
		std::println("FAIL: spawn httpd: stage {} code {}", std::to_underlying(server.error().stage), server.error().code);
		return 1;
	}

	auto const banner = server->read_line_until(Clock::now() + listen_budget);
	if (!banner) {
		std::println("FAIL: httpd listening line: stage {} code {}", std::to_underlying(banner.error().stage), banner.error().code);
		return 1;
	}
	auto const port = parse_port(*banner);
	if (!port) {
		std::println("FAIL: httpd banner was {}", *banner);
		return 1;
	}
	auto const url = std::format("http://127.0.0.1:{}/hammer", *port);

	auto const bound = run_bombardier(paths->bombardier, url, starter::max_connection_count, bound_requests);
	if (!bound) {
		std::println("FAIL: bound bombardier: stage {} code {}", std::to_underlying(bound.error().stage), bound.error().code);
		return 1;
	}
	expect(bound->status == 0, "bound bombardier exited 0");
	expect(bound->ok == bound_requests, "bound bombardier 2xx equals request count");
	expect(bound->fail == 0, "bound bombardier 5xx is zero");
	expect(bound->other == 0, "bound bombardier others is zero");
	expect(bound->clean_errors, "bound bombardier errorDist is empty");
	expect(server->alive(), "server stayed up through the bound hammer");
	if (bound->ok != bound_requests || bound->fail != 0 || !bound->clean_errors) {
		std::println("{}", bound->output);
	}

	auto const storm = run_bombardier(paths->bombardier, url, oversubscribe_connections, oversubscribe_requests);
	if (!storm) {
		std::println("FAIL: oversubscribe bombardier: stage {} code {}", std::to_underlying(storm.error().stage), storm.error().code);
		return 1;
	}
	expect(server->alive(), "server stayed up through oversubscription");
	expect(storm->fail == 0, "oversubscribe bombardier 5xx is zero");
	if (storm->fail != 0) {
		std::println("{}", storm->output);
	}

	auto const recovery = run_bombardier(paths->bombardier, url, 64, recovery_requests);
	if (!recovery) {
		std::println("FAIL: recovery bombardier: stage {} code {}", std::to_underlying(recovery.error().stage), recovery.error().code);
		return 1;
	}
	expect(recovery->status == 0, "recovery bombardier exited 0");
	expect(recovery->ok == recovery_requests, "recovery bombardier 2xx equals request count");
	expect(recovery->fail == 0, "recovery bombardier 5xx is zero");
	expect(recovery->other == 0, "recovery bombardier others is zero");
	expect(recovery->clean_errors, "recovery bombardier errorDist is empty");
	expect(server->alive(), "server stayed up through recovery");
	if (recovery->ok != recovery_requests || recovery->fail != 0 || !recovery->clean_errors) {
		std::println("{}", recovery->output);
	}

	server->interrupt();
	auto const stopped = server->wait_until(Clock::now() + stop_budget);
	if (!stopped) {
		std::println("FAIL: httpd did not stop: stage {} code {}", std::to_underlying(stopped.error().stage), stopped.error().code);
		return 1;
	}
	expect(*stopped == 0, "httpd exited 0 after SIGINT");

	if (failures == 0) {
		std::println("pass: bombardier {}@{} then {}@{} then recovery; httpd drained", bound_requests,
		             starter::max_connection_count, oversubscribe_requests, oversubscribe_connections);
	}
	return failures == 0 ? 0 : 1;
}
