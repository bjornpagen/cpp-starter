import std;
import starter.core;

auto main() -> int {
    return starter::greeting("world")
        .transform([](std::string const& text) -> int {
            std::println("{}", text);
            return 0;
        })
        .value_or(1);
}
