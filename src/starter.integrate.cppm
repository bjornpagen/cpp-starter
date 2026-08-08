export module starter.integrate;

import std;
import starter.particles;

namespace starter {

// KERNEL 1: plain span loop — SoA keeps it friendly to the compiler's
// autovectorizer. The explicit SIMD competitors live in unsafe/ (module
// starter.simd).
export auto integrate_loop(KinematicView const& view, float dt) -> void;

} // namespace starter
