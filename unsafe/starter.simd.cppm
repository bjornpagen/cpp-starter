// Explicit-SIMD integration kernels over the reflection-derived kinematic
// view. The interface is dialect-clean; the intrinsic bodies live in
// implementation units below, selected per target by the build graph
// (AGENTS.md §3.1). A kernel whose implementation the build selected out
// reports unavailable, and calling its integrate function is a contract
// violation.
export module starter.simd;

import std;
import starter.particles;

namespace starter {

// KERNEL 2: std::experimental::simd, fused across axes. Portable — always
// available.
export auto integrate_stdsimd(KinematicView const& view, float dt) -> void;

// KERNEL 3: raw NEON intrinsics (aarch64 targets only).
export auto neon_available() -> bool;

// pre: neon_available()
export auto integrate_neon(KinematicView const& view, float dt) -> void;

// KERNEL 5: raw NEON, x4-unrolled, over the dense SoA slab — when the view
// covers the full capacity, each half's axis arrays are adjacent (a
// compile-time law of the derived storage) and all axes stream through one
// fused pass; otherwise it falls back to x4-unrolled per-axis passes.
// pre: neon_available()
export auto integrate_neon_slab(KinematicView const& view, float dt) -> void;

// KERNEL 4: raw SVE intrinsics, fully predicated, vector-length-agnostic,
// no scalar tail (aarch64 targets built with STARTER_SVE=ON only).
export auto sve_available() -> bool;

// pre: sve_available()
export auto integrate_sve(KinematicView const& view, float dt) -> void;

} // namespace starter
