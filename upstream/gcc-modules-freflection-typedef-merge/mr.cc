module;
#include <meta>
#include <string>
export module mr;
export struct Widget { int x; };
export template <typename T> consteval auto type_name() -> std::string_view {
	return std::meta::identifier_of(^^T);
}
export auto reflected() -> std::string {
	return std::string{type_name<Widget>()};
}
