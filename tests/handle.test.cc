import std;
import starter;

namespace {

struct CaseResult {
	std::string_view name;
	bool passed;
};

using Table = starter::ConnectionTable<int, 4, 3>;

[[nodiscard]] auto check_pack_roundtrip() -> CaseResult {
	auto const handle = starter::pack_connection_handle(7, 42);
	return CaseResult{
	    .name = "pack/unpack preserves index and generation",
	    .passed = starter::connection_index(handle) == 7 && starter::connection_generation(handle) == 42,
	};
}

[[nodiscard]] auto check_insert_get_release() -> CaseResult {
	auto table = Table{};
	auto const handle = table.insert(11);
	auto const found = handle ? table.try_get(*handle) : std::optional<int&>{};
	auto const live = found.has_value() && *found == 11;
	if (handle) {
		table.release(*handle);
	}
	auto const stale = handle && !table.try_get(*handle);
	return CaseResult{.name = "insert, get, release: live succeeds, released is stale", .passed = live && stale};
}

[[nodiscard]] auto check_stale_after_reuse() -> CaseResult {
	auto table = Table{};
	auto const first = table.insert(1);
	if (!first) {
		return CaseResult{.name = "reused slot rejects the prior generation", .passed = false};
	}
	table.release(*first);
	auto const second = table.insert(2);
	auto const found = second ? table.try_get(*second) : std::optional<int&>{};
	auto const new_live = found.has_value() && *found == 2;
	auto const old_stale = !table.try_get(*first);
	return CaseResult{.name = "reused slot rejects the prior generation", .passed = new_live && old_stale};
}

[[nodiscard]] auto check_churn_no_stale_success() -> CaseResult {
	auto table = Table{};
	auto live = std::array<std::optional<starter::ConnectionHandle>, 4>{};
	auto passed = true;
	for (auto round = 0; round < 32; ++round) {
		for (auto index = std::size_t{0}; index < live.size(); ++index) {
			if (live[index]) {
				passed = passed && table.try_get(*live[index]).has_value();
				table.release(*live[index]);
				passed = passed && !table.try_get(*live[index]);
				live[index] = std::nullopt;
			}
			live[index] = table.insert(round);
			if (live[index]) {
				passed = passed && table.try_get(*live[index]).has_value();
			}
		}
	}
	for (auto const& handle : live) {
		if (handle) {
			table.release(*handle);
			passed = passed && !table.try_get(*handle);
		}
	}
	return CaseResult{.name = "create/destroy churn never validates a stale handle", .passed = passed};
}

[[nodiscard]] auto check_generation_retirement() -> CaseResult {
	auto table = starter::ConnectionTable<int, 1, 3>{};
	auto stale_rejected = true;
	for (auto cycle = 0; cycle < 16; ++cycle) {
		auto handle = table.insert(cycle);
		if (!handle) {
			break;
		}
		auto const prior = *handle;
		table.release(prior);
		stale_rejected = stale_rejected && !table.try_get(prior);
	}
	auto const after = table.insert(99);
	return CaseResult{
	    .name = "generation exhaustion retires slots and never wraps into a valid stale handle",
	    .passed = !after && stale_rejected && table.retired_count() == 1,
	};
}

[[nodiscard]] auto check_distinct_type() -> CaseResult {
	return CaseResult{
	    .name = "ConnectionHandle is a struct, not a typedef of the integer",
	    .passed = !std::is_same_v<starter::ConnectionHandle, std::uint64_t>,
	};
}

}

auto main() -> int {
	auto const results = std::array{
	    check_pack_roundtrip(),
	    check_insert_get_release(),
	    check_stale_after_reuse(),
	    check_churn_no_stale_success(),
	    check_generation_retirement(),
	    check_distinct_type(),
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