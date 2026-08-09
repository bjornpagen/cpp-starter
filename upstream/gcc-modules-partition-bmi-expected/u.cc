#include <expected>
import M;

int main() {
	return Db{}.admit(std::expected<int, int>{41}).value_or(0) == 42 ? 0 : 1;
}
