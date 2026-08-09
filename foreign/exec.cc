// Sender/receiver execution surface over the vendored reference
// implementation (NVIDIA stdexec, pinned by SHA in the top-level
// CMakeLists.txt). The exported interface is dialect-clean; every sender
// composition lives in exec.backend.cc — the combinator half of the swap
// boundary (AGENTS.md §15) — and is reached through a narrow ABI, exactly
// like the :net partition reaches its kqueue io-context.
//
// Why the boundary is shaped this way: GCC 16.1 ICEs whenever a stdexec
// header is textually included in ANY module unit (global-module-fragment
// extern-const/inline-constexpr CPO pattern, pinned in exec.backend.cc), so
// senders cannot cross the module boundary on this toolchain. What crosses
// instead is the executable spec of the boundary: one concrete chain per
// probe-verified combinator, each returning the result shape of OUR
// expected-erroring wait —
//
//   engaged optional + value      the chain's value channel
//   engaged optional + ExecError  the chain's typed error channel, payload
//                                 preserved (never exception_ptr)
//   std::nullopt                  the chain was stopped (cancellation)
//
// When __cpp_lib_senders lands (tombstone in tests/conformance.test.cc),
// the vendor is deleted and the boundary is rewritten over std::execution;
// importers of this partition are untouched.
export module starter:exec;

import std;

// Narrow ABI to the swap-boundary TU: the extern "C++" block attaches these
// declarations to the global module, matching the plain definitions in
// exec.backend.cc (same mechanism and rationale as :net's io-context).
extern "C++" {
namespace starter::exec_backend {

[[nodiscard]] auto value_chain(std::int32_t seed) noexcept -> std::optional<std::expected<std::int32_t, std::int32_t>>;

[[nodiscard]] auto error_recovery_chain(std::int32_t code) noexcept -> std::optional<std::expected<std::int32_t, std::int32_t>>;

[[nodiscard]] auto error_reroute_chain(std::int32_t code) noexcept -> std::optional<std::expected<std::int32_t, std::int32_t>>;

[[nodiscard]] auto error_passthrough_chain(std::int32_t code) noexcept -> std::optional<std::expected<std::int32_t, std::int32_t>>;

[[nodiscard]] auto stopped_chain() noexcept -> std::optional<std::expected<std::int32_t, std::int32_t>>;

[[nodiscard]] auto pool_when_all_sum(std::int32_t a, std::int32_t b, std::int32_t c) noexcept
    -> std::optional<std::expected<std::int32_t, std::int32_t>>;

[[nodiscard]] auto variant_roundtrip(std::int32_t value) noexcept -> std::optional<std::expected<std::int32_t, std::int32_t>>;

[[nodiscard]] auto stopped_as_optional_chain(bool stop, std::int32_t value) noexcept
    -> std::optional<std::expected<std::int32_t, std::int32_t>>;

} // namespace starter::exec_backend
}

namespace starter {

// The typed asynchronous error: sender error channels carry values, never
// exceptions (AGENTS.md §11); only the payload crosses the narrow ABI.
export struct [[nodiscard]] ExecError {
	std::int32_t code;

	// Spelled out, not `= default`: GCC 16.1 pinned quirk — streaming a
	// defaulted (friend or member) comparison of an exported partition type
	// through the BMI ICEs the importer (segfault at the operator's
	// declaration). Re-try `= default` on the next toolchain bump.
	[[nodiscard]] constexpr auto operator==(ExecError const& other) const -> bool {
		return code == other.code;
	}
};

// The result shape of the boundary's expected-erroring wait: nullopt is the
// stopped (cancellation) channel, unexpected is the typed error channel.
export using ExecResult = std::optional<std::expected<std::int32_t, ExecError>>;

namespace {

[[nodiscard]] auto lift(std::optional<std::expected<std::int32_t, std::int32_t>> raw) -> ExecResult {
	if (!raw) {
		return std::nullopt;
	}
	return raw->transform_error([](std::int32_t code) {
		return ExecError{code};
	});
}

} // namespace

// just | let_value | then through the value channel: 2*seed + 1.
export [[nodiscard]] auto exec_value_chain(std::int32_t seed) -> ExecResult {
	return lift(exec_backend::value_chain(seed));
}

// just_error | upon_error: typed error recovered to a value, code + 1.
export [[nodiscard]] auto exec_error_recovery_chain(std::int32_t code) -> ExecResult {
	return lift(exec_backend::error_recovery_chain(code));
}

// just_error | let_error: typed error rerouted to a new sender, 2*code.
export [[nodiscard]] auto exec_error_reroute_chain(std::int32_t code) -> ExecResult {
	return lift(exec_backend::error_reroute_chain(code));
}

// Typed error delivered through wait's error channel: unexpected(code).
export [[nodiscard]] auto exec_error_passthrough_chain(std::int32_t code) -> ExecResult {
	return lift(exec_backend::error_passthrough_chain(code));
}

// Stopped channel delivered through wait: std::nullopt.
export [[nodiscard]] auto exec_stopped_chain() -> ExecResult {
	return lift(exec_backend::stopped_chain());
}

// when_all join of three thread-pool tasks (starts_on, schedule, on),
// rejoined via continues_on: 2*(a + b + c).
export [[nodiscard]] auto exec_pool_when_all_sum(std::int32_t a, std::int32_t b, std::int32_t c) -> ExecResult {
	return lift(exec_backend::pool_when_all_sum(a, b, c));
}

// into_variant round trip: the value comes back unchanged.
export [[nodiscard]] auto exec_variant_roundtrip(std::int32_t value) -> ExecResult {
	return lift(exec_backend::variant_roundtrip(value));
}

// stopped_as_optional: value passes through engaged; stopped materializes
// as the disengaged marker -1 instead of ending the chain.
export [[nodiscard]] auto exec_stopped_as_optional_chain(bool stop, std::int32_t value) -> ExecResult {
	return lift(exec_backend::stopped_as_optional_chain(stop, value));
}

} // namespace starter
