import std;
import starter;

namespace {

struct CaseResult {
	std::string_view name;
	bool passed;
};

// A count that is neither the capacity nor a multiple of any SIMD width in
// play, so every kernel's tail path is exercised.
constexpr auto test_count = std::size_t{1003};

constexpr auto dt = 0.5f;

auto fill(starter::ParticleSoa& soa) -> void {
	auto state = std::uint64_t{0x243f6a8885a308d3};
	auto const view = starter::field_spans(soa, starter::soa_capacity);
	for (auto const axes : {view.position, view.velocity}) {
		for (auto const axis : axes) {
			for (auto& value : axis) {
				state = state * std::uint64_t{6364136223846793005}
					+ std::uint64_t{1442695040888963407};
				auto const mantissa = static_cast<std::uint32_t>(state >> 40);
				value = static_cast<float>(mantissa)
					/ static_cast<float>(std::uint32_t{1} << 24) - 0.5f;
			}
		}
	}
}

auto check_soa_derives_particle_layout() -> CaseResult {
	return CaseResult{
		.name = "SoA storage derives one array per motion axis",
		.passed = starter::axis_count == 3
			&& sizeof(starter::ParticleSoa)
				== sizeof(float) * 2 * starter::axis_count * starter::soa_capacity,
	};
}

auto check_view_follows_declaration_order() -> CaseResult {
	auto soa = starter::ParticleSoa{};
	auto const view = starter::field_spans(soa, starter::soa_capacity);
	view.position[0][3] = 1.0f;  // x
	view.position[2][7] = 2.0f;  // z
	view.velocity[0][11] = 3.0f; // x
	view.velocity[2][13] = 4.0f; // z
	return CaseResult{
		.name = "the kinematic view exposes axes in declaration order",
		.passed = soa.position.x[3] == 1.0f && soa.position.z[7] == 2.0f
			&& soa.velocity.x[11] == 3.0f && soa.velocity.z[13] == 4.0f
			&& view.position[0].size() == starter::soa_capacity
			&& starter::field_spans(soa, test_count).velocity[2].size() == test_count,
	};
}

template<class Kernel>
auto agrees_with_scalar_reference(Kernel kernel, std::size_t count) -> bool {
	auto reference = starter::ParticleSoa{};
	fill(reference);
	auto const reference_view = starter::field_spans(reference, count);
	for (auto axis = std::size_t{0}; axis < starter::axis_count; ++axis) {
		for (auto i = std::size_t{0}; i < count; ++i) {
			reference_view.position[axis][i] += reference_view.velocity[axis][i] * dt;
		}
	}

	auto subject = starter::ParticleSoa{};
	fill(subject);
	auto const subject_view = starter::field_spans(subject, count);
	kernel(subject_view, dt);

	// FMA contraction may differ per kernel; allow a few ulps around values
	// of magnitude <= 0.75.
	constexpr auto tolerance = 1.0e-5f;
	for (auto axis = std::size_t{0}; axis < starter::axis_count; ++axis) {
		for (auto i = std::size_t{0}; i < count; ++i) {
			auto const position_difference
				= subject_view.position[axis][i] - reference_view.position[axis][i];
			auto const velocity_difference
				= subject_view.velocity[axis][i] - reference_view.velocity[axis][i];
			if (!(std::abs(position_difference) <= tolerance)
					|| velocity_difference != 0.0f) {
				return false;
			}
		}
	}
	return true;
}

// Both the partial-count path (scalar/SIMD tails, non-adjacent slab
// fallback) and the full-capacity path (dense-slab fast path) must agree.
template<class Kernel>
auto kernel_matches(Kernel kernel) -> bool {
	return agrees_with_scalar_reference(kernel, test_count)
		&& agrees_with_scalar_reference(kernel, starter::soa_capacity);
}

auto check_kernels_agree() -> CaseResult {
	auto passed = kernel_matches(
		[](starter::KinematicView const& view, float step) {
			starter::integrate_loop(view, step);
		});
	passed = passed && kernel_matches(
		[](starter::KinematicView const& view, float step) {
			starter::integrate_stdsimd(view, step);
		});
	if (starter::neon_available()) {
		passed = passed && kernel_matches(
			[](starter::KinematicView const& view, float step) {
				starter::integrate_neon(view, step);
			});
		passed = passed && kernel_matches(
			[](starter::KinematicView const& view, float step) {
				starter::integrate_neon_slab(view, step);
			});
	}
	if (starter::sve_available()) {
		passed = passed && kernel_matches(
			[](starter::KinematicView const& view, float step) {
				starter::integrate_sve(view, step);
			});
	}
	return CaseResult{
		.name = "every available kernel matches the scalar reference",
		.passed = passed,
	};
}

} // namespace

auto main() -> int {
	auto const results = std::array{
		check_soa_derives_particle_layout(),
		check_view_follows_declaration_order(),
		check_kernels_agree(),
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
