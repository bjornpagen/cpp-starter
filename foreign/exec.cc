export module starter:exec;

import std;

/* PIN(gcc-gmf-stdexec-ice): narrow ABI to the plain backend TU — senders cannot cross the module boundary; see PINS.md */
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

}
}

namespace starter {

/** The typed asynchronous error; only its payload crosses the narrow ABI. */
export struct [[nodiscard]] ExecError {
	std::int32_t code;

	/** Opaque boundary codes have no known retry condition. */
	[[nodiscard]] constexpr auto is_transient() const -> bool {
		return false;
	}

	[[nodiscard]] constexpr auto operator==(ExecError const&) const -> bool = default;
};

export enum class StopMode : std::uint8_t {
	Complete,
	Stop,
};

/**
 * The result shape of the boundary's expected-erroring wait: nullopt is
 * the stopped (cancellation) channel, unexpected the typed error
 * channel.
 */
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

}

/** just | let_value | then through the value channel: 2*seed + 1. */
export [[nodiscard]] auto exec_value_chain(std::int32_t seed) -> ExecResult {
	return lift(exec_backend::value_chain(seed));
}

/** just_error | upon_error: typed error recovered to a value, code + 1. */
export [[nodiscard]] auto exec_error_recovery_chain(std::int32_t code) -> ExecResult {
	return lift(exec_backend::error_recovery_chain(code));
}

/** just_error | let_error: typed error rerouted to a new sender, 2*code. */
export [[nodiscard]] auto exec_error_reroute_chain(std::int32_t code) -> ExecResult {
	return lift(exec_backend::error_reroute_chain(code));
}

/** Typed error delivered through wait's error channel: unexpected(code). */
export [[nodiscard]] auto exec_error_passthrough_chain(std::int32_t code) -> ExecResult {
	return lift(exec_backend::error_passthrough_chain(code));
}

/** Stopped channel delivered through wait: std::nullopt. */
export [[nodiscard]] auto exec_stopped_chain() -> ExecResult {
	return lift(exec_backend::stopped_chain());
}

/**
 * when_all join of three thread-pool tasks (starts_on, schedule, on),
 * rejoined via continues_on: 2*(a + b + c).
 */
export [[nodiscard]] auto exec_pool_when_all_sum(std::int32_t a, std::int32_t b, std::int32_t c) -> ExecResult {
	return lift(exec_backend::pool_when_all_sum(a, b, c));
}

/** into_variant round trip: the value comes back unchanged. */
export [[nodiscard]] auto exec_variant_roundtrip(std::int32_t value) -> ExecResult {
	return lift(exec_backend::variant_roundtrip(value));
}

/**
 * stopped_as_optional: value passes through engaged; stopped
 * materializes as the disengaged marker -1 instead of ending the chain.
 */
export [[nodiscard]] auto exec_stopped_as_optional_chain(StopMode mode, std::int32_t value) -> ExecResult {
	return lift(exec_backend::stopped_as_optional_chain(mode == StopMode::Stop, value));
}

}
