#include <meta>

enum class E { A, B };

consteval int count() {
	int total = 0;
	template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^E))) {
		total += 1;
	}
	return total;
}

static_assert(count() == 2);

int main() {}
