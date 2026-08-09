#include <string>
#include <algorithm>

struct name_text {
	char data[8];
	std::size_t size;

	template<std::size_t N>
	consteval name_text(char const (&literal)[N]) : data{}, size{N - 1} {
		std::copy_n(static_cast<char const*>(literal), N - 1, static_cast<char*>(data));
	}
};

template<name_text S>
consteval std::size_t via_pointer_size() {
	return std::string(static_cast<char const*>(S.data), S.size).size();
}

template<name_text S>
consteval std::size_t via_iterators() {
	return std::string(static_cast<char const*>(S.data), static_cast<char const*>(S.data) + S.size).size();
}

static_assert(via_iterators<name_text{"hi"}>() == 2);
static_assert(via_pointer_size<name_text{"hi"}>() == 2);

int main() {}
