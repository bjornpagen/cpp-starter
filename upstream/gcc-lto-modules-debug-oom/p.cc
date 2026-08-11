export module m:p;

import std;

namespace n {

export struct S {
	std::string text;

	[[nodiscard]] auto describe() const -> std::string;
};

auto S::describe() const -> std::string {
	return std::format("S({})", text);
}

export [[nodiscard]] auto make(std::string_view name) -> std::expected<S, int> {
	if (name.empty()) {
		return std::unexpected(1);
	}
	return S{std::string{name}};
}

}
