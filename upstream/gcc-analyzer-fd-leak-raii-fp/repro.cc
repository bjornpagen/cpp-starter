// Minimized from unsafe/net.backend.cc (kqueue accept loop).
// g++-16 -O2 -fanalyzer -c repro.cc
// Every fd below is owned by an Fd object and closed by ~Fd (or handed to
// slot.descriptor, whose ~Fd closes it later); -fanalyzer still reports
// CWE-775 fd leaks.
#include <cerrno>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

class Fd {
public:
	Fd() = default;

	explicit Fd(int value) noexcept : value_{value} {}

	Fd(Fd&& other) noexcept : value_{std::exchange(other.value_, -1)} {}

	auto operator=(Fd&& other) noexcept -> Fd& {
		if (this != &other) {
			reset();
			value_ = std::exchange(other.value_, -1);
		}
		return *this;
	}

	Fd(Fd const&) = delete;
	auto operator=(Fd const&) -> Fd& = delete;

	~Fd() {
		reset();
	}

	[[nodiscard]] auto get() const noexcept -> int {
		return value_;
	}

	[[nodiscard]] auto valid() const noexcept -> bool {
		return value_ >= 0;
	}

private:
	auto reset() noexcept -> void {
		if (value_ >= 0) {
			::close(std::exchange(value_, -1));
		}
	}

	int value_ = -1;
};

struct Slot {
	Fd descriptor{};
	bool busy = false;
};

struct Server {
	Fd queue{};
	Fd listener{};
	Slot slots[4]{};
};

[[nodiscard]] auto set_nonblocking(int fd) noexcept -> int {
	auto const flags = ::fcntl(fd, F_GETFL, 0);
	if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
		return errno;
	}
	return 0;
}

[[nodiscard]] auto accept_ready(Server& server) noexcept -> int {
	for (auto& slot : server.slots) {
		if (slot.busy) {
			continue;
		}
		auto descriptor = Fd{::accept(server.listener.get(), nullptr, nullptr)};
		if (!descriptor.valid()) {
			if (errno == EINTR) {
				continue;
			}
			return errno;
		}
		if (set_nonblocking(descriptor.get()) != 0) {
			continue;
		}
		slot.descriptor = std::move(descriptor);
		slot.busy = true;
	}
	return 0;
}
