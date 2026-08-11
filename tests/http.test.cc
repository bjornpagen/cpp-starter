import std;
import starter;

namespace {

struct CaseResult {
	std::string_view name;
	bool passed;
};

[[nodiscard]] constexpr auto http_error(starter::HttpErrorKind kind) -> starter::HttpError {
	return starter::HttpError{kind};
}

constexpr std::string_view valid_request = "GET /hello/world HTTP/1.1\r\n"
                                           "Host: localhost:8080\r\n"
                                           "User-Agent: curl/8.7.1\r\n"
                                           "Accept:  */* \r\n"
                                           "\r\n";

[[nodiscard]] auto check_valid_request_line() -> CaseResult {
	auto const parsed = starter::parse_request(valid_request);
	return CaseResult{
	    .name = "valid request: method/target/version parsed",
	    .passed = parsed && parsed->method == "GET" && parsed->target == "/hello/world" && parsed->version == "HTTP/1.1",
	};
}

[[nodiscard]] auto check_valid_headers() -> CaseResult {
	auto const parsed = starter::parse_request(valid_request);
	return CaseResult{
	    .name = "valid request: headers parsed, OWS trimmed",
	    .passed = parsed && parsed->headers.size() == 3 && parsed->headers[0].name == "Host" &&
	              parsed->headers[0].value == "localhost:8080" && parsed->headers[1].name == "User-Agent" &&
	              parsed->headers[1].value == "curl/8.7.1" && parsed->headers[2].name == "Accept" && parsed->headers[2].value == "*/*",
	};
}

[[nodiscard]] auto check_no_headers() -> CaseResult {
	auto const parsed = starter::parse_request("GET / HTTP/1.1\r\n\r\n");
	return CaseResult{
	    .name = "request without headers parses with an empty header list",
	    .passed = parsed && parsed->target == "/" && parsed->headers.empty(),
	};
}

[[nodiscard]] auto check_request_owns_temporary_input() -> CaseResult {
	auto const parsed = starter::parse_request(std::string{valid_request});
	return CaseResult{
	    .name = "parsed requests own data from temporary input",
	    .passed = parsed && parsed->target == "/hello/world" && parsed->headers[0].value == "localhost:8080",
	};
}

[[nodiscard]] auto parse_prefix_incomplete(std::size_t length) -> bool {
	return starter::parse_request(valid_request.substr(0, length)) == std::unexpected(http_error(starter::HttpErrorKind::Incomplete));
}

[[nodiscard]] auto check_torn_buffer_prefixes() -> CaseResult {
	auto passed = true;
	for (auto length = std::size_t{0}; length < valid_request.size(); ++length) {
		passed = passed && parse_prefix_incomplete(length);
	}
	return CaseResult{.name = "every torn-buffer prefix is Incomplete, not Malformed", .passed = passed};
}

[[nodiscard]] auto check_request_size_bound() -> CaseResult {
	auto input = std::string(starter::max_request_bytes, 'x');
	auto const at_limit = starter::parse_request(input);
	input.push_back('x');
	auto const over_limit = starter::parse_request(input);
	return CaseResult{
	    .name = "a full unterminated head and any oversized input are MessageTooLarge",
	    .passed = at_limit == std::unexpected(http_error(starter::HttpErrorKind::MessageTooLarge)) &&
	              over_limit == std::unexpected(http_error(starter::HttpErrorKind::MessageTooLarge)),
	};
}

[[nodiscard]] auto check_malformed_request_lines() -> CaseResult {
	using enum starter::HttpErrorKind;
	auto const malformed = std::array<std::string_view, 6>{
	    "GET /\r\n\r\n",          "GET  / HTTP/1.1\r\n\r\n",    "GET / HTTP/11\r\n\r\n", "GET /a b HTTP/1.1\r\n\r\n",
	    "G@T / HTTP/1.1\r\n\r\n", "\r\nGET / HTTP/1.1\r\n\r\n",
	};
	auto passed = true;
	for (auto const request : malformed) {
		passed = passed && starter::parse_request(request) == std::unexpected(http_error(Malformed));
	}
	return CaseResult{.name = "malformed request lines are rejected", .passed = passed};
}

[[nodiscard]] auto check_malformed_headers() -> CaseResult {
	using enum starter::HttpErrorKind;
	auto const malformed = std::array<std::string_view, 3>{
	    "GET / HTTP/1.1\r\nHost localhost\r\n\r\n",
	    "GET / HTTP/1.1\r\nHo st: x\r\n\r\n",
	    "GET / HTTP/1.1\r\n: value\r\n\r\n",
	};
	auto passed = true;
	for (auto const request : malformed) {
		passed = passed && starter::parse_request(request) == std::unexpected(http_error(Malformed));
	}
	return CaseResult{.name = "malformed header lines are rejected", .passed = passed};
}

[[nodiscard]] auto check_too_many_headers() -> CaseResult {
	auto request = std::string{"GET / HTTP/1.1\r\n"};
	for (auto index = std::size_t{0}; index < starter::max_header_count + 1; ++index) {
		request += std::format("X-Filler-{}: {}\r\n", index, index);
	}
	request += "\r\n";
	return CaseResult{
	    .name = "more than max_header_count field lines is TooManyHeaders",
	    .passed = starter::parse_request(request) == std::unexpected(http_error(starter::HttpErrorKind::TooManyHeaders)),
	};
}

[[nodiscard]] auto request_with_header(std::string name, std::string value) -> starter::Request {
	return starter::Request{
	    .method = "POST",
	    .target = "/",
	    .version = "HTTP/1.1",
	    .headers = {starter::Header{.name = std::move(name), .value = std::move(value)}},
	};
}

[[nodiscard]] auto check_declared_body_absent() -> CaseResult {
	auto const bare = starter::Request{.method = "GET", .target = "/", .version = "HTTP/1.1", .headers = {}};
	return CaseResult{
	    .name = "has_declared_body: no headers and Content-Length 0 declare no body",
	    .passed = !starter::has_declared_body(bare) && !starter::has_declared_body(request_with_header("Content-Length", "0")),
	};
}

[[nodiscard]] auto check_declared_body_content_length() -> CaseResult {
	return CaseResult{
	    .name = "has_declared_body: nonzero Content-Length declares a body case-insensitively",
	    .passed = starter::has_declared_body(request_with_header("Content-Length", "5")) &&
	              starter::has_declared_body(request_with_header("content-length", "5")),
	};
}

[[nodiscard]] auto check_declared_body_transfer_encoding() -> CaseResult {
	return CaseResult{
	    .name = "has_declared_body: any Transfer-Encoding declares a body case-insensitively",
	    .passed = starter::has_declared_body(request_with_header("Transfer-Encoding", "chunked")) &&
	              starter::has_declared_body(request_with_header("TRANSFER-ENCODING", "chunked")),
	};
}

[[nodiscard]] auto check_write_response() -> CaseResult {
	auto const response = starter::Response{
	    .status = 200,
	    .reason = "OK",
	    .headers = {starter::Header{.name = "Content-Type", .value = "text/plain"}},
	    .body = "path /\n",
	};
	auto const written = starter::write_response(response);
	constexpr std::string_view expected = "HTTP/1.1 200 OK\r\n"
	                                      "Content-Type: text/plain\r\n"
	                                      "Connection: close\r\n"
	                                      "Content-Length: 7\r\n"
	                                      "\r\n"
	                                      "path /\n";
	return CaseResult{
	    .name = "write_response emits the exact status line, headers, length, and body",
	    .passed = written && *written == expected,
	};
}

[[nodiscard]] auto check_write_response_roundtrip() -> CaseResult {
	auto const response = starter::Response{
	    .status = 404,
	    .reason = "Not Found",
	    .headers = {starter::Header{.name = "Content-Type", .value = "text/plain"}},
	    .body = "missing\n",
	};
	auto const written = starter::write_response(response);
	auto const text = written ? std::string_view{written->data(), written->size()} : std::string_view{};
	return CaseResult{
	    .name = "write_response derives Content-Length from the body",
	    .passed = written && text.starts_with("HTTP/1.1 404 Not Found\r\n") && text.contains("\r\nContent-Length: 8\r\n\r\nmissing\n"),
	};
}

[[nodiscard]] auto check_write_response_owns_output() -> CaseResult {
	auto const written = starter::write_response(starter::Response{.status = 200, .reason = "OK", .headers = {}, .body = "body"});
	return CaseResult{
	    .name = "write_response owns bytes derived from a temporary response",
	    .passed = written && written->ends_with("\r\n\r\nbody"),
	};
}

[[nodiscard]] auto check_reserved_response_headers() -> CaseResult {
	auto passed = true;
	for (auto const name : std::array<std::string_view, 3>{"content-length", "TRANSFER-ENCODING", "Connection"}) {
		auto const written = starter::write_response(starter::Response{
		    .status = 200,
		    .reason = "OK",
		    .headers = {starter::Header{.name = std::string{name}, .value = "hostile"}},
		    .body = "body",
		});
		passed = passed && written == std::unexpected(http_error(starter::HttpErrorKind::Malformed));
	}
	return CaseResult{.name = "write_response owns framing headers case-insensitively", .passed = passed};
}

[[nodiscard]] auto check_response_splitting_rejected() -> CaseResult {
	auto const bad_reason =
	    starter::write_response(starter::Response{.status = 200, .reason = "OK\r\nInjected: yes", .headers = {}, .body = "body"});
	auto const bad_value = starter::write_response(starter::Response{
	    .status = 200,
	    .reason = "OK",
	    .headers = {starter::Header{.name = "X-Safe", .value = "value\r\nInjected: yes"}},
	    .body = "body",
	});
	return CaseResult{
	    .name = "status reasons and field values cannot inject response lines",
	    .passed = bad_reason == std::unexpected(http_error(starter::HttpErrorKind::Malformed)) &&
	              bad_value == std::unexpected(http_error(starter::HttpErrorKind::Malformed)),
	};
}

[[nodiscard]] auto check_response_header_bound() -> CaseResult {
	auto response = starter::Response{.status = 200, .reason = "OK", .headers = {}, .body = "body"};
	for (auto index = std::size_t{0}; index < starter::max_header_count + 1; ++index) {
		response.headers.push_back(starter::Header{.name = std::format("X-{}", index), .value = "value"});
	}
	auto const written = starter::write_response(response);
	return CaseResult{
	    .name = "write_response rejects more than max_header_count caller fields",
	    .passed = written == std::unexpected(http_error(starter::HttpErrorKind::TooManyHeaders)),
	};
}

[[nodiscard]] auto check_response_size_bound() -> CaseResult {
	auto const written = starter::write_response(
	    starter::Response{.status = 200, .reason = "OK", .headers = {}, .body = std::string(starter::max_response_bytes, 'x')});
	return CaseResult{
	    .name = "write_response enforces max_response_bytes before allocating output",
	    .passed = written == std::unexpected(http_error(starter::HttpErrorKind::MessageTooLarge)),
	};
}

[[nodiscard]] auto check_http_error_classification() -> CaseResult {
	using enum starter::HttpErrorKind;
	return CaseResult{
	    .name = "HTTP retryability is exhaustive: only a torn buffer is transient",
	    .passed = http_error(Incomplete).is_transient() && !http_error(Malformed).is_transient() &&
	              !http_error(TooManyHeaders).is_transient() && !http_error(MessageTooLarge).is_transient() &&
	              !http_error(SizeOverflow).is_transient(),
	};
}

}

auto main() -> int {
	auto const results = std::array{
	    check_valid_request_line(),
	    check_valid_headers(),
	    check_no_headers(),
	    check_request_owns_temporary_input(),
	    check_torn_buffer_prefixes(),
	    check_request_size_bound(),
	    check_malformed_request_lines(),
	    check_malformed_headers(),
	    check_too_many_headers(),
	    check_declared_body_absent(),
	    check_declared_body_content_length(),
	    check_declared_body_transfer_encoding(),
	    check_write_response(),
	    check_write_response_roundtrip(),
	    check_write_response_owns_output(),
	    check_reserved_response_headers(),
	    check_response_splitting_rejected(),
	    check_response_header_bound(),
	    check_response_size_bound(),
	    check_http_error_classification(),
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
