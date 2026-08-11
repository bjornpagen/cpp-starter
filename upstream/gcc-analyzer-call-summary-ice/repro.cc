#include <tuple>
#include <variant>

static std::tuple<int> take(std::variant<std::tuple<int>>& v) {
	return std::get<0>(v);
}

static void run(std::tuple<int>& out, std::variant<std::tuple<int>>& v) {
	out = take(v);
}

void caller1(std::tuple<int>& t, std::variant<std::tuple<int>>& v) {
	run(t, v);
}

void caller2(std::tuple<int>& t, std::variant<std::tuple<int>>& v) {
	run(t, v);
}
