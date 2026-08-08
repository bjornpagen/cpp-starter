import std;
import starter.core;

namespace {

struct CaseResult {
    std::string_view name;
    bool passed;
};

auto check_greeting_formats_name() -> CaseResult {
    auto const result = starter::greeting("modules");
    return CaseResult{
        .name = "greeting formats the given name",
        .passed = result.has_value() && result.value() == "hello, modules",
    };
}

auto check_greeting_rejects_empty_name() -> CaseResult {
    auto const result = starter::greeting("");
    return CaseResult{
        .name = "greeting rejects an empty name",
        .passed = !result.has_value()
            && result.error() == starter::GreetError::EmptyName,
    };
}

} // namespace

auto main() -> int {
    auto const results = std::array{
        check_greeting_formats_name(),
        check_greeting_rejects_empty_name(),
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
