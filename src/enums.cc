export module starter:enums;

import std;

namespace starter {

export template<class E>
    requires std::is_enum_v<E>
[[nodiscard]] auto enum_name(E value) -> std::optional<std::string_view> {
	template for (constexpr auto enumerator : std::define_static_array(std::meta::enumerators_of(^^E))) {
		if (value == [:enumerator:]) {
			return std::meta::identifier_of(enumerator);
		}
	}
	return std::nullopt;
}

}
