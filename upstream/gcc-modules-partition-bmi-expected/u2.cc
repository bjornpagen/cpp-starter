// Driver for the q.cc/p2.cc/m.cc attempt (payload type from imported module Q).
#include <expected>
import M;
import Q;

int main() {
	return Db{}.admit(std::expected<q::handle, int>{q::handle{41}}).value_or(0) == 42 ? 0 : 1;
}
