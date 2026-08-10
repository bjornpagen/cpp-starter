import std;
import starter;

namespace {

struct CaseResult {
	std::string_view name;
	bool passed;
};

using Expected = std::expected<std::int32_t, starter::ExecError>;

[[nodiscard]] auto check_value_chain() -> CaseResult {
	return CaseResult{
	    .name = "value chain (just | let_value | then) yields 2*seed + 1",
	    .passed = starter::exec_value_chain(20) == starter::ExecResult{Expected{41}},
	};
}

[[nodiscard]] auto check_error_recovery() -> CaseResult {
	return CaseResult{
	    .name = "upon_error recovers a typed error to a value, payload preserved",
	    .passed = starter::exec_error_recovery_chain(7) == starter::ExecResult{Expected{8}},
	};
}

[[nodiscard]] auto check_error_reroute() -> CaseResult {
	return CaseResult{
	    .name = "let_error reroutes a typed error to a new sender, payload preserved",
	    .passed = starter::exec_error_reroute_chain(21) == starter::ExecResult{Expected{42}},
	};
}

[[nodiscard]] auto check_error_passthrough() -> CaseResult {
	return CaseResult{
	    .name = "wait's error channel delivers the typed error, code intact",
	    .passed = starter::exec_error_passthrough_chain(123) == starter::ExecResult{Expected{std::unexpect, starter::ExecError{123}}},
	};
}

[[nodiscard]] auto check_error_classification() -> CaseResult {
	return CaseResult{
	    .name = "opaque execution errors default to permanent",
	    .passed = !starter::ExecError{123}.is_transient(),
	};
}

[[nodiscard]] auto check_stopped() -> CaseResult {
	return CaseResult{
	    .name = "wait's stopped channel is nullopt, distinct from any error",
	    .passed = starter::exec_stopped_chain() == starter::ExecResult{std::nullopt},
	};
}

[[nodiscard]] auto check_pool_when_all() -> CaseResult {
	return CaseResult{
	    .name = "when_all joins three pool tasks (starts_on/schedule/on + continues_on)",
	    .passed = starter::exec_pool_when_all_sum(1, 2, 3) == starter::ExecResult{Expected{12}},
	};
}

[[nodiscard]] auto check_variant_roundtrip() -> CaseResult {
	return CaseResult{
	    .name = "into_variant round-trips the value",
	    .passed = starter::exec_variant_roundtrip(9) == starter::ExecResult{Expected{9}},
	};
}

[[nodiscard]] auto check_stopped_as_optional_value() -> CaseResult {
	return CaseResult{
	    .name = "stopped_as_optional passes a value through engaged",
	    .passed = starter::exec_stopped_as_optional_chain(starter::StopMode::Complete, 41) == starter::ExecResult{Expected{41}},
	};
}

[[nodiscard]] auto check_stopped_as_optional_stopped() -> CaseResult {
	return CaseResult{
	    .name = "stopped_as_optional materializes stopped as the disengaged marker",
	    .passed = starter::exec_stopped_as_optional_chain(starter::StopMode::Stop, 41) == starter::ExecResult{Expected{-1}},
	};
}

}

auto main() -> int {
	auto const results = std::array{
	    check_value_chain(),
	    check_error_recovery(),
	    check_error_reroute(),
	    check_error_passthrough(),
	    check_error_classification(),
	    check_stopped(),
	    check_pool_when_all(),
	    check_variant_roundtrip(),
	    check_stopped_as_optional_value(),
	    check_stopped_as_optional_stopped(),
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
