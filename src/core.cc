export module starter:core;

import std;

namespace starter {

export enum class GreetErrorKind {
	EmptyName,
};

export struct [[nodiscard]] GreetError {
	GreetErrorKind kind;

	[[nodiscard]] constexpr auto is_transient() const -> bool {
		switch (kind) {
		case GreetErrorKind::EmptyName:
			return false;
		}
	}

	[[nodiscard]] constexpr auto operator==(GreetError const&) const -> bool = default;
};

export [[nodiscard]] auto greeting(std::string_view name) -> std::expected<std::string, GreetError> {
	if (name.empty()) {
		return std::unexpected(GreetError{GreetErrorKind::EmptyName});
	}
	return std::format("hello, {}", name);
}

}
