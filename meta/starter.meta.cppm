export module starter.meta;

import std;

export namespace starter::meta {

// Reflection-derived enumerator name: the enum declaration is the single
// source of truth (AGENTS.md §17); no parallel string table exists.
template<class E>
    requires std::is_enum_v<E>
auto enum_name(E value) -> std::optional<std::string_view> {
    template for (
        constexpr auto enumerator :
        std::define_static_array(std::meta::enumerators_of(^^E))
    ) {
        if (value == [:enumerator:]) {
            return std::meta::identifier_of(enumerator);
        }
    }
    return std::nullopt;
}

} // namespace starter::meta
