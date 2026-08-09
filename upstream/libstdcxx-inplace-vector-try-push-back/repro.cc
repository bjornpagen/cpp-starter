#include <inplace_vector>
#include <type_traits>

int main() {
	std::inplace_vector<int, 2> v;
	static_assert(std::is_same_v<decltype(v.try_push_back(1)), int*>, "P0843: try_push_back returns pointer");
}
