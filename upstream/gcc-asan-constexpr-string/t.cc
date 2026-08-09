#include <string>

inline constexpr char text[] = {'h', 'i'};

consteval std::size_t via_pointer_size() {
	return std::string(static_cast<char const*>(text), 2).size();
}

consteval std::size_t via_iterators() {
	return std::string(text, text + 2).size();
}

static_assert(via_pointer_size() == 2);
static_assert(via_iterators() == 2);

int main() {}
