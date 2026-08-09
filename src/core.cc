export module starter:core;

import std;

namespace starter {

export enum class [[nodiscard]] GreetError {
	EmptyName,
};

export [[nodiscard]] auto greeting(std::string_view name) -> std::expected<std::string, GreetError> {
	if (name.empty()) {
		return std::unexpected(GreetError::EmptyName);
	}
	return std::format("hello, {}", name);
}

}
