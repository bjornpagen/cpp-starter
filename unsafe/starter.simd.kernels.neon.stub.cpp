// Selected by the build graph on targets without NEON.
#include <array>
#include <cstddef>

namespace starter::unsafe_kernels {

auto neon_kernel_available() noexcept -> bool {
	return false;
}

auto neon_kernel(std::array<float*, 3> const& positions,
	std::array<float const*, 3> const& velocities, std::size_t count,
	float dt) noexcept -> void
{
	static_cast<void>(positions);
	static_cast<void>(velocities);
	static_cast<void>(count);
	static_cast<void>(dt);
	contract_assert(false); // pre: neon_kernel_available()
}

} // namespace starter::unsafe_kernels
