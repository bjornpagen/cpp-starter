// Times the four integration kernels over the same reflection-derived SoA
// storage: plain span loop (autovectorized), std::experimental::simd, raw
// NEON, raw SVE. Kernel bodies live behind the module boundary, so every
// timed call is opaque to this TU and no kernel can be folded away. Build
// with the release preset; Debug numbers are meaningless.
import std;
import starter;

namespace {

constexpr auto warmup_calls = std::size_t{10'000};
constexpr auto timed_calls = std::size_t{100'000};
constexpr auto repetitions = std::size_t{5};
constexpr auto dt = 1.0e-6f;

// Deterministic, well-conditioned values in [-0.5, 0.5); no clock or seed
// dependency, so every kernel integrates the same initial state.
auto fill(starter::ParticleSoa& soa) -> void {
	auto state = std::uint64_t{0x9e3779b97f4a7c15};
	auto const view = starter::field_spans(soa, starter::soa_capacity);
	for (auto const axes : {view.position, view.velocity}) {
		for (auto const axis : axes) {
			for (auto& value : axis) {
				state = state * std::uint64_t{6364136223846793005} + std::uint64_t{1442695040888963407};
				auto const mantissa = static_cast<std::uint32_t>(state >> 40);
				value = static_cast<float>(mantissa) / static_cast<float>(std::uint32_t{1} << 24) - 0.5f;
			}
		}
	}
}

struct Measurement {
	std::string_view name;
	double ns_per_call;
};

template<class Kernel>
auto measure(std::string_view name, starter::ParticleSoa& soa, Kernel kernel) -> Measurement {
	fill(soa);
	auto const view = starter::field_spans(soa, starter::soa_capacity);
	for (auto call = std::size_t{0}; call < warmup_calls; ++call) {
		kernel(view, dt);
	}
	auto best = std::numeric_limits<double>::infinity();
	for (auto rep = std::size_t{0}; rep < repetitions; ++rep) {
		auto const start = std::chrono::steady_clock::now();
		for (auto call = std::size_t{0}; call < timed_calls; ++call) {
			kernel(view, dt);
		}
		auto const stop = std::chrono::steady_clock::now();
		auto const ns = std::chrono::duration<double, std::nano>{stop - start}.count();
		best = std::min(best, ns / static_cast<double>(timed_calls));
	}
	// Fold the integrated state into an observable value.
	auto checksum = 0.0;
	for (auto const axis : view.position) {
		for (auto const value : axis) {
			checksum += static_cast<double>(value);
		}
	}
	auto const elements = static_cast<double>(starter::soa_capacity * starter::axis_count);
	std::println("  {:<24} {:>8.1f} ns/step   {:>6.2f} Gelem/s   (checksum {:+.4f})", name, best, elements / best, checksum);
	return Measurement{.name = name, .ns_per_call = best};
}

} // namespace

auto main() -> int {
	std::println("particle integration: {} particles x {} axes, {} calls x {} repetitions", starter::soa_capacity, starter::axis_count,
	             timed_calls, repetitions);

	auto soa = starter::ParticleSoa{};
	auto results = std::inplace_vector<Measurement, 5>{};

	results.push_back(measure("loop (autovectorized)", soa, [](starter::KinematicView const& view, float step) {
		starter::integrate_loop(view, step);
	}));
	results.push_back(measure("std::experimental::simd", soa, [](starter::KinematicView const& view, float step) {
		starter::integrate_stdsimd(view, step);
	}));
	if (starter::neon_available()) {
		results.push_back(measure("neon intrinsics", soa, [](starter::KinematicView const& view, float step) {
			starter::integrate_neon(view, step);
		}));
		results.push_back(measure("neon x4 dense slab", soa, [](starter::KinematicView const& view, float step) {
			starter::integrate_neon_slab(view, step);
		}));
	} else {
		std::println("  {:<24} skipped: not available on this target", "neon intrinsics");
	}
	if (starter::sve_available()) {
		results.push_back(measure("sve intrinsics", soa, [](starter::KinematicView const& view, float step) {
			starter::integrate_sve(view, step);
		}));
	} else {
		std::println("  {:<24} skipped: not available on this target", "sve intrinsics");
	}

	auto const fastest = std::ranges::min_element(results, std::ranges::less{}, [](Measurement const& m) {
		return m.ns_per_call;
	});

	std::println("\nfastest: {}", fastest->name);
	for (auto const& result : results) {
		std::println("  {:<24} {:>5.2f}x", result.name, result.ns_per_call / fastest->ns_per_call);
	}
	return 0;
}
