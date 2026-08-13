#ifndef STARTER_NET_INTERNAL_H
#define STARTER_NET_INTERNAL_H

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <csignal>
#include <exception>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <ctime>
#include <tuple>
#include <utility>
#include <variant>

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace starter::net_backend {

using WireError = std::array<std::int32_t, 2>;
using RawHandler = std::optional<std::size_t> (*)(std::string_view request, std::span<char> out) noexcept;

inline constexpr std::size_t buffer_bytes = 8192;
inline constexpr std::size_t max_connections = 128;
inline constexpr std::int64_t max_timeout_milliseconds = 86'400'000;
inline constexpr int listen_backlog = 256;
inline constexpr std::uint64_t handle_index_bits = 8;
inline constexpr std::uint64_t handle_index_mask = (std::uint64_t{1} << handle_index_bits) - 1;
inline constexpr std::uint64_t handle_max_generation = std::numeric_limits<std::uint64_t>::max() >> handle_index_bits;

inline constexpr std::int32_t stage_configuration = 1;
inline constexpr std::int32_t stage_socket = 2;
inline constexpr std::int32_t stage_option = 3;
inline constexpr std::int32_t stage_bind = 4;
inline constexpr std::int32_t stage_listen = 5;
inline constexpr std::int32_t stage_nonblock = 6;
inline constexpr std::int32_t stage_queue = 7;
inline constexpr std::int32_t stage_signal = 8;
inline constexpr std::int32_t stage_resolve = 9;
inline constexpr std::int32_t stage_accept = 10;

inline constexpr std::uint64_t listener_token = 1;
inline constexpr std::uint64_t stop_token = 2;
inline constexpr std::uint64_t retry_token = 3;
inline constexpr std::chrono::milliseconds accept_retry_delay{100};

static_assert(max_connections <= (std::size_t{1} << handle_index_bits));

using Clock = std::chrono::steady_clock;
using Deadline = Clock::time_point;
using SignalAction = void (*)(int);

[[nodiscard]] constexpr auto wire_error(std::int32_t stage, std::int32_t code) -> WireError {
	return {stage, code};
}

template<class To, class From>
[[nodiscard]] constexpr auto represent_as(From value) -> To {
	if constexpr (std::same_as<To, From>) {
		return value;
	} else {
		return static_cast<To>(value);
	}
}

/* PIN(clang-contracts): keep the Clang-readable boundary fail-stop until Clang parses C++26 contracts */
inline auto invariant(bool condition) noexcept -> void {
	if (!condition) {
		std::terminate();
	}
}

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
			std::ignore = ::close(std::exchange(value_, -1));
		}
	}

	int value_ = -1;
};

struct Reading {
	std::size_t received;
	Deadline deadline;
};

struct Writing {
	std::size_t size;
	std::size_t sent;
	Deadline deadline;
};

using SlotState = std::variant<std::monostate, Reading, Writing>;

struct Slot {
	std::uint64_t generation = 1;
	Fd descriptor{};
	std::array<char, buffer_bytes> input{};
	std::array<char, buffer_bytes> output{};
	SlotState state{};
};

enum class Ready : std::uint8_t {
	Read,
	Write,
};

struct Event {
	std::uint64_t token;
	Ready ready;
	bool error;
	std::int32_t error_code;
};

struct Server {
	explicit Server(std::size_t connection_count) : slot_count{connection_count} {}
	Server(Server const&) = delete;
	auto operator=(Server const&) -> Server& = delete;
	~Server();

	Fd queue{};
	Fd aux_timer{};
	Fd aux_signal{};
	Fd listener{};
	std::uint16_t port = 0;
	std::size_t slot_count;
	std::array<Slot, max_connections> slots{};
	bool listener_armed = false;
	bool accept_backoff = false;
	bool ran = false;
	std::chrono::milliseconds request_timeout{0};
	std::chrono::milliseconds response_timeout{0};
	SignalAction old_interrupt = nullptr;
	SignalAction old_terminate = nullptr;
	bool signals_blocked = false;
	sigset_t saved_mask{};
};

[[nodiscard]] constexpr auto connection_token(std::size_t index, std::uint64_t generation) -> std::uint64_t {
	return (generation << handle_index_bits) | represent_as<std::uint64_t>(index);
}

[[nodiscard]] constexpr auto token_index(std::uint64_t token) -> std::size_t {
	return represent_as<std::size_t>(token & handle_index_mask);
}

[[nodiscard]] constexpr auto token_generation(std::uint64_t token) -> std::uint64_t {
	return token >> handle_index_bits;
}

[[nodiscard]] auto queue_open(Server& server) noexcept -> std::expected<void, WireError>;
auto queue_close(Server& server) noexcept -> void;
[[nodiscard]] auto arm_read(Server& server, int fd, std::uint64_t token) noexcept -> std::int32_t;
[[nodiscard]] auto arm_write(Server& server, int fd, std::uint64_t token) noexcept -> std::int32_t;
[[nodiscard]] auto arm_listen(Server& server, int fd, std::uint64_t token) noexcept -> std::int32_t;
[[nodiscard]] auto arm_timer(Server& server, std::chrono::milliseconds delay, std::uint64_t token) noexcept -> std::int32_t;
[[nodiscard]] auto arm_signals(Server& server) noexcept -> std::int32_t;
[[nodiscard]] auto wait_events(Server& server, std::span<Event> out, std::optional<timespec> timeout) noexcept
    -> std::expected<std::size_t, std::int32_t>;
[[nodiscard]] auto configure_connection(int fd) noexcept -> std::int32_t;
[[nodiscard]] auto accept_connection(int listener) noexcept -> int;
[[nodiscard]] auto connection_write(int fd, std::span<char const> bytes) noexcept -> std::ptrdiff_t;

}

#endif