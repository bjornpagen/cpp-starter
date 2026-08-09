// Explicit-SIMD integration kernels over the reflection-derived kinematic
// view. The exported interface is dialect-clean; the intrinsic bodies live
// in plain translation units beside this file, selected per target by the
// build graph (AGENTS.md §3.1). A kernel whose implementation the build
// selected out reports unavailable, and calling its integrate function is a
// contract violation.
//
// This partition is the adapter between that interface and the intrinsic
// translation units. The pinned GCC cannot mix `import std` with textual SDK
// includes in one TU on macOS (conflicting imported declarations), so the
// intrinsic bodies live in module-free TUs and are reached through a narrow
// ABI: the extern "C++" block attaches the declarations to the global
// module, matching the plain definitions.
export module starter:simd;

import std;
import :particles;

extern "C++" {
namespace starter::unsafe_kernels {

auto stdsimd_kernel(std::array<float*, 3> const& positions,
	std::array<float const*, 3> const& velocities, std::size_t count,
	float dt) noexcept -> void;

auto neon_kernel_available() noexcept -> bool;

auto neon_kernel(std::array<float*, 3> const& positions,
	std::array<float const*, 3> const& velocities, std::size_t count,
	float dt) noexcept -> void;

auto neon_slab_kernel(std::array<float*, 3> const& positions,
	std::array<float const*, 3> const& velocities, std::size_t count,
	float dt) noexcept -> void;

auto sve_kernel_available() noexcept -> bool;

auto sve_kernel(std::array<float*, 3> const& positions,
	std::array<float const*, 3> const& velocities, std::size_t count,
	float dt) noexcept -> void;

} // namespace starter::unsafe_kernels
}

namespace starter {

static_assert(axis_count == 3,
	"the intrinsic kernels are written for 3 axes; update them deliberately");

namespace {

auto position_pointers(KinematicView const& view) -> std::array<float*, 3> {
	return {view.position[0].data(), view.position[1].data(),
		view.position[2].data()};
}

auto velocity_pointers(KinematicView const& view)
	-> std::array<float const*, 3>
{
	return {view.velocity[0].data(), view.velocity[1].data(),
		view.velocity[2].data()};
}

} // namespace

// KERNEL 2: std::experimental::simd, fused across axes. Portable — always
// available.
export auto integrate_stdsimd(KinematicView const& view, float dt) -> void {
	unsafe_kernels::stdsimd_kernel(position_pointers(view),
		velocity_pointers(view), view.position[0].size(), dt);
}

// KERNEL 3: raw NEON intrinsics (aarch64 targets only).
export auto neon_available() -> bool {
	return unsafe_kernels::neon_kernel_available();
}

// pre: neon_available()
export auto integrate_neon(KinematicView const& view, float dt) -> void {
	contract_assert(neon_available());
	unsafe_kernels::neon_kernel(position_pointers(view),
		velocity_pointers(view), view.position[0].size(), dt);
}

// KERNEL 5: raw NEON, x4-unrolled, over the dense SoA slab — when the view
// covers the full capacity, each half's axis arrays are adjacent (a
// compile-time law of the derived storage) and all axes stream through one
// fused pass; otherwise it falls back to x4-unrolled per-axis passes.
// pre: neon_available()
export auto integrate_neon_slab(KinematicView const& view, float dt) -> void {
	contract_assert(neon_available());
	unsafe_kernels::neon_slab_kernel(position_pointers(view),
		velocity_pointers(view), view.position[0].size(), dt);
}

// KERNEL 4: raw SVE intrinsics, fully predicated, vector-length-agnostic,
// no scalar tail (aarch64 targets built with STARTER_SVE=ON only).
export auto sve_available() -> bool {
	return unsafe_kernels::sve_kernel_available();
}

// pre: sve_available()
export auto integrate_sve(KinematicView const& view, float dt) -> void {
	contract_assert(sve_available());
	unsafe_kernels::sve_kernel(position_pointers(view),
		velocity_pointers(view), view.position[0].size(), dt);
}

} // namespace starter
