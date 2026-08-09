export module starter:integrate;

import std;
import :particles;

namespace starter {

// KERNEL 1: plain span loop — SoA keeps it friendly to the compiler's
// autovectorizer. The explicit SIMD competitors live in unsafe/ (the :simd
// partition).
export auto integrate_loop(KinematicView const& view, float dt) -> void {
	auto const count = view.position[0].size();
	for_each_axis([&]<std::size_t Axis>(std::integral_constant<std::size_t, Axis>) {
		auto const pos = view.position[Axis];
		auto const vel = view.velocity[Axis];
		for (auto i = std::size_t{0}; i < count; ++i) {
			pos[i] += vel[i] * dt;
		}
	});
}

} // namespace starter
