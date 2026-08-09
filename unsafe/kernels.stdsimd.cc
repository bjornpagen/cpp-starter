// KERNEL 2: std::experimental::simd, fused across axes. Module-free TU:
// see simd.cc for why the intrinsic bodies cannot import modules.
#include <array>
#include <cstddef>
#include <experimental/simd>
#include <utility>

namespace stdx = std::experimental;

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
	for_each_axis_impl(std::forward<F>(f),
		std::make_index_sequence<axis_count>{});
}

} // namespace

auto stdsimd_kernel(std::array<float*, 3> const& positions,
	std::array<float const*, 3> const& velocities, std::size_t count,
	float dt) noexcept -> void
{
	using vecf = stdx::native_simd<float>;
	vecf const vdt = dt;
	constexpr std::size_t width = vecf::size();
	std::size_t i = 0;
	for (; i + width <= count; i += width) {
		for_each_axis([&]<std::size_t K>(std::integral_constant<std::size_t, K>) {
			vecf pos;
			vecf vel;
			pos.copy_from(positions[K] + i, stdx::element_aligned);
			vel.copy_from(velocities[K] + i, stdx::element_aligned);
			pos += vel * vdt;
			pos.copy_to(positions[K] + i, stdx::element_aligned);
		});
	}
	// scalar tail
	for (; i < count; ++i) {
		for_each_axis([&]<std::size_t K>(std::integral_constant<std::size_t, K>) {
			positions[K][i] += velocities[K][i] * dt;
		});
	}
}

} // namespace starter::unsafe_kernels
