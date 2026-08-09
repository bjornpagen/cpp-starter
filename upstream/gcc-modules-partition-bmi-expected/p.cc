module;
#include <expected>
export module M:db;

export struct Db {
	auto admit(std::expected<int, int> opened) -> std::expected<int, int>;
};

auto Db::admit(std::expected<int, int> opened) -> std::expected<int, int> {
	return std::move(opened).transform([](int handle) { return handle + 1; });
}
