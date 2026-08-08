// Adapter between the dialect-clean module interface and the plain
// intrinsic translation units. The pinned GCC cannot mix `import std`
// with textual SDK includes in one TU on macOS (conflicting imported
// declarations), so the intrinsic bodies live in module-free TUs and are
// reached through this narrow ABI: the extern "C++" block attaches the
// declarations to the global module, matching the plain definitions.
module starter.simd;

import std;
import starter.particles;

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

auto integrate_stdsimd(KinematicView const& view, float dt) -> void {
	unsafe_kernels::stdsimd_kernel(position_pointers(view),
		velocity_pointers(view), view.position[0].size(), dt);
}

auto neon_available() -> bool {
	return unsafe_kernels::neon_kernel_available();
}

auto integrate_neon(KinematicView const& view, float dt) -> void {
	contract_assert(neon_available());
	unsafe_kernels::neon_kernel(position_pointers(view),
		velocity_pointers(view), view.position[0].size(), dt);
}

auto integrate_neon_slab(KinematicView const& view, float dt) -> void {
	contract_assert(neon_available());
	unsafe_kernels::neon_slab_kernel(position_pointers(view),
		velocity_pointers(view), view.position[0].size(), dt);
}

auto sve_available() -> bool {
	return unsafe_kernels::sve_kernel_available();
}

auto integrate_sve(KinematicView const& view, float dt) -> void {
	contract_assert(sve_available());
	unsafe_kernels::sve_kernel(position_pointers(view),
		velocity_pointers(view), view.position[0].size(), dt);
}

} // namespace starter
