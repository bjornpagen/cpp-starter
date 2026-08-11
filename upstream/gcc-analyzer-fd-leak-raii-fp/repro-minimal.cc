// Reduction: both false-positive mechanisms in one small TU.
// g++-16 -O2 -fanalyzer -c repro-minimal.cc
#include <sys/socket.h>
#include <unistd.h>

class Fd {
public:
	Fd() = default;
	explicit Fd(int value) noexcept : value_{value} {}
	Fd(Fd const&) = delete;
	auto operator=(Fd const&) -> Fd& = delete;
	~Fd() {
		if (value_ >= 0) {
			::close(value_);
		}
	}
	[[nodiscard]] auto get() const noexcept -> int { return value_; }
private:
	int value_ = -1;
};

struct Server {
	Fd listener{};
};

auto accept_one(Server& server) noexcept -> int {
	auto descriptor = Fd{::accept(server.listener.get(), nullptr, nullptr)};
	return descriptor.get() >= 0 ? 0 : -1;
}
