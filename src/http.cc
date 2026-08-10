export module starter:http;

import std;

namespace starter {

/**
 * Incomplete is the torn-buffer signal, not a rejection: the head
 * terminator has not arrived yet, so the caller keeps reading.
 */
export enum class HttpErrorKind : std::uint8_t {
	Incomplete,
	Malformed,
	TooManyHeaders,
	MessageTooLarge,
	SizeOverflow,
};

export struct [[nodiscard]] HttpError {
	HttpErrorKind kind;

	[[nodiscard]] constexpr auto is_transient() const -> bool {
		switch (kind) {
		case HttpErrorKind::Incomplete:
			return true;
		case HttpErrorKind::Malformed:
		case HttpErrorKind::TooManyHeaders:
		case HttpErrorKind::MessageTooLarge:
		case HttpErrorKind::SizeOverflow:
			return false;
		}
	}

	[[nodiscard]] constexpr auto operator==(HttpError const&) const -> bool = default;
};

export inline constexpr std::size_t max_header_count = 32;
export inline constexpr std::size_t max_request_bytes = 8192;
export inline constexpr std::size_t max_response_bytes = 8192;

export struct Header {
	std::string name;
	std::string value;
};

/* PIN(gcc-partition-bmi-inplace-vector): vector preserves ownership until GCC can import the bounded member */
/** The parsed request owns every byte exposed by its fields. */
export struct Request {
	std::string method;
	std::string target;
	std::string version;
	std::vector<Header> headers;
};

/* PIN(gcc-partition-bmi-inplace-vector): same exported-field serializer failure as Request */
/** A handler result owns every byte that the response writer consumes. */
export struct Response {
	std::uint16_t status;
	std::string reason;
	std::vector<Header> headers;
	std::string body;
};

namespace {

constexpr std::string_view crlf = "\r\n";
constexpr std::string_view head_terminator = "\r\n\r\n";

[[nodiscard]] constexpr auto is_tchar(char c) -> bool {
	if (c >= 'a' && c <= 'z') {
		return true;
	}
	if (c >= 'A' && c <= 'Z') {
		return true;
	}
	if (c >= '0' && c <= '9') {
		return true;
	}
	return std::string_view{"!#$%&'*+-.^_`|~"}.contains(c);
}

[[nodiscard]] constexpr auto is_token(std::string_view text) -> bool {
	return !text.empty() && std::ranges::all_of(text, is_tchar);
}

[[nodiscard]] constexpr auto is_vchar(char c) -> bool {
	return c > '\x20' && c != '\x7f';
}

[[nodiscard]] constexpr auto is_field_char(char c) -> bool {
	return is_vchar(c) || c == ' ' || c == '\t';
}

[[nodiscard]] constexpr auto ascii_lower(char c) -> char {
	return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c;
}

[[nodiscard]] constexpr auto ascii_equal_fold(std::string_view left, std::string_view right) -> bool {
	if (left.size() != right.size()) {
		return false;
	}
	for (auto index = std::size_t{0}; index < left.size(); ++index) {
		if (ascii_lower(left[index]) != ascii_lower(right[index])) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] constexpr auto is_reserved_response_header(std::string_view name) -> bool {
	return ascii_equal_fold(name, "Content-Length") || ascii_equal_fold(name, "Transfer-Encoding") || ascii_equal_fold(name, "Connection");
}

[[nodiscard]] constexpr auto trim_ows(std::string_view text) -> std::string_view {
	while (text.starts_with(' ') || text.starts_with('\t')) {
		text.remove_prefix(1);
	}
	while (text.ends_with(' ') || text.ends_with('\t')) {
		text.remove_suffix(1);
	}
	return text;
}

[[nodiscard]] constexpr auto is_http_version(std::string_view text) -> bool {
	constexpr std::string_view prefix = "HTTP/";
	return text.size() == prefix.size() + 3 && text.starts_with(prefix) && (text[5] >= '0' && text[5] <= '9') && text[6] == '.' &&
	       (text[7] >= '0' && text[7] <= '9');
}

[[nodiscard]] constexpr auto next_line(std::string_view& remaining) -> std::string_view {
	auto const end = remaining.find(crlf);
	if (end == std::string_view::npos) {
		return std::exchange(remaining, std::string_view{});
	}
	auto const line = remaining.substr(0, end);
	remaining.remove_prefix(end + crlf.size());
	return line;
}

[[nodiscard]] constexpr auto decimal_width(std::size_t value) -> std::size_t {
	auto width = std::size_t{1};
	while (value >= 10) {
		value /= 10;
		++width;
	}
	return width;
}

[[nodiscard]] constexpr auto checked_add(std::size_t left, std::size_t right) -> std::optional<std::size_t> {
	if (right > std::numeric_limits<std::size_t>::max() - left) {
		return std::nullopt;
	}
	return left + right;
}

[[nodiscard]] auto add_size(std::size_t& total, std::size_t part) -> bool {
	auto const sum = checked_add(total, part);
	if (!sum) {
		return false;
	}
	total = *sum;
	return true;
}

}

export [[nodiscard]] auto parse_request(std::string_view input) -> std::expected<Request, HttpError> {
	if (input.size() > max_request_bytes) {
		return std::unexpected(HttpError{HttpErrorKind::MessageTooLarge});
	}
	auto const head_end = input.find(head_terminator);
	if (head_end == std::string_view::npos) {
		return std::unexpected(HttpError{input.size() == max_request_bytes ? HttpErrorKind::MessageTooLarge : HttpErrorKind::Incomplete});
	}

	auto head = input.substr(0, head_end);
	auto const request_line = next_line(head);

	auto const method_end = request_line.find(' ');
	if (method_end == std::string_view::npos) {
		return std::unexpected(HttpError{HttpErrorKind::Malformed});
	}
	auto const method = request_line.substr(0, method_end);

	auto const rest = request_line.substr(method_end + 1);
	auto const target_end = rest.find(' ');
	if (target_end == std::string_view::npos) {
		return std::unexpected(HttpError{HttpErrorKind::Malformed});
	}
	auto const target = rest.substr(0, target_end);
	auto const version = rest.substr(target_end + 1);

	if (!is_token(method) || target.empty() || !std::ranges::all_of(target, is_vchar) || !is_http_version(version)) {
		return std::unexpected(HttpError{HttpErrorKind::Malformed});
	}

	auto request = Request{.method = std::string{method}, .target = std::string{target}, .version = std::string{version}, .headers = {}};
	request.headers.reserve(max_header_count);

	while (!head.empty()) {
		auto const line = next_line(head);
		auto const colon = line.find(':');
		if (colon == std::string_view::npos) {
			return std::unexpected(HttpError{HttpErrorKind::Malformed});
		}
		auto const name = line.substr(0, colon);
		auto const value = trim_ows(line.substr(colon + 1));
		if (!is_token(name) || !std::ranges::all_of(value, is_field_char)) {
			return std::unexpected(HttpError{HttpErrorKind::Malformed});
		}
		if (request.headers.size() == max_header_count) {
			return std::unexpected(HttpError{HttpErrorKind::TooManyHeaders});
		}
		request.headers.push_back(Header{.name = std::string{name}, .value = std::string{value}});
	}

	return request;
}

/**
 * Returns one owned connection-closing response. Framing is owned here:
 * callers may not supply Content-Length, Transfer-Encoding, or Connection.
 * Size arithmetic and the wire bound are checked before allocation.
 */
export [[nodiscard]] auto write_response(Response const& response) -> std::expected<std::string, HttpError> {
	constexpr std::string_view status_prefix = "HTTP/1.1 ";
	constexpr std::string_view connection_close = "Connection: close\r\n";
	constexpr std::string_view length_prefix = "Content-Length: ";

	if (response.status < 100 || response.status > 599 || !std::ranges::all_of(response.reason, is_field_char)) {
		return std::unexpected(HttpError{HttpErrorKind::Malformed});
	}
	if (response.headers.size() > max_header_count) {
		return std::unexpected(HttpError{HttpErrorKind::TooManyHeaders});
	}
	for (auto const& header : response.headers) {
		if (!is_token(header.name) || is_reserved_response_header(header.name) || !std::ranges::all_of(header.value, is_field_char)) {
			return std::unexpected(HttpError{HttpErrorKind::Malformed});
		}
	}

	auto needed = std::size_t{0};
	if (!add_size(needed, status_prefix.size()) || !add_size(needed, 3) || !add_size(needed, 1) ||
	    !add_size(needed, response.reason.size()) || !add_size(needed, crlf.size())) {
		return std::unexpected(HttpError{HttpErrorKind::SizeOverflow});
	}
	for (auto const& header : response.headers) {
		if (!add_size(needed, header.name.size()) || !add_size(needed, 2) || !add_size(needed, header.value.size()) ||
		    !add_size(needed, crlf.size())) {
			return std::unexpected(HttpError{HttpErrorKind::SizeOverflow});
		}
	}
	if (!add_size(needed, connection_close.size()) || !add_size(needed, length_prefix.size()) ||
	    !add_size(needed, decimal_width(response.body.size())) || !add_size(needed, crlf.size()) || !add_size(needed, crlf.size()) ||
	    !add_size(needed, response.body.size())) {
		return std::unexpected(HttpError{HttpErrorKind::SizeOverflow});
	}
	if (needed > max_response_bytes) {
		return std::unexpected(HttpError{HttpErrorKind::MessageTooLarge});
	}

	auto output = std::string(needed, '\0');
	auto cursor = output.begin();
	cursor = std::format_to(cursor, "{}{} {}{}", status_prefix, response.status, response.reason, crlf);
	for (auto const& header : response.headers) {
		cursor = std::format_to(cursor, "{}: {}{}", header.name, header.value, crlf);
	}
	cursor = std::ranges::copy(connection_close, cursor).out;
	cursor = std::format_to(cursor, "{}{}{}{}", length_prefix, response.body.size(), crlf, crlf);
	cursor = std::ranges::copy(response.body, cursor).out;

	auto const written = static_cast<std::size_t>(std::ranges::distance(output.begin(), cursor));
	contract_assert(written == needed);
	return output;
}

}
