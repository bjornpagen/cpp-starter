export module starter:http;

import std;

namespace starter {

/**
 * Incomplete is the torn-buffer signal, not a rejection: the head
 * terminator has not arrived yet, so the caller keeps reading.
 */
export enum class [[nodiscard]] HttpError : std::uint8_t {
	Incomplete,
	Malformed,
	TooManyHeaders,
	BufferTooSmall,
};

export inline constexpr std::size_t max_header_count = 32;

export struct HeaderView {
	std::string_view name;
	std::string_view value;
};

/**
 * The parsed request head: views borrowed from the parse input, valid only
 * while that buffer is. Only headers[0 .. header_count) are meaningful.
 */
export struct RequestView {
	std::string_view method;
	std::string_view target;
	std::string_view version;
	/* PIN(gcc-partition-bmi-inplace-vector): array + count, not inplace_vector — see PINS.md */
	std::array<HeaderView, max_header_count> headers;
	std::size_t header_count;
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

}

export [[nodiscard]] auto parse_request(std::string_view input) -> std::expected<RequestView, HttpError> {
	auto const head_end = input.find(head_terminator);
	if (head_end == std::string_view::npos) {
		return std::unexpected(HttpError::Incomplete);
	}

	auto head = input.substr(0, head_end);
	auto const request_line = next_line(head);

	auto const method_end = request_line.find(' ');
	if (method_end == std::string_view::npos) {
		return std::unexpected(HttpError::Malformed);
	}
	auto const method = request_line.substr(0, method_end);

	auto const rest = request_line.substr(method_end + 1);
	auto const target_end = rest.find(' ');
	if (target_end == std::string_view::npos) {
		return std::unexpected(HttpError::Malformed);
	}
	auto const target = rest.substr(0, target_end);
	auto const version = rest.substr(target_end + 1);

	if (!is_token(method) || target.empty() || !std::ranges::all_of(target, is_vchar) || !is_http_version(version)) {
		return std::unexpected(HttpError::Malformed);
	}

	auto request = RequestView{.method = method, .target = target, .version = version, .headers = {}, .header_count = 0};

	while (!head.empty()) {
		auto const line = next_line(head);
		auto const colon = line.find(':');
		if (colon == std::string_view::npos) {
			return std::unexpected(HttpError::Malformed);
		}
		auto const name = line.substr(0, colon);
		auto const value = trim_ows(line.substr(colon + 1));
		if (!is_token(name) || !std::ranges::all_of(value, is_field_char)) {
			return std::unexpected(HttpError::Malformed);
		}
		if (request.header_count == max_header_count) {
			return std::unexpected(HttpError::TooManyHeaders);
		}
		request.headers[request.header_count] = HeaderView{.name = name, .value = value};
		++request.header_count;
	}

	return request;
}

export struct ResponseHead {
	std::uint16_t status;
	std::string_view reason;
};

/**
 * Writes the full response — status line, `headers`, a derived
 * Content-Length, blank line, `body` — into `out` and returns the byte
 * count. The size is computed first, so on failure `out` is untouched.
 */
export [[nodiscard]] auto write_response(ResponseHead head, std::span<HeaderView const> headers, std::string_view body, std::span<char> out)
    -> std::expected<std::size_t, HttpError> {
	constexpr std::string_view status_prefix = "HTTP/1.1 ";
	constexpr std::string_view length_prefix = "Content-Length: ";

	if (head.status < 100 || head.status > 999 || !std::ranges::all_of(head.reason, is_field_char)) {
		return std::unexpected(HttpError::Malformed);
	}
	for (auto const& header : headers) {
		if (!is_token(header.name) || !std::ranges::all_of(header.value, is_field_char)) {
			return std::unexpected(HttpError::Malformed);
		}
	}

	auto needed = status_prefix.size() + 3 + 1 + head.reason.size() + crlf.size();
	for (auto const& header : headers) {
		needed += header.name.size() + 2 + header.value.size() + crlf.size();
	}
	needed += length_prefix.size() + decimal_width(body.size()) + crlf.size() + crlf.size() + body.size();
	if (needed > out.size()) {
		return std::unexpected(HttpError::BufferTooSmall);
	}

	auto cursor = out.begin();
	cursor = std::format_to(cursor, "{}{} {}{}", status_prefix, head.status, head.reason, crlf);
	for (auto const& header : headers) {
		cursor = std::format_to(cursor, "{}: {}{}", header.name, header.value, crlf);
	}
	cursor = std::format_to(cursor, "{}{}{}{}", length_prefix, body.size(), crlf, crlf);
	cursor = std::ranges::copy(body, cursor).out;

	auto const written = static_cast<std::size_t>(std::ranges::distance(out.begin(), cursor));
	contract_assert(written == needed);
	return written;
}

}
