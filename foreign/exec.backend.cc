/* PIN(gcc-gmf-stdexec-ice): plain TU — with unsafe/net.backend.cc, the repository's entire stdexec spelling surface; see PINS.md */
#include <cstdint>
#include <exception>
#include <expected>
#include <optional>
#include <tuple>
#include <utility>
#include <variant>

#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

/** Exactly the probe-verified vocabulary subset; nothing else escapes. */
namespace ex {

using stdexec::continues_on;
using stdexec::into_variant;
using stdexec::just;
using stdexec::just_error;
using stdexec::just_stopped;
using stdexec::let_error;
using stdexec::let_value;
using stdexec::on;
using stdexec::schedule;
using stdexec::starts_on;
using stdexec::stopped_as_optional;
using stdexec::then;
using stdexec::upon_error;
using stdexec::when_all;

using exec::static_thread_pool;

namespace ours {

/**
 * Mirrors stdexec::sync_wait's environment: get_scheduler,
 * get_start_scheduler, and get_delegation_scheduler all answer with the
 * run_loop's scheduler, so combinators that must return to "the
 * caller's context" (`on`) complete back on this loop. Signature
 * computation must use this same environment, not env<>.
 */
struct WaitEnv {
	stdexec::run_loop* loop;

	[[nodiscard]] auto query(stdexec::get_scheduler_t) const noexcept -> stdexec::run_loop::scheduler {
		return loop->get_scheduler();
	}

	[[nodiscard]] auto query(stdexec::get_start_scheduler_t) const noexcept -> stdexec::run_loop::scheduler {
		return loop->get_scheduler();
	}

	[[nodiscard]] auto query(stdexec::get_delegation_scheduler_t) const noexcept -> stdexec::run_loop::scheduler {
		return loop->get_scheduler();
	}
};

template<class Tuple, class E>
struct WaitState {
	std::optional<std::expected<Tuple, E>> result;
	bool stopped = false;
	stdexec::run_loop loop;
};

template<class Tuple, class E>
struct WaitReceiver {
	using receiver_concept = stdexec::receiver_t;
	WaitState<Tuple, E>* state;

	template<class... Vs>
	auto set_value(Vs&&... vs) noexcept -> void {
		state->result.emplace(std::in_place, std::forward<Vs>(vs)...);
		state->loop.finish();
	}

	auto set_error(E e) noexcept -> void {
		state->result.emplace(std::unexpect, std::move(e));
		state->loop.finish();
	}

	/** Unreachable under -fno-exceptions; reaching it is process failure. */
	auto set_error(std::exception_ptr) noexcept -> void {
		std::terminate();
	}

	auto set_stopped() noexcept -> void {
		state->stopped = true;
		state->loop.finish();
	}

	[[nodiscard]] auto get_env() const noexcept -> WaitEnv {
		return WaitEnv{&state->loop};
	}
};

template<class T>
using Single = T;

template<class Sndr>
using ValueTuple = stdexec::value_types_of_t<Sndr, WaitEnv, std::tuple, Single>;

/**
 * The dialect's synchronous wait: set_value(vs...) is an engaged
 * expected holding tuple{vs...}; set_error(E) is unexpected(e), payload
 * preserved; set_stopped() is std::nullopt. stdexec::sync_wait is
 * deliberately absent — its specified optional<tuple> shape cannot
 * return errors as values.
 */
template<class E, stdexec::sender Sndr>
[[nodiscard]] auto wait(Sndr&& sndr) -> std::optional<std::expected<ValueTuple<Sndr>, E>> {
	using Tuple = ValueTuple<Sndr>;
	WaitState<Tuple, E> state;
	auto op = stdexec::connect(std::forward<Sndr>(sndr), WaitReceiver<Tuple, E>{&state});
	stdexec::start(op);
	state.loop.run();
	if (state.stopped) {
		return std::nullopt;
	}
	return std::move(state.result);
}

}

using ours::wait;

}

namespace starter::exec_backend {

namespace {

struct BoundaryError {
	std::int32_t code;
};

using ChainResult = std::optional<std::expected<std::int32_t, std::int32_t>>;

/**
 * Probe-pinned caveat, kept as compile coverage: just_stopped() is a
 * well-formed sender, but every wait site statically requires at least
 * one set_value signature, so it can only appear where a value channel
 * survives.
 */
static_assert(stdexec::sender<decltype(ex::just_stopped())>);

enum class Channel : std::uint8_t {
	Value,
	Error,
	Stopped,
};

/**
 * The minimal sender with all three completion channels declared,
 * choosing one at runtime: wait sites need the value signature to exist
 * even when the error or stopped path is taken (bare just_error and
 * just_stopped have no value signature and are statically rejected
 * there).
 */
struct ProbeSender {
	using sender_concept = stdexec::sender_t;
	using completion_signatures =
	    stdexec::completion_signatures<stdexec::set_value_t(std::int32_t), stdexec::set_error_t(BoundaryError), stdexec::set_stopped_t()>;

	Channel channel;
	std::int32_t payload;

	template<class Rcvr>
	struct Operation {
		Rcvr receiver;
		Channel channel;
		std::int32_t payload;

		auto start() & noexcept -> void {
			switch (channel) {
			case Channel::Value:
				stdexec::set_value(std::move(receiver), payload);
				return;
			case Channel::Error:
				stdexec::set_error(std::move(receiver), BoundaryError{payload});
				return;
			case Channel::Stopped:
				stdexec::set_stopped(std::move(receiver));
				return;
			}
		}
	};

	template<class Rcvr>
	[[nodiscard]] auto connect(Rcvr rcvr) const noexcept -> Operation<Rcvr> {
		return {std::move(rcvr), channel, payload};
	}
};

[[nodiscard]] auto flatten(auto&& waited) -> ChainResult {
	if (!waited) {
		return std::nullopt;
	}
	if (!*waited) {
		return std::expected<std::int32_t, std::int32_t>{std::unexpect, waited->error().code};
	}
	return std::expected<std::int32_t, std::int32_t>{std::get<0>(**waited)};
}

}

[[nodiscard]] auto value_chain(std::int32_t seed) noexcept -> ChainResult {
	auto chain = ex::just(seed) | ex::let_value([](std::int32_t v) {
		             return ex::just(v + v);
	             }) |
	             ex::then([](std::int32_t v) {
		             return v + 1;
	             });
	return flatten(ex::wait<BoundaryError>(std::move(chain)));
}

[[nodiscard]] auto error_recovery_chain(std::int32_t code) noexcept -> ChainResult {
	auto chain = ex::just_error(BoundaryError{code}) | ex::upon_error([](BoundaryError e) {
		             return e.code + 1;
	             });
	return flatten(ex::wait<BoundaryError>(std::move(chain)));
}

[[nodiscard]] auto error_reroute_chain(std::int32_t code) noexcept -> ChainResult {
	auto chain = ex::just_error(BoundaryError{code}) | ex::let_error([](BoundaryError e) {
		             return ex::just(e.code + e.code);
	             });
	return flatten(ex::wait<BoundaryError>(std::move(chain)));
}

[[nodiscard]] auto error_passthrough_chain(std::int32_t code) noexcept -> ChainResult {
	return flatten(ex::wait<BoundaryError>(ProbeSender{Channel::Error, code}));
}

[[nodiscard]] auto stopped_chain() noexcept -> ChainResult {
	return flatten(ex::wait<BoundaryError>(ProbeSender{Channel::Stopped, 0}));
}

[[nodiscard]] auto pool_when_all_sum(std::int32_t a, std::int32_t b, std::int32_t c) noexcept -> ChainResult {
	ex::static_thread_pool pool(4);
	auto sched = pool.get_scheduler();
	auto joined = ex::when_all(ex::starts_on(sched, ex::just(a) | ex::then([](std::int32_t v) {
		                                                return v + v;
	                                                })),
	                           ex::schedule(sched) | ex::then([b] {
		                           return b + b;
	                           }),
	                           ex::on(sched, ex::just(c) | ex::then([](std::int32_t v) {
		                                         return v + v;
	                                         }))) |
	              ex::continues_on(sched) | ex::then([](std::int32_t x, std::int32_t y, std::int32_t z) {
		              return x + y + z;
	              });
	return flatten(ex::wait<BoundaryError>(std::move(joined)));
}

[[nodiscard]] auto variant_roundtrip(std::int32_t value) noexcept -> ChainResult {
	auto chain = ex::just(value) | ex::into_variant() | ex::then([](std::variant<std::tuple<std::int32_t>> v) {
		             return std::get<0>(std::get<0>(v));
	             });
	return flatten(ex::wait<BoundaryError>(std::move(chain)));
}

[[nodiscard]] auto stopped_as_optional_chain(bool stop, std::int32_t value) noexcept -> ChainResult {
	auto chain = ProbeSender{stop ? Channel::Stopped : Channel::Value, value} | ex::stopped_as_optional() |
	             ex::then([](std::optional<std::int32_t> v) {
		             return v ? *v : std::int32_t{-1};
	             });
	return flatten(ex::wait<BoundaryError>(std::move(chain)));
}

}
