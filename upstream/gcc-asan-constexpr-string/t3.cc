#include <meta>
#include <string>

consteval std::size_t via_pointer_size() {
	char const* p = std::define_static_string("hi");
	return std::string(p, 2).size();
}

consteval std::size_t via_iterators() {
	char const* p = std::define_static_string("hi");
	return std::string(p, p + 2).size();
}

static_assert(via_iterators() == 2);
static_assert(via_pointer_size() == 2);

int main() {}
