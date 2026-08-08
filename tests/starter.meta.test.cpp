import std;
import starter.core;
import starter.meta;

namespace {

struct CaseResult {
    std::string_view name;
    bool passed;
};

auto check_enum_name_derives_identifier() -> CaseResult {
    auto const name = starter::meta::enum_name(starter::GreetError::EmptyName);
    return CaseResult{
        .name = "enum_name derives the enumerator identifier via reflection",
        .passed = name.has_value() && name.value() == "EmptyName",
    };
}

auto check_enum_name_rejects_unnamed_value() -> CaseResult {
    auto const name = starter::meta::enum_name(static_cast<starter::GreetError>(255));
    return CaseResult{
        .name = "enum_name reports absence for a value with no enumerator",
        .passed = !name.has_value(),
    };
}

} // namespace

auto main() -> int {
    auto const results = std::array{
        check_enum_name_derives_identifier(),
        check_enum_name_rejects_unnamed_value(),
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
