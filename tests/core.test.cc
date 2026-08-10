import std;
import starter;

namespace {

struct CaseResult {
	std::string_view name;
	bool passed;
};

using Greeting = std::expected<std::string, starter::GreetError>;

[[nodiscard]] auto check_greeting_formats_name() -> CaseResult {
	return CaseResult{
	    .name = "greeting formats the given name",
	    .passed = starter::greeting("modules") == Greeting{"hello, modules"},
	};
}

[[nodiscard]] auto check_greeting_rejects_empty_name() -> CaseResult {
	return CaseResult{
	    .name = "greeting rejects an empty name",
	    .passed = starter::greeting("") == std::unexpected(starter::GreetError{starter::GreetErrorKind::EmptyName}),
	};
}

[[nodiscard]] auto check_greeting_error_classification() -> CaseResult {
	return CaseResult{
	    .name = "invalid greeting input is permanently classified",
	    .passed = !starter::GreetError{starter::GreetErrorKind::EmptyName}.is_transient(),
	};
}

}

auto main() -> int {
	auto const results = std::array{
	    check_greeting_formats_name(),
	    check_greeting_rejects_empty_name(),
	    check_greeting_error_classification(),
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
