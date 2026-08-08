// starter.conformance.test.cpp — the executable toolchain spec.
//
// This TU pins what the production toolchain (GCC 16.1.0, -std=c++26
// -freflection, libstdc++) actually delivers. Conformance failures are
// preferred as compile failures: everything that can be checked in
// static_assert/consteval is. The reflection block below makes this TU
// GCC-only; it is excluded from the Clang lint graph in CMakeLists.txt.
//
// Feature-test macros are preprocessor state, which `import std;` does not
// (and cannot) provide; <version> is the macro-only header for exactly this
// purpose.
#include <version>

import std;

// ---------------------------------------------------------------------------
// Compile-time gate: feature-test macros the probes verified.
// ---------------------------------------------------------------------------

static_assert(__cplusplus >= 202400L, "C++26 mode required");
static_assert(__cpp_lib_inplace_vector >= 202603L, "std::inplace_vector");
static_assert(__cpp_lib_indirect >= 202502L, "std::indirect (<memory>)");
static_assert(__cpp_lib_polymorphic >= 202502L, "std::polymorphic (<memory>)");
static_assert(__cpp_lib_function_ref >= 202603L, "std::function_ref");
static_assert(__cpp_lib_reflection >= 202603L, "P2996 reflection library");
// Contracts: the macro is compiler-defined under plain -std=c++26. Note the
// asymmetry: pre/post/contract_assert *compile* with no extra flag, but
// *linking* a violation handler needs -lstdc++exp — it is a linker flag.
static_assert(__cpp_contracts >= 202502L, "P2900 contracts");

// SURPRISE, pinned deliberately: __cpp_lib_optional_ref is UNDEFINED on this
// toolchain even though std::optional<int&> is fully implemented (exercised
// below, including write-through). When the macro appears, flip this on:
// static_assert(__cpp_lib_optional_ref >= 202602L, "std::optional<T&>");
#ifdef __cpp_lib_optional_ref
static_assert(false,
    "__cpp_lib_optional_ref appeared: the toolchain caught up, "
    "promote the commented static_assert above and delete this trap");
#endif

// ---------------------------------------------------------------------------
// Tombstones: capabilities the probes verified as ABSENT. Each names the
// feature-test macro to flip when the toolchain catches up.
// ---------------------------------------------------------------------------

// TOMBSTONE(std::execution, P2300 senders/receivers): __cpp_lib_senders is
// UNDEFINED; <execution> compiles but holds only the classic parallel
// policies — std::execution::just/then do not exist. When the macro appears,
// enable and exercise a just | then | sync_wait pipeline for real:
// static_assert(__cpp_lib_senders >= 202406L, "std::execution (P2300)");

// ---------------------------------------------------------------------------
// std::optional<int&>: rebinding-free reference semantics, write-through.
// ---------------------------------------------------------------------------

consteval auto optional_ref_writes_through() -> bool {
    int slot = 1;
    std::optional<int&> ref{slot};
    ref.value() = 5; // .value() -> int&, writes through to the referent
    return slot == 5
        && &ref.value() == &slot
        && !std::optional<int&>{}.has_value();
}
static_assert(optional_ref_writes_through());

// ---------------------------------------------------------------------------
// std::inplace_vector: bounded push, capacity is a compile-time fact.
// ---------------------------------------------------------------------------

consteval auto inplace_vector_bounded_push() -> bool {
    std::inplace_vector<int, 3> v;
    v.push_back(1);
    v.push_back(2);
    v.push_back(3);
    // Full: try_push_back reports the bound instead of allocating. Pinned
    // toolchain quirk: GCC 16.1 returns std::optional<int&> here, not the
    // standard's T* — hence the nullopt comparison instead of nullptr.
    return v.size() == 3
        && v.try_push_back(4) == std::nullopt
        && v[0] == 1 && v[2] == 3
        && decltype(v)::capacity() == 3;
}
static_assert(inplace_vector_bounded_push());

// ---------------------------------------------------------------------------
// std::expected: monadic composition (and_then / transform / or_else).
// ---------------------------------------------------------------------------

consteval auto expected_monadic_chain() -> bool {
    using E = std::expected<int, char>;
    auto const good = E{20}
        .and_then([](int v) -> E { return E{v + 1}; })
        .transform([](int v) { return v * 2; })
        .or_else([](char) -> E { return E{0}; });
    auto const bad = E{std::unexpect, 'e'}
        .transform([](int v) { return v + 1; }) // skipped on the error path
        .or_else([](char c) -> E {
            return E{std::unexpect, static_cast<char>(c + 1)};
        });
    return good == E{42} && bad == std::unexpected('f');
}
static_assert(expected_monadic_chain());

// ---------------------------------------------------------------------------
// Reflection: expansion-statement static_assert. This block is what makes
// the TU GCC-only.
// ---------------------------------------------------------------------------

namespace {

enum class Compass { North, East, South, West };

consteval auto named_enumerator_count() -> std::size_t {
    auto count = std::size_t{0};
    template for (
        constexpr auto enumerator :
        std::define_static_array(std::meta::enumerators_of(^^Compass))
    ) {
        if (!std::meta::identifier_of(enumerator).empty()) {
            ++count;
        }
    }
    return count;
}

consteval auto first_enumerator_is_north() -> bool {
    template for (
        constexpr auto enumerator :
        std::define_static_array(std::meta::enumerators_of(^^Compass))
    ) {
        return std::meta::identifier_of(enumerator) == "North";
    }
    return false;
}

} // namespace

static_assert(named_enumerator_count() == 4);
static_assert(first_enumerator_is_north());

// ---------------------------------------------------------------------------
// Runtime spot-checks for the parts the library does not make consteval:
// std::function_ref invocation and std::indirect value semantics.
// ---------------------------------------------------------------------------

namespace {

struct CaseResult {
    std::string_view name;
    bool passed;
};

auto check_function_ref_binds_lambda() -> CaseResult {
    int captured = 3;
    auto const add_captured = [&captured](int x) { return captured + x; };
    std::function_ref<int(int)> const ref{add_captured};
    captured = 30; // function_ref refers, it does not copy the callable
    return CaseResult{
        .name = "function_ref binds a capturing lambda by reference",
        .passed = ref(4) == 34,
    };
}

auto check_indirect_value_semantics() -> CaseResult {
    std::indirect<int> const original{11};
    auto copy = original; // deep copy: distinct object, equal value
    *copy += 1;
    auto const moved = std::move(copy); // source becomes valueless, not null-UB
    return CaseResult{
        .name = "indirect construction, deep copy, and move are value-shaped",
        .passed = *original == 11 && *moved == 12
            && copy.valueless_after_move(),
    };
}

} // namespace

auto main() -> int {
    auto const results = std::array{
        check_function_ref_binds_lambda(),
        check_indirect_value_semantics(),
    };

    auto failures = std::size_t{0};
    for (auto const& result : results) {
        if (result.passed) {
            std::println("pass: {}", result.name);
        } else {
            std::println("FAIL: {}", result.name);
            ++failures;
        }
    }

    return failures == 0 ? 0 : 1;
}
