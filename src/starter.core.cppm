export module starter.core;

import std;

export namespace starter {

enum class GreetError : std::uint8_t {
    EmptyName,
};

auto greeting(std::string_view name)
    -> std::expected<std::string, GreetError>
{
    if (name.empty()) {
        return std::unexpected(GreetError::EmptyName);
    }
    return std::format("hello, {}", name);
}

} // namespace starter
