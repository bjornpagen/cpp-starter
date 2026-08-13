#include "net.internal.h"

#include <sys/event.h>

namespace starter::net_backend {

namespace {

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

}

[[nodiscard]] auto queue_open(Server& server) noexcept -> std::expected<void, WireError> {
	auto queue = Fd{::kqueue()};
	if (!queue.valid()) {
		return std::unexpected(wire_error(stage_queue, errno));
	}
	server.queue = std::move(queue);
	return {};
}

auto queue_close(Server& server) noexcept -> void {
	if (server.old_interrupt != nullptr) {
		std::ignore = std::signal(SIGINT, server.old_interrupt);
		server.old_interrupt = nullptr;
	}
	if (server.old_terminate != nullptr) {
		std::ignore = std::signal(SIGTERM, server.old_terminate);
		server.old_terminate = nullptr;
	}
}

[[nodiscard]] auto arm_read(Server& server, int fd, std::uint64_t token) noexcept -> std::int32_t {
	return submit(server.queue.get(), make_event(static_cast<std::uint64_t>(fd), EVFILT_READ, EV_ADD | EV_ONESHOT, 0, token));
}

[[nodiscard]] auto arm_write(Server& server, int fd, std::uint64_t token) noexcept -> std::int32_t {
	return submit(server.queue.get(), make_event(static_cast<std::uint64_t>(fd), EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, token));
}

[[nodiscard]] auto arm_listen(Server& server, int fd, std::uint64_t token) noexcept -> std::int32_t {
	return arm_read(server, fd, token);
}

[[nodiscard]] auto arm_timer(Server& server, std::chrono::milliseconds delay, std::uint64_t token) noexcept -> std::int32_t {
	auto event = make_event(1, EVFILT_TIMER, EV_ADD | EV_ONESHOT, 0, token);
	event.data = delay.count();
	return submit(server.queue.get(), event);
}

[[nodiscard]] auto arm_signals(Server& server) noexcept -> std::int32_t {
	/* SAFETY: Darwin EVFILT_SIGNAL records delivery attempts even when SIG_IGN prevents default action */
	auto const old_interrupt = std::signal(SIGINT, SIG_IGN);
	if (old_interrupt == SIG_ERR) {
		return errno == 0 ? EINVAL : errno;
	}
	auto const old_terminate = std::signal(SIGTERM, SIG_IGN);
	if (old_terminate == SIG_ERR) {
		auto const code = errno == 0 ? EINVAL : errno;
		std::ignore = std::signal(SIGINT, old_interrupt);
		return code;
	}
	server.old_interrupt = old_interrupt;
	server.old_terminate = old_terminate;
	auto const interrupt = make_event(static_cast<std::uint64_t>(SIGINT), EVFILT_SIGNAL, EV_ADD | EV_CLEAR, 0, stop_token);
	if (auto const code = submit(server.queue.get(), interrupt); code != 0) {
		return code;
	}
	auto const terminate = make_event(static_cast<std::uint64_t>(SIGTERM), EVFILT_SIGNAL, EV_ADD | EV_CLEAR, 0, stop_token);
	return submit(server.queue.get(), terminate);
}

[[nodiscard]] auto wait_events(Server& server, std::span<Event> out, std::optional<timespec> timeout) noexcept
    -> std::expected<std::size_t, std::int32_t> {
	auto events = std::array<::kevent64_s, 64>{};
	auto const cap = std::min(out.size(), events.size());
	auto const count =
	    ::kevent64(server.queue.get(), nullptr, 0, events.data(), static_cast<int>(cap), 0, timeout ? &*timeout : nullptr);
	if (count < 0) {
		return std::unexpected(errno);
	}
	auto const n = static_cast<std::size_t>(count);
	for (auto index = std::size_t{0}; index < n; ++index) {
		auto const& event = events[index];
		auto ready = Ready::Read;
		if (event.filter == EVFILT_WRITE) {
			ready = Ready::Write;
		}
		out[index] = Event{
		    .token = event.udata,
		    .ready = ready,
		    .error = (event.flags & EV_ERROR) != 0,
		    .error_code = (event.flags & EV_ERROR) != 0 ? static_cast<std::int32_t>(event.data) : 0,
		};
	}
	return n;
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

[[nodiscard]] auto accept_connection(int listener) noexcept -> int {
	return ::accept(listener, nullptr, nullptr);
}

[[nodiscard]] auto connection_write(int fd, std::span<char const> bytes) noexcept -> std::ptrdiff_t {
	return ::write(fd, bytes.data(), bytes.size());
}

}