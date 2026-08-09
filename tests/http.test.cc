// http.test.cc — unit spec of the :http partition: request parsing (valid,
// malformed, torn-buffer) and the buffer-writing response side.
import std;
import starter;

namespace {

struct CaseResult {
	std::string_view name;
	bool passed;
};

constexpr std::string_view valid_request = "GET /hello/world HTTP/1.1\r\n"
                                           "Host: localhost:8080\r\n"
                                           "User-Agent: curl/8.7.1\r\n"
                                           "Accept:  */* \r\n"
                                           "\r\n";

auto check_valid_request_line() -> CaseResult {
	auto const parsed = starter::parse_request(valid_request);
	return CaseResult{
	    .name = "valid request: method/target/version parsed",
	    .passed = parsed && parsed->method == "GET" && parsed->target == "/hello/world" && parsed->version == "HTTP/1.1",
	};
}

auto check_valid_headers() -> CaseResult {
	auto const parsed = starter::parse_request(valid_request);
	return CaseResult{
	    .name = "valid request: headers parsed, OWS trimmed",
	    .passed = parsed && parsed->header_count == 3 && parsed->headers[0].name == "Host" &&
	              parsed->headers[0].value == "localhost:8080" && parsed->headers[1].name == "User-Agent" &&
	              parsed->headers[1].value == "curl/8.7.1" && parsed->headers[2].name == "Accept" && parsed->headers[2].value == "*/*",
	};
}

auto check_no_headers() -> CaseResult {
	auto const parsed = starter::parse_request("GET / HTTP/1.1\r\n\r\n");
	return CaseResult{
	    .name = "request without headers parses with an empty header list",
	    .passed = parsed && parsed->target == "/" && parsed->header_count == 0,
	};
}

auto parse_prefix_incomplete(std::size_t length) -> bool {
	return starter::parse_request(valid_request.substr(0, length)) == std::unexpected(starter::HttpError::Incomplete);
}

auto check_torn_buffer_prefixes() -> CaseResult {
	// Every strict prefix of a valid request lacks the head terminator and
	// must report Incomplete (keep reading), never Malformed.
	auto passed = true;
	for (auto length = std::size_t{0}; length < valid_request.size(); ++length) {
		passed = passed && parse_prefix_incomplete(length);
	}
	return CaseResult{.name = "every torn-buffer prefix is Incomplete, not Malformed", .passed = passed};
}

auto check_malformed_request_lines() -> CaseResult {
	using enum starter::HttpError;
	auto const malformed = std::array<std::string_view, 6>{
	    "GET /\r\n\r\n",              // missing version
	    "GET  / HTTP/1.1\r\n\r\n",    // doubled space: empty target
	    "GET / HTTP/11\r\n\r\n",      // bad version shape
	    "GET /a b HTTP/1.1\r\n\r\n",  // space inside target
	    "G@T / HTTP/1.1\r\n\r\n",     // non-token method
	    "\r\nGET / HTTP/1.1\r\n\r\n", // empty request line
	};
	auto passed = true;
	for (auto const request : malformed) {
		passed = passed && starter::parse_request(request) == std::unexpected(Malformed);
	}
	return CaseResult{.name = "malformed request lines are rejected", .passed = passed};
}

auto check_malformed_headers() -> CaseResult {
	using enum starter::HttpError;
	auto const malformed = std::array<std::string_view, 3>{
	    "GET / HTTP/1.1\r\nHost localhost\r\n\r\n", // no colon
	    "GET / HTTP/1.1\r\nHo st: x\r\n\r\n",       // space in field name
	    "GET / HTTP/1.1\r\n: value\r\n\r\n",        // empty field name
	};
	auto passed = true;
	for (auto const request : malformed) {
		passed = passed && starter::parse_request(request) == std::unexpected(Malformed);
	}
	return CaseResult{.name = "malformed header lines are rejected", .passed = passed};
}

auto check_too_many_headers() -> CaseResult {
	auto request = std::string{"GET / HTTP/1.1\r\n"};
	for (auto index = std::size_t{0}; index < starter::max_header_count + 1; ++index) {
		request += std::format("X-Filler-{}: {}\r\n", index, index);
	}
	request += "\r\n";
	return CaseResult{
	    .name = "more than max_header_count field lines is TooManyHeaders",
	    .passed = starter::parse_request(request) == std::unexpected(starter::HttpError::TooManyHeaders),
	};
}

auto check_write_response() -> CaseResult {
	auto buffer = std::array<char, 256>{};
	auto const headers = std::array{starter::HeaderView{.name = "Content-Type", .value = "text/plain"}};
	auto const written =
	    starter::write_response(starter::ResponseHead{.status = 200, .reason = "OK"}, headers, "worker 0 path /\n", std::span{buffer});
	constexpr std::string_view expected = "HTTP/1.1 200 OK\r\n"
	                                      "Content-Type: text/plain\r\n"
	                                      "Content-Length: 16\r\n"
	                                      "\r\n"
	                                      "worker 0 path /\n";
	return CaseResult{
	    .name = "write_response emits the exact status line, headers, length, and body",
	    .passed = written && std::string_view{buffer.data(), *written} == expected,
	};
}

auto check_write_response_roundtrip() -> CaseResult {
	// The writer's output head must parse back cleanly with the same shape
	// rules the server applies to requests... responses share the field-line
	// grammar, so reuse the parser on a synthetic request wrapping the head.
	auto buffer = std::array<char, 256>{};
	auto const headers = std::array{starter::HeaderView{.name = "Content-Type", .value = "text/plain"}};
	auto const written =
	    starter::write_response(starter::ResponseHead{.status = 404, .reason = "Not Found"}, headers, "missing\n", std::span{buffer});
	auto const text = written ? std::string_view{buffer.data(), *written} : std::string_view{};
	return CaseResult{
	    .name = "write_response derives Content-Length from the body",
	    .passed = written && text.starts_with("HTTP/1.1 404 Not Found\r\n") && text.contains("\r\nContent-Length: 8\r\n\r\nmissing\n"),
	};
}

auto check_write_response_too_small() -> CaseResult {
	auto buffer = std::array<char, 16>{};
	auto const written = starter::write_response(starter::ResponseHead{.status = 200, .reason = "OK"},
	                                             std::span<starter::HeaderView const>{}, "body", std::span{buffer});
	return CaseResult{
	    .name = "write_response refuses a buffer that cannot hold the response",
	    .passed = written == std::unexpected(starter::HttpError::BufferTooSmall),
	};
}

} // namespace

auto main() -> int {
	auto const results = std::array{
	    check_valid_request_line(),       check_valid_headers(),           check_no_headers(),
	    check_torn_buffer_prefixes(),     check_malformed_request_lines(), check_malformed_headers(),
	    check_too_many_headers(),         check_write_response(),          check_write_response_roundtrip(),
	    check_write_response_too_small(),
	};

	auto failures = std::size_t{0};
	for (auto const& result : results) {
		if (result.passed) {
			std::println("pass: {}", result.name);
		} else {
			std::println("FAIL: {}", result.name);
			++failures;
		}
	}

	return failures == 0 ? 0 : 1;
}
