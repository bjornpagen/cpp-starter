// KERNEL 4: raw SVE intrinsics — vector-length agnostic; predication
// handles the tail, so there is no scalar epilogue. Module-free TU: see
// simd.cc for why the intrinsic bodies cannot import modules.
// Selected by the build graph only when STARTER_SVE=ON: compiling this unit
// requires an SVE-enabled -march, and calling the kernel requires hardware
// that executes SVE (no Apple silicon does).
#include <arm_sve.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace starter::unsafe_kernels {

namespace {

constexpr std::size_t axis_count = 3;

// Compile-time axis expansion: don't merely hope the optimizer unrolls a
// runtime axis loop.
template<class F, std::size_t... I>
constexpr void for_each_axis_impl(F&& f, std::index_sequence<I...>) {
	(f(std::integral_constant<std::size_t, I>{}), ...);
}

template<class F>
constexpr void for_each_axis(F&& f) {
	for_each_axis_impl(std::forward<F>(f), std::make_index_sequence<axis_count>{});
}

} // namespace

auto sve_kernel_available() noexcept -> bool {
	return true;
}

auto sve_kernel(std::array<float*, 3> const& positions, std::array<float const*, 3> const& velocities, std::size_t count, float dt) noexcept
    -> void {
	svfloat32_t const vdt = svdup_f32(dt);
	std::size_t i = 0;
	while (i < count) {
		svbool_t const pg = svwhilelt_b32(std::uint64_t{i}, std::uint64_t{count});
		for_each_axis([&]<std::size_t K>(std::integral_constant<std::size_t, K>) {
			svfloat32_t const pos = svld1_f32(pg, positions[K] + i);
			svfloat32_t const vel = svld1_f32(pg, velocities[K] + i);
			svst1_f32(pg, positions[K] + i, svmla_f32_x(pg, pos, vel, vdt));
		});
		i += svcntw();
	}
}

} // namespace starter::unsafe_kernels
