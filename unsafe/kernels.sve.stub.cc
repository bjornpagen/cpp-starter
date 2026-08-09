// Selected by the build graph unless STARTER_SVE=ON.
#include <array>
#include <cstddef>

namespace starter::unsafe_kernels {

auto sve_kernel_available() noexcept -> bool {
	return false;
}

auto sve_kernel(std::array<float*, 3> const& positions,
	std::array<float const*, 3> const& velocities, std::size_t count,
	float dt) noexcept -> void
{
	static_cast<void>(positions);
	static_cast<void>(velocities);
	static_cast<void>(count);
	static_cast<void>(dt);
	contract_assert(false); // pre: sve_kernel_available()
}

} // namespace starter::unsafe_kernels
