#include "net.internal.h"

#include <cstdio>

namespace starter::net_backend {

struct ServerDeleter {
	auto operator()(Server* server) const noexcept -> void;
};

using ServerOwner = std::unique_ptr<Server, ServerDeleter>;

[[nodiscard]] auto err_transient(std::int32_t code) noexcept -> bool;

namespace {

[[nodiscard]] auto set_nonblocking(int fd) noexcept -> std::int32_t {
	auto const flags = ::fcntl(fd, F_GETFL, 0);
	if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
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
	return ::timespec{
	    .tv_sec = represent_as<::time_t>(seconds.count()),
	    .tv_nsec = represent_as<long>(nanoseconds.count()),
	};
}

[[nodiscard]] auto find_vacant(Server const& server) -> std::optional<std::size_t> {
	for (auto index = std::size_t{0}; index < server.slot_count; ++index) {
		if (std::holds_alternative<std::monostate>(server.slots[index].state)) {
			return index;
		}
	}
	return std::nullopt;
}

auto release_slot(Slot& slot) -> void {
	invariant(slot.generation < handle_max_generation);
	++slot.generation;
	slot.descriptor = Fd{};
	std::ignore = slot.state.emplace<std::monostate>();
}

[[nodiscard]] auto arm_listener(Server& server) noexcept -> std::expected<void, WireError> {
	if (server.accept_backoff || server.listener_armed || !find_vacant(server)) {
		return {};
	}
	if (auto const code = arm_listen(server, server.listener.get(), listener_token); code != 0) {
		return std::unexpected(wire_error(stage_queue, code));
	}
	server.listener_armed = true;
	return {};
}

[[nodiscard]] auto arm_connection_read(Server& server, std::size_t index) noexcept -> std::expected<void, WireError> {
	auto& slot = server.slots[index];
	invariant(slot.descriptor.valid());
	if (auto const code = arm_read(server, slot.descriptor.get(), connection_token(index, slot.generation)); code != 0) {
		return std::unexpected(wire_error(stage_queue, code));
	}
	return {};
}

[[nodiscard]] auto arm_connection_write(Server& server, std::size_t index) noexcept -> std::expected<void, WireError> {
	auto& slot = server.slots[index];
	invariant(slot.descriptor.valid());
	if (auto const code = arm_write(server, slot.descriptor.get(), connection_token(index, slot.generation)); code != 0) {
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
		auto const remaining = std::span<char const>{slot.output}.subspan(writing->sent, writing->size - writing->sent);
		auto const count = connection_write(slot.descriptor.get(), remaining);
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
		if (errno == EAGAIN) {
			return arm_connection_write(server, index);
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
		auto const received_before = reading->received;
		auto const count = ::read(slot.descriptor.get(), slot.input.data() + reading->received, slot.input.size() - reading->received);
		if (count > 0) {
			reading->received += static_cast<std::size_t>(count);
			auto const seam = received_before >= 3 ? received_before - 3 : 0;
			auto const input = std::string_view{slot.input.data() + seam, reading->received - seam};
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
		if (errno == EAGAIN) {
			return arm_connection_read(server, index);
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
		auto descriptor = Fd{accept_connection(server.listener.get())};
		if (!descriptor.valid()) {
			if (errno == EINTR || errno == ECONNABORTED) {
				continue;
			}
			if (errno == EAGAIN) {
				return {};
			}
			if (err_transient(errno)) {
				server.accept_backoff = true;
				if (auto const code = arm_timer(server, accept_retry_delay, retry_token); code != 0) {
					return std::unexpected(wire_error(stage_queue, code));
				}
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

[[nodiscard]] auto connections_idle(Server const& server) -> bool {
	return std::ranges::all_of(std::span{server.slots}.first(server.slot_count), [](Slot const& slot) {
		return std::holds_alternative<std::monostate>(slot.state);
	});
}

[[nodiscard]] auto dispatch(Server& server, std::span<Event const> events, RawHandler handler, bool& draining) noexcept
    -> std::expected<bool, WireError> {
	for (auto const& event : events) {
		if (event.token == stop_token) {
			if (draining) {
				return true;
			}
			draining = true;
			continue;
		}
		if (event.token == listener_token) {
			if (event.error) {
				return std::unexpected(wire_error(stage_queue, event.error_code));
			}
			if (draining) {
				server.listener_armed = false;
				continue;
			}
			if (auto accepted = accept_ready(server, handler); !accepted) {
				return std::unexpected(accepted.error());
			}
			continue;
		}
		if (event.token == retry_token) {
			if (event.error) {
				return std::unexpected(wire_error(stage_queue, event.error_code));
			}
			server.accept_backoff = false;
			continue;
		}

		auto const index = token_index(event.token);
		auto const generation = token_generation(event.token);
		if (index >= server.slot_count) {
			continue;
		}
		auto& slot = server.slots[index];
		if (generation == 0 || generation != slot.generation) {
			continue;
		}
		if (event.error) {
			release_slot(slot);
			continue;
		}
		if (event.ready == Ready::Read && std::holds_alternative<Reading>(slot.state)) {
			if (auto advanced = advance_read(server, index, handler); !advanced) {
				return std::unexpected(advanced.error());
			}
		} else if (event.ready == Ready::Write && std::holds_alternative<Writing>(slot.state)) {
			if (auto advanced = advance_write(server, index); !advanced) {
				return std::unexpected(advanced.error());
			}
		}
	}
	return false;
}

}

Server::~Server() {
	queue_close(*this);
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

[[nodiscard]] auto handle_index_width() noexcept -> std::uint64_t {
	return handle_index_bits;
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

	auto server = ServerOwner{new Server{connection_count}};
	if (auto opened = queue_open(*server); !opened) {
		return std::unexpected(opened.error());
	}
	server->listener = std::move(*listener);
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

	if (auto const code = arm_signals(server); code != 0) {
		return std::unexpected(wire_error(stage_signal, code));
	}
	if (auto armed = arm_listener(server); !armed) {
		return armed;
	}

	auto events = std::array<Event, 64>{};
	auto draining = false;
	for (;;) {
		auto timeout = timeout_until(nearest_deadline(server));
		auto const waited = wait_events(server, events, timeout);
		if (!waited) {
			if (waited.error() == EINTR) {
				continue;
			}
			return std::unexpected(wire_error(stage_queue, waited.error()));
		}
		expire_slots(server, Clock::now());
		auto const ready = std::span{events}.first(*waited);
		auto dispatched = dispatch(server, ready, handler, draining);
		if (!dispatched) {
			return std::unexpected(dispatched.error());
		}
		if (*dispatched) {
			return {};
		}
		if (draining && connections_idle(server)) {
			return {};
		}
		if (!draining) {
			if (auto armed = arm_listener(server); !armed) {
				return armed;
			}
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