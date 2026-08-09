// KERNEL 3: raw NEON intrinsics. Module-free TU: see simd.cc for why the
// intrinsic bodies cannot import modules. Selected by the build graph for
// aarch64 targets only.
#include <arm_neon.h>

#include <array>
#include <cstddef>
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

// One x4-unrolled fused-multiply-add pass over a contiguous run. 16 elements
// per iteration keeps four independent FMA chains in flight and lets the
// 64-byte structured loads/stores saturate the load/store ports.
void fma_run_x4(float* __restrict positions, float const* __restrict velocities, std::size_t total, float dt) noexcept {
	float32x4_t const vdt = vdupq_n_f32(dt);
	std::size_t i = 0;
	for (; i + 16 <= total; i += 16) {
		float32x4x4_t pos = vld1q_f32_x4(positions + i);
		float32x4x4_t const vel = vld1q_f32_x4(velocities + i);
		pos.val[0] = vfmaq_f32(pos.val[0], vel.val[0], vdt);
		pos.val[1] = vfmaq_f32(pos.val[1], vel.val[1], vdt);
		pos.val[2] = vfmaq_f32(pos.val[2], vel.val[2], vdt);
		pos.val[3] = vfmaq_f32(pos.val[3], vel.val[3], vdt);
		vst1q_f32_x4(positions + i, pos);
	}
	// scalar tail
	for (; i < total; ++i) {
		positions[i] += velocities[i] * dt;
	}
}

} // namespace

auto neon_kernel_available() noexcept -> bool {
	return true;
}

auto neon_kernel(std::array<float*, 3> const& positions, std::array<float const*, 3> const& velocities, std::size_t count,
                 float dt) noexcept -> void {
	float32x4_t const vdt = vdupq_n_f32(dt);
	std::size_t i = 0;
	for (; i + 4 <= count; i += 4) {
		for_each_axis([&]<std::size_t K>(std::integral_constant<std::size_t, K>) {
			float32x4_t const pos = vld1q_f32(positions[K] + i);
			float32x4_t const vel = vld1q_f32(velocities[K] + i);
			vst1q_f32(positions[K] + i, vfmaq_f32(pos, vel, vdt));
		});
	}
	// scalar tail
	for (; i < count; ++i) {
		for_each_axis([&]<std::size_t K>(std::integral_constant<std::size_t, K>) {
			positions[K][i] += velocities[K][i] * dt;
		});
	}
}

auto neon_slab_kernel(std::array<float*, 3> const& positions, std::array<float const*, 3> const& velocities, std::size_t count,
                      float dt) noexcept -> void {
	// Dense-slab fast path: the derived storage lays each half's axis arrays
	// end to end (a static_assert'd law of the :particles partition), so when the
	// view covers the full capacity the axis pointers are adjacent and all
	// axes stream through one fused pass. The cross-array pointer arithmetic
	// this implies is exactly what this quarantine exists for.
	bool const dense = positions[1] == positions[0] + count && positions[2] == positions[1] + count &&
	                   velocities[1] == velocities[0] + count && velocities[2] == velocities[1] + count;
	if (dense) {
		fma_run_x4(positions[0], velocities[0], axis_count * count, dt);
		return;
	}
	for_each_axis([&]<std::size_t K>(std::integral_constant<std::size_t, K>) {
		fma_run_x4(positions[K], velocities[K], count, dt);
	});
}

} // namespace starter::unsafe_kernels
