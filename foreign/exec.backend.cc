// exec.backend.cc — the combinator half of the sender/receiver swap
// boundary (the I/O half is unsafe/net.backend.cc; together they are the
// entire stdexec spelling surface of the repository, AGENTS.md §15). When
// the pinned toolchain defines __cpp_lib_senders (see the tombstone in
// tests/conformance.test.cc), the eventual swap edits only these two files
// (and deletes the FetchContent pin): `namespace ex` below re-binds from
// stdexec:: to std::execution:: and everything upward is untouched. Direct
// stdexec/header use anywhere else in the repository is forbidden.
//
// Why this is a plain (non-module) TU and not the :exec partition itself:
// GCC 16.1 ICEs (cc1plus segfault) whenever stdexec/execution.hpp is
// textually included in ANY module unit — interface partition, primary, or
// implementation unit, with or without `import std`. Root cause is the
// `extern T const x;` forward-declaration followed by the
// `inline constexpr T x{};` definition in the global module fragment; the
// stdexec CPO headers contain 54+ such load-bearing pairs, so it is not
// patchable. Consequence: sender composition cannot cross the module
// boundary on this toolchain — only concrete function surfaces do. The
// :exec partition (foreign/exec.cc) reaches these definitions through the
// same extern "C++" narrow ABI the :net partition uses for its io-context.
// Re-verify with the pinned micro-repro on every toolchain bump.
//
// `namespace ex` re-exports EXACTLY the probe-verified subset of the
// vocabulary, plus OUR expected-erroring wait. stdexec::sync_wait is
// deliberately absent. The pinned fork already fixes its worst behavior
// (upstream sync_wait silently misreported typed errors as stopped under
// -fno-exceptions; the fork terminates instead — submitted upstream as
// github.com/NVIDIA/stdexec/pull/2168). Terminate-on-error is safe but
// still not this dialect's error algebra: errors are values (AGENTS.md
// §11), and ours::wait returns them as std::expected, which sync_wait's
// specified optional<tuple> shape cannot.
#include <cstdint>
#include <exception>
#include <expected>
#include <optional>
#include <tuple>
#include <utility>
#include <variant>

#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

// The vocabulary namespace: exactly the allowed subset (probe-verified on
// the pinned GCC 16.1 / -fno-exceptions toolchain), nothing else escapes.
// The eventual swap re-points these using-declarations at std::execution.
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

// The scheduler provider the probes verified `schedule` against. Lives in
// stdexec's exec:: companion namespace today; std::execution provides its
// own pool type to re-bind to at swap time.
using exec::static_thread_pool;

// OUR synchronous wait — dialect surface, ported from the verified probe
// prototype. wait<E>(sender) drives the sender on a stdexec::run_loop and
// returns std::optional<std::expected<value-tuple, E>>:
//
//   set_value(vs...)  -> engaged optional, expected holds tuple{vs...}
//   set_error(E e)    -> engaged optional, unexpected(e) — payload preserved
//   set_stopped()     -> std::nullopt
//
// The receiver's environment mirrors stdexec::sync_wait's: it answers the
// get_scheduler, get_start_scheduler, and get_delegation_scheduler queries
// with the run_loop's scheduler, so combinators that must know or return to
// "the caller's context" (e.g. `on`, whose completion signatures are not
// even computable without it) complete back on this loop. Signature
// computation below must use this same environment, not env<>.
namespace ours {

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

	// Some senders advertise set_error_t(exception_ptr) even though
	// -fno-exceptions makes a non-null exception_ptr unrepresentable
	// (see the sync_wait note above). Reaching this completion is
	// therefore an unrecoverable process failure, not a recoverable
	// error: terminate, per the error algebra (AGENTS.md §11).
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

} // namespace ours

using ours::wait;

} // namespace ex

namespace starter::exec_backend {

namespace {

// The typed error the conformance chains route through the error
// channel; only its payload crosses the narrow ABI.
struct BoundaryError {
	std::int32_t code;
};

using ChainResult = std::optional<std::expected<std::int32_t, std::int32_t>>;

// probe-verified caveat, pinned as compile coverage: just_stopped() is a
// well-formed sender, but any wait site statically requires at least one
// set_value signature, so it can only appear where a value channel
// survives (e.g. never as the sole when_all child ahead of a wait).
static_assert(stdexec::sender<decltype(ex::just_stopped())>);

// Conformance sender: the minimal sender with all three completion
// channels declared, choosing one at runtime. wait sites need the value
// signature to exist even when the error/stopped path is taken (bare
// just_error/just_stopped have no value signature and are statically
// rejected there — same probe caveat as above).
enum class Channel : std::uint8_t {
	Value,
	Error,
	Stopped,
};

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

} // namespace

[[nodiscard]] auto value_chain(std::int32_t seed) noexcept -> ChainResult {
	// just | let_value | then through the value channel: 2*seed + 1.
	auto chain = ex::just(seed) | ex::let_value([](std::int32_t v) {
		             return ex::just(v + v);
	             }) |
	             ex::then([](std::int32_t v) {
		             return v + 1;
	             });
	return flatten(ex::wait<BoundaryError>(std::move(chain)));
}

[[nodiscard]] auto error_recovery_chain(std::int32_t code) noexcept -> ChainResult {
	// just_error | upon_error: typed error -> value, payload preserved.
	auto chain = ex::just_error(BoundaryError{code}) | ex::upon_error([](BoundaryError e) {
		             return e.code + 1;
	             });
	return flatten(ex::wait<BoundaryError>(std::move(chain)));
}

[[nodiscard]] auto error_reroute_chain(std::int32_t code) noexcept -> ChainResult {
	// just_error | let_error: typed error -> new sender, payload preserved.
	auto chain = ex::just_error(BoundaryError{code}) | ex::let_error([](BoundaryError e) {
		             return ex::just(e.code + e.code);
	             });
	return flatten(ex::wait<BoundaryError>(std::move(chain)));
}

[[nodiscard]] auto error_passthrough_chain(std::int32_t code) noexcept -> ChainResult {
	// Typed error through OUR wait's error channel: unexpected(code).
	return flatten(ex::wait<BoundaryError>(ProbeSender{Channel::Error, code}));
}

[[nodiscard]] auto stopped_chain() noexcept -> ChainResult {
	// Stopped channel through OUR wait: nullopt.
	return flatten(ex::wait<BoundaryError>(ProbeSender{Channel::Stopped, 0}));
}

[[nodiscard]] auto pool_when_all_sum(std::int32_t a, std::int32_t b, std::int32_t c) noexcept -> ChainResult {
	// when_all join of three pool tasks reached three verified ways
	// (starts_on, schedule|then, on), rejoined via continues_on: 2(a+b+c).
	// `on` returns to the caller's context, which is wait's run_loop
	// scheduler exposed through the receiver environment.
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
	// into_variant: value completions reified as variant<tuple<...>>.
	auto chain = ex::just(value) | ex::into_variant() | ex::then([](std::variant<std::tuple<std::int32_t>> v) {
		             return std::get<0>(std::get<0>(v));
	             });
	return flatten(ex::wait<BoundaryError>(std::move(chain)));
}

[[nodiscard]] auto stopped_as_optional_chain(bool stop, std::int32_t value) noexcept -> ChainResult {
	// stopped_as_optional: stopped -> disengaged optional value (mapped to
	// -1 here purely so one scalar crosses the ABI), value -> engaged.
	// Probe caveat: the child must have exactly one value signature.
	auto chain = ProbeSender{stop ? Channel::Stopped : Channel::Value, value} | ex::stopped_as_optional() |
	             ex::then([](std::optional<std::int32_t> v) {
		             return v ? *v : std::int32_t{-1};
	             });
	return flatten(ex::wait<BoundaryError>(std::move(chain)));
}

} // namespace starter::exec_backend
