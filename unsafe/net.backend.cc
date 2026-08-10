#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>

namespace starter::net_backend {

struct Server;

struct ServerDeleter {
	auto operator()(Server* server) const noexcept -> void;
};

using ServerOwner = std::unique_ptr<Server, ServerDeleter>;
using WireError = std::array<std::int32_t, 2>;
using RawHandler = std::optional<std::size_t> (*)(std::string_view request, std::span<char> out) noexcept;

namespace {

inline constexpr std::size_t buffer_bytes = 8192;
inline constexpr std::size_t max_connections = 128;
inline constexpr std::int64_t max_timeout_milliseconds = 86'400'000;
inline constexpr int listen_backlog = 256;

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
inline constexpr std::uint64_t interrupt_token = 2;
inline constexpr std::uint64_t terminate_token = 3;
inline constexpr std::uint64_t slot_bits = 8;
inline constexpr std::uint64_t slot_mask = (std::uint64_t{1} << slot_bits) - 1;
inline constexpr std::uint64_t max_generation = std::numeric_limits<std::uint64_t>::max() >> slot_bits;

static_assert(max_connections <= (std::uint64_t{1} << slot_bits));

using Clock = std::chrono::steady_clock;
using Deadline = Clock::time_point;

[[nodiscard]] constexpr auto wire_error(std::int32_t stage, std::int32_t code) -> WireError {
	return {stage, code};
}

/* PIN(clang-contracts): keep the Clang-readable boundary fail-stop until Clang parses C++26 contracts */
auto invariant(bool condition) noexcept -> void {
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

[[nodiscard]] auto make_event(std::uint64_t ident, std::int16_t filter, std::uint16_t flags, std::uint32_t fflags,
                              std::uint64_t token) noexcept -> ::kevent64_s {
	auto event = ::kevent64_s{};
	event.ident = ident;
	event.filter = filter;
	event.flags = flags;
	event.fflags = fflags;
	event.data = 0;
	event.udata = token;
	event.ext[0] = 0;
	event.ext[1] = 0;
	return event;
}

[[nodiscard]] auto submit(int queue, ::kevent64_s const& event) noexcept -> std::int32_t {
	return ::kevent64(queue, &event, 1, nullptr, 0, 0, nullptr) < 0 ? errno : 0;
}

[[nodiscard]] auto set_nonblocking(int fd) noexcept -> std::int32_t {
	auto const flags = ::fcntl(fd, F_GETFL, 0);
	if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
		return errno;
	}
	return 0;
}

[[nodiscard]] auto configure_connection(int fd) noexcept -> std::int32_t {
	if (auto const code = set_nonblocking(fd); code != 0) {
		return code;
	}
	auto const one = 1;
	if (::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one) < 0) {
		return errno;
	}
	return 0;
}

[[nodiscard]] auto make_listener(std::uint16_t port) noexcept -> std::expected<Fd, WireError> {
	auto descriptor = Fd{::socket(AF_INET, SOCK_STREAM, 0)};
	if (!descriptor.valid()) {
		return std::unexpected(wire_error(stage_socket, errno));
	}

	auto const one = 1;
	if (::setsockopt(descriptor.get(), SOL_SOCKET, SO_REUSEADDR, &one, sizeof one) < 0) {
		return std::unexpected(wire_error(stage_option, errno));
	}

	auto address = ::sockaddr_in{};
	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	/* SAFETY: sockaddr_in -> sockaddr is the bind(2) ABI's aliasing rule */
	if (::bind(descriptor.get(), reinterpret_cast<::sockaddr const*>(&address), sizeof address) < 0) {
		return std::unexpected(wire_error(stage_bind, errno));
	}
	if (auto const code = set_nonblocking(descriptor.get()); code != 0) {
		return std::unexpected(wire_error(stage_nonblock, code));
	}
	if (::listen(descriptor.get(), listen_backlog) < 0) {
		return std::unexpected(wire_error(stage_listen, errno));
	}
	return descriptor;
}

[[nodiscard]] auto bound_port(int fd) noexcept -> std::expected<std::uint16_t, WireError> {
	auto address = ::sockaddr_in{};
	auto length = ::socklen_t{sizeof address};
	/* SAFETY: sockaddr_in -> sockaddr is the getsockname(2) ABI's aliasing rule */
	if (::getsockname(fd, reinterpret_cast<::sockaddr*>(&address), &length) < 0) {
		return std::unexpected(wire_error(stage_resolve, errno));
	}
	return ntohs(address.sin_port);
}

[[nodiscard]] constexpr auto connection_token(std::size_t index, std::uint64_t generation) -> std::uint64_t {
	return (generation << slot_bits) | static_cast<std::uint64_t>(index);
}

[[nodiscard]] constexpr auto token_index(std::uint64_t token) -> std::size_t {
	return static_cast<std::size_t>(token & slot_mask);
}

[[nodiscard]] constexpr auto token_generation(std::uint64_t token) -> std::uint64_t {
	return token >> slot_bits;
}

[[nodiscard]] auto deadline_after(std::chrono::milliseconds timeout) -> Deadline {
	auto const now = Clock::now();
	auto const delta = std::chrono::duration_cast<Clock::duration>(timeout);
	if (now > Deadline::max() - delta) {
		return Deadline::max();
	}
	return now + delta;
}

[[nodiscard]] auto timeout_until(std::optional<Deadline> deadline) -> std::optional<::timespec> {
	if (!deadline) {
		return std::nullopt;
	}
	auto const now = Clock::now();
	auto const remaining = *deadline <= now ? Clock::duration::zero() : *deadline - now;
	auto const seconds = std::chrono::duration_cast<std::chrono::seconds>(remaining);
	auto const nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(remaining - seconds);
	return ::timespec{.tv_sec = static_cast<::time_t>(seconds.count()), .tv_nsec = static_cast<long>(nanoseconds.count())};
}

using SignalHandler = decltype(SIG_DFL);

class SignalGuard {
public:
	[[nodiscard]] static auto make() noexcept -> std::expected<SignalGuard, std::int32_t> {
		/* SAFETY: Darwin EVFILT_SIGNAL records delivery attempts even when SIG_IGN prevents default action */
		auto const old_interrupt = std::signal(SIGINT, SIG_IGN);
		if (old_interrupt == SIG_ERR) {
			return std::unexpected(errno == 0 ? EINVAL : errno);
		}
		auto const old_terminate = std::signal(SIGTERM, SIG_IGN);
		if (old_terminate == SIG_ERR) {
			auto const code = errno == 0 ? EINVAL : errno;
			std::ignore = std::signal(SIGINT, old_interrupt);
			return std::unexpected(code);
		}
		return SignalGuard{old_interrupt, old_terminate};
	}

	SignalGuard(SignalGuard&& other) noexcept
	    : old_interrupt_{other.old_interrupt_}, old_terminate_{other.old_terminate_}, active_{std::exchange(other.active_, false)} {}

	auto operator=(SignalGuard&&) -> SignalGuard& = delete;
	SignalGuard(SignalGuard const&) = delete;
	auto operator=(SignalGuard const&) -> SignalGuard& = delete;

	~SignalGuard() {
		if (active_) {
			std::ignore = std::signal(SIGINT, old_interrupt_);
			std::ignore = std::signal(SIGTERM, old_terminate_);
		}
	}

private:
	SignalGuard(SignalHandler old_interrupt, SignalHandler old_terminate) noexcept
	    : old_interrupt_{old_interrupt}, old_terminate_{old_terminate} {}

	SignalHandler old_interrupt_;
	SignalHandler old_terminate_;
	bool active_ = true;
};

}

struct Server {
	explicit Server(std::size_t connection_count) : slot_count{connection_count} {}

	Fd queue{};
	Fd listener{};
	std::uint16_t port = 0;
	std::size_t slot_count;
	std::array<Slot, max_connections> slots{};
	bool listener_armed = false;
	bool ran = false;
	std::chrono::milliseconds request_timeout{0};
	std::chrono::milliseconds response_timeout{0};
};

namespace {

[[nodiscard]] auto find_vacant(Server const& server) -> std::optional<std::size_t> {
	for (auto index = std::size_t{0}; index < server.slot_count; ++index) {
		if (std::holds_alternative<std::monostate>(server.slots[index].state)) {
			return index;
		}
	}
	return std::nullopt;
}

auto release_slot(Slot& slot) -> void {
	invariant(slot.generation < max_generation);
	++slot.generation;
	slot.descriptor = Fd{};
	std::ignore = slot.state.emplace<std::monostate>();
}

[[nodiscard]] auto arm_listener(Server& server) noexcept -> std::expected<void, WireError> {
	if (server.listener_armed || !find_vacant(server)) {
		return {};
	}
	auto const event = make_event(static_cast<std::uint64_t>(server.listener.get()), EVFILT_READ, EV_ADD | EV_ONESHOT, 0, listener_token);
	if (auto const code = submit(server.queue.get(), event); code != 0) {
		return std::unexpected(wire_error(stage_queue, code));
	}
	server.listener_armed = true;
	return {};
}

[[nodiscard]] auto arm_connection(Server& server, std::size_t index, std::int16_t filter) noexcept -> std::expected<void, WireError> {
	auto& slot = server.slots[index];
	invariant(slot.descriptor.valid());
	auto const event = make_event(static_cast<std::uint64_t>(slot.descriptor.get()), filter, EV_ADD | EV_ONESHOT, 0,
	                              connection_token(index, slot.generation));
	if (auto const code = submit(server.queue.get(), event); code != 0) {
		return std::unexpected(wire_error(stage_queue, code));
	}
	return {};
}

[[nodiscard]] auto advance_write(Server& server, std::size_t index) noexcept -> std::expected<void, WireError> {
	auto& slot = server.slots[index];
	auto* writing = std::get_if<Writing>(&slot.state);
	invariant(writing != nullptr);

	for (;;) {
		if (Clock::now() >= writing->deadline) {
			release_slot(slot);
			return {};
		}
		if (writing->sent == writing->size) {
			release_slot(slot);
			return {};
		}
		auto const count = ::write(slot.descriptor.get(), slot.output.data() + writing->sent, writing->size - writing->sent);
		if (count > 0) {
			writing->sent += static_cast<std::size_t>(count);
			continue;
		}
		if (count == 0) {
			release_slot(slot);
			return {};
		}
		if (errno == EINTR) {
			continue;
		}
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return arm_connection(server, index, EVFILT_WRITE);
		}
		release_slot(slot);
		return {};
	}
}

[[nodiscard]] auto advance_read(Server& server, std::size_t index, RawHandler handler) noexcept -> std::expected<void, WireError> {
	auto& slot = server.slots[index];
	auto* reading = std::get_if<Reading>(&slot.state);
	invariant(reading != nullptr);

	for (;;) {
		if (Clock::now() >= reading->deadline) {
			release_slot(slot);
			return {};
		}
		if (reading->received == slot.input.size()) {
			break;
		}
		auto const count = ::read(slot.descriptor.get(), slot.input.data() + reading->received, slot.input.size() - reading->received);
		if (count > 0) {
			reading->received += static_cast<std::size_t>(count);
			auto const input = std::string_view{slot.input.data(), reading->received};
			if (input.contains("\r\n\r\n")) {
				break;
			}
			continue;
		}
		if (count == 0) {
			break;
		}
		if (errno == EINTR) {
			continue;
		}
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return arm_connection(server, index, EVFILT_READ);
		}
		release_slot(slot);
		return {};
	}

	if (reading->received == 0) {
		release_slot(slot);
		return {};
	}
	auto const response = handler(std::string_view{slot.input.data(), reading->received}, std::span<char>{slot.output});
	if (!response || *response > slot.output.size()) {
		release_slot(slot);
		return {};
	}
	std::ignore = slot.state.emplace<Writing>(Writing{.size = *response, .sent = 0, .deadline = deadline_after(server.response_timeout)});
	return advance_write(server, index);
}

[[nodiscard]] auto accept_ready(Server& server, RawHandler handler) noexcept -> std::expected<void, WireError> {
	server.listener_armed = false;
	for (;;) {
		auto const vacant = find_vacant(server);
		if (!vacant) {
			return {};
		}
		auto descriptor = Fd{::accept(server.listener.get(), nullptr, nullptr)};
		if (!descriptor.valid()) {
			if (errno == EINTR || errno == ECONNABORTED) {
				continue;
			}
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				return {};
			}
			return std::unexpected(wire_error(stage_accept, errno));
		}
		if (configure_connection(descriptor.get()) != 0) {
			continue;
		}
		auto& slot = server.slots[*vacant];
		slot.input.fill('\0');
		slot.output.fill('\0');
		slot.descriptor = std::move(descriptor);
		std::ignore = slot.state.emplace<Reading>(Reading{.received = 0, .deadline = deadline_after(server.request_timeout)});
		if (auto advanced = advance_read(server, *vacant, handler); !advanced) {
			return advanced;
		}
	}
}

[[nodiscard]] auto nearest_deadline(Server const& server) -> std::optional<Deadline> {
	auto nearest = std::optional<Deadline>{};
	for (auto const& slot : std::span{server.slots}.first(server.slot_count)) {
		auto deadline = std::optional<Deadline>{};
		if (auto const* reading = std::get_if<Reading>(&slot.state)) {
			deadline = reading->deadline;
		} else if (auto const* writing = std::get_if<Writing>(&slot.state)) {
			deadline = writing->deadline;
		}
		if (deadline && (!nearest || *deadline < *nearest)) {
			nearest = deadline;
		}
	}
	return nearest;
}

auto expire_slots(Server& server, Deadline now) -> void {
	for (auto& slot : std::span{server.slots}.first(server.slot_count)) {
		if (auto const* reading = std::get_if<Reading>(&slot.state); reading != nullptr && reading->deadline <= now) {
			release_slot(slot);
			continue;
		}
		if (auto const* writing = std::get_if<Writing>(&slot.state); writing != nullptr && writing->deadline <= now) {
			release_slot(slot);
		}
	}
}

[[nodiscard]] auto register_signals(Server& server) noexcept -> std::expected<void, WireError> {
	auto const interrupt = make_event(SIGINT, EVFILT_SIGNAL, EV_ADD | EV_CLEAR, 0, interrupt_token);
	if (auto const code = submit(server.queue.get(), interrupt); code != 0) {
		return std::unexpected(wire_error(stage_signal, code));
	}
	auto const terminate = make_event(SIGTERM, EVFILT_SIGNAL, EV_ADD | EV_CLEAR, 0, terminate_token);
	if (auto const code = submit(server.queue.get(), terminate); code != 0) {
		return std::unexpected(wire_error(stage_signal, code));
	}
	return {};
}

[[nodiscard]] auto dispatch(Server& server, std::span<::kevent64_s const> events, RawHandler handler) noexcept
    -> std::expected<bool, WireError> {
	if (std::ranges::any_of(events, [](auto const& event) {
		    return event.udata == interrupt_token || event.udata == terminate_token;
	    })) {
		return true;
	}

	for (auto const& event : events) {
		if (event.udata == listener_token) {
			if ((event.flags & EV_ERROR) != 0) {
				return std::unexpected(wire_error(stage_queue, static_cast<std::int32_t>(event.data)));
			}
			if (auto accepted = accept_ready(server, handler); !accepted) {
				return std::unexpected(accepted.error());
			}
			continue;
		}

		auto const index = token_index(event.udata);
		auto const generation = token_generation(event.udata);
		if (index >= server.slot_count) {
			continue;
		}
		auto& slot = server.slots[index];
		if (generation == 0 || generation != slot.generation) {
			continue;
		}
		if ((event.flags & EV_ERROR) != 0) {
			release_slot(slot);
			continue;
		}
		if (event.filter == EVFILT_READ && std::holds_alternative<Reading>(slot.state)) {
			if (auto advanced = advance_read(server, index, handler); !advanced) {
				return std::unexpected(advanced.error());
			}
		} else if (event.filter == EVFILT_WRITE && std::holds_alternative<Writing>(slot.state)) {
			if (auto advanced = advance_write(server, index); !advanced) {
				return std::unexpected(advanced.error());
			}
		}
	}
	return false;
}

}

[[nodiscard]] auto err_transient(std::int32_t code) noexcept -> bool {
	if (code == EAGAIN) {
		return true;
	}
	switch (code) {
	case EINTR:
	case ECONNRESET:
	case ECONNABORTED:
	case EPIPE:
	case ETIMEDOUT:
	case ENETRESET:
	case EMFILE:
	case ENFILE:
	case ENOBUFS:
	case ENOMEM:
	case EADDRINUSE:
		return true;
	default:
		return false;
	}
}

[[nodiscard]] auto buffer_capacity() noexcept -> std::size_t {
	return buffer_bytes;
}

[[nodiscard]] auto connection_capacity() noexcept -> std::size_t {
	return max_connections;
}

[[nodiscard]] auto timeout_capacity() noexcept -> std::chrono::milliseconds {
	return std::chrono::milliseconds{max_timeout_milliseconds};
}

[[nodiscard]] auto server_open(std::uint16_t port, std::size_t connection_count, std::chrono::milliseconds request_timeout,
                               std::chrono::milliseconds response_timeout) noexcept -> std::expected<ServerOwner, WireError> {
	if (connection_count == 0 || connection_count > max_connections || request_timeout.count() <= 0 || response_timeout.count() <= 0 ||
	    request_timeout.count() > max_timeout_milliseconds || response_timeout.count() > max_timeout_milliseconds) {
		return std::unexpected(wire_error(stage_configuration, EINVAL));
	}

	auto listener = make_listener(port);
	if (!listener) {
		return std::unexpected(listener.error());
	}
	auto resolved_port = bound_port(listener->get());
	if (!resolved_port) {
		return std::unexpected(resolved_port.error());
	}
	auto queue = Fd{::kqueue()};
	if (!queue.valid()) {
		return std::unexpected(wire_error(stage_queue, errno));
	}

	auto server = ServerOwner{new Server{connection_count}};
	server->listener = std::move(*listener);
	server->queue = std::move(queue);
	server->port = *resolved_port;
	server->request_timeout = request_timeout;
	server->response_timeout = response_timeout;
	return server;
}

[[nodiscard]] auto server_run(Server& server, RawHandler handler) noexcept -> std::expected<void, WireError> {
	if (server.ran || handler == nullptr) {
		return std::unexpected(wire_error(stage_configuration, EALREADY));
	}
	server.ran = true;

	auto signal_guard = SignalGuard::make();
	if (!signal_guard) {
		return std::unexpected(wire_error(stage_signal, signal_guard.error()));
	}
	if (auto registered = register_signals(server); !registered) {
		return registered;
	}
	if (auto armed = arm_listener(server); !armed) {
		return armed;
	}

	auto events = std::array<::kevent64_s, 64>{};
	for (;;) {
		auto timeout = timeout_until(nearest_deadline(server));
		auto const count =
		    ::kevent64(server.queue.get(), nullptr, 0, events.data(), static_cast<int>(events.size()), 0, timeout ? &*timeout : nullptr);
		if (count < 0) {
			if (errno == EINTR) {
				continue;
			}
			return std::unexpected(wire_error(stage_queue, errno));
		}
		expire_slots(server, Clock::now());
		auto const ready = std::span{events}.first(static_cast<std::size_t>(count));
		auto dispatched = dispatch(server, ready, handler);
		if (!dispatched) {
			return std::unexpected(dispatched.error());
		}
		if (*dispatched) {
			return {};
		}
		if (auto armed = arm_listener(server); !armed) {
			return armed;
		}
	}
}

[[nodiscard]] auto server_port(Server const& server) noexcept -> std::uint16_t {
	return server.port;
}

auto ServerDeleter::operator()(Server* server) const noexcept -> void {
	delete server;
}

auto output_flush() noexcept -> void {
	std::ignore = std::fflush(nullptr);
}

}
