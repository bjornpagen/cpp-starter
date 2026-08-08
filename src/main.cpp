import std;
import starter.core;

auto main() -> int {
    auto const result = starter::greeting("world");
    if (!result.has_value()) {
        return 1;
    }
    std::println("{}", result.value());
    return 0;
}
