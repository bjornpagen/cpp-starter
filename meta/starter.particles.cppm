export module starter.particles;

import std;

namespace starter {

// ============================================================
// Single source of semantic truth.
//
// position and velocity have literally the same shape; adding
// or removing an axis changes both together.
// ============================================================

export struct Particle {
	struct Vector {
		float x;
		float y;
		float z;
	};

	Vector position;
	Vector velocity;
};

export constexpr auto soa_capacity = std::size_t{1024};

// ============================================================
// Reflection utilities.
// ============================================================

template<class T>
consteval auto member_count() -> std::size_t {
	return std::meta::nonstatic_data_members_of(
		^^T, std::meta::access_context::current()).size();
}

template<class T>
consteval auto all_members_are_float() -> bool {
	for (auto member : std::meta::nonstatic_data_members_of(
			 ^^T, std::meta::access_context::current())) {
		if (std::meta::type_of(member) != ^^float) {
			return false;
		}
	}
	return true;
}

// ============================================================
// Integrator contract.
//
// These assertions are deliberately strict: if somebody changes
// Particle's semantic structure, nothing silently adapts — this
// subsystem must be updated consciously.
// ============================================================

export using MotionVector = decltype(Particle::position);

static_assert(
	std::same_as<decltype(Particle::position), decltype(Particle::velocity)>,
	"position and velocity must have identical structural types");

static_assert(
	member_count<Particle>() == 2,
	"Particle's integration contract changed; "
	"update the integrator deliberately");

static_assert(
	all_members_are_float<MotionVector>(),
	"the SIMD kernels require float vector components");

export constexpr auto axis_count = member_count<MotionVector>();

static_assert(axis_count > 0);

// ============================================================
// Reflection-derived SoA.
//
//     struct Vector { float x; float y; float z; };
//
// becomes
//
//     struct type { array<float, N> x; ... array<float, N> z; };
//
// via C++26 define_aggregate/data_member_spec.
// ============================================================

template<class T, std::size_t N>
struct SoaStorage {
	struct type;

	consteval {
		std::vector<std::meta::info> specs;
		auto ctx = std::meta::access_context::current();
		for (auto member : std::meta::nonstatic_data_members_of(^^T, ctx)) {
			auto array_type = std::meta::substitute(
				^^std::array,
				{std::meta::type_of(member), std::meta::reflect_constant(N)});
			specs.push_back(std::meta::data_member_spec(
				array_type, {.name = std::meta::identifier_of(member)}));
		}
		std::meta::define_aggregate(^^type, specs);
	}
};

template<class T, std::size_t N>
using soa_t = typename SoaStorage<T, N>::type;

// The outer structure is NOT inferred from field ordering: position and
// velocity are semantic concepts used by the integrator, so they are spelled
// explicitly. Their internal structure is derived automatically.
export template<class P, std::size_t N>
struct KinematicSoa {
	static_assert(member_count<P>() == 2, "kinematic particle contract changed");

	using position_type = std::remove_cvref_t<decltype(std::declval<P&>().position)>;
	using velocity_type = std::remove_cvref_t<decltype(std::declval<P&>().velocity)>;

	static_assert(
		std::same_as<position_type, velocity_type>,
		"position and velocity shapes diverged");

	using vector_storage = soa_t<position_type, N>;

	vector_storage position;
	vector_storage velocity;
};

export using ParticleSoa = KinematicSoa<Particle, soa_capacity>;

// ============================================================
// Reflection-derived access.
//
// Reflect the generated storage itself — not Particle::Vector,
// whose members belong to a different class — so the splice
// below is valid. Dialect code borrows through spans; the raw
// pointers behind them appear only inside unsafe/ kernels.
// ============================================================

export using AxisSpans = std::array<std::span<float>, axis_count>;

// Ephemeral view product (AGENTS.md §12): one span per generated axis array,
// in declaration order, sized to the live particle count.
export struct KinematicView {
	AxisSpans position;
	AxisSpans velocity;
};

template<class Storage>
auto storage_spans(Storage& storage, std::size_t count) -> AxisSpans {
	static_assert(member_count<Storage>() == axis_count);
	auto spans = AxisSpans{};
	auto index = std::size_t{0};
	template for (
		constexpr auto member :
		std::define_static_array(std::meta::nonstatic_data_members_of(
			^^Storage, std::meta::access_context::current()))
	) {
		spans[index] = std::span{storage.[:member:]}.first(count);
		++index;
	}
	return spans;
}

export auto field_spans(ParticleSoa& soa, std::size_t count) -> KinematicView {
	contract_assert(count <= soa_capacity);
	return KinematicView{
		.position = storage_spans(soa.position, count),
		.velocity = storage_spans(soa.velocity, count),
	};
}

// ============================================================
// Compile-time axis iteration.
//
// Don't merely hope the optimizer unrolls a runtime axis loop:
// expand the axis operations at compile time.
// ============================================================

template<class F, std::size_t... I>
constexpr auto for_each_axis_impl(F&& f, std::index_sequence<I...>) -> void {
	(f(std::integral_constant<std::size_t, I>{}), ...);
}

export template<class F>
constexpr auto for_each_axis(F&& f) -> void {
	for_each_axis_impl(std::forward<F>(f), std::make_index_sequence<axis_count>{});
}

} // namespace starter
