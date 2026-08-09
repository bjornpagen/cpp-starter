module;
#include <expected>
export module M:db;
import Q;

export struct Db {
	auto admit(std::expected<q::handle, int> opened) -> std::expected<int, int>;
};

auto Db::admit(std::expected<q::handle, int> opened) -> std::expected<int, int> {
	return std::move(opened).transform([](q::handle h) { return h.fd + 1; }).transform_error([](int e) { return -e; });
}
