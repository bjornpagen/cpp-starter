import std;
import starter;

namespace {

struct CaseResult {
	std::string_view name;
	bool passed;
};

auto check_enum_name_derives_identifier() -> CaseResult {
	return CaseResult{
		.name = "enum_name derives the enumerator identifier via reflection",
		.passed = starter::enum_name(starter::GreetError::EmptyName)
			== std::optional<std::string_view>{"EmptyName"},
	};
}

auto check_enum_name_rejects_unnamed_value() -> CaseResult {
	return CaseResult{
		.name = "enum_name reports absence for a value with no enumerator",
		.passed = starter::enum_name(static_cast<starter::GreetError>(255))
			== std::nullopt,
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
