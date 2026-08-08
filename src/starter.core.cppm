export module starter.core;

import std;

namespace starter {

export enum class GreetError {
    EmptyName,
};

export auto greeting(std::string_view name)
    -> std::expected<std::string, GreetError>;

} // namespace starter
