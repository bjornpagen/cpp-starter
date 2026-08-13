#include "net.internal.h"

#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>

namespace starter::net_backend {

namespace {

[[nodiscard]] auto set_nonblocking(int fd) noexcept -> std::int32_t {
	auto const flags = ::fcntl(fd, F_GETFL, 0);
	if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
		return errno;
	}
	return 0;
}

[[nodiscard]] auto arm_epoll(int queue, int fd, std::uint32_t events, std::uint64_t token) noexcept -> std::int32_t {
	auto event = ::epoll_event{};
	event.events = events;
	event.data.u64 = token;
	if (::epoll_ctl(queue, EPOLL_CTL_MOD, fd, &event) == 0) {
		return 0;
	}
	if (errno != ENOENT) {
		return errno;
	}
	if (::epoll_ctl(queue, EPOLL_CTL_ADD, fd, &event) < 0) {
		return errno;
	}
	return 0;
}

[[nodiscard]] auto timeout_milliseconds(std::optional<timespec> timeout) noexcept -> int {
	if (!timeout) {
		return -1;
	}
	auto const sec_ms = static_cast<long long>(timeout->tv_sec) * 1000LL;
	auto const nsec_ms = (static_cast<long long>(timeout->tv_nsec) + 999999LL) / 1000000LL;
	auto const total = sec_ms + nsec_ms;
	if (total <= 0) {
		return 0;
	}
	if (total >= static_cast<long long>(std::numeric_limits<int>::max())) {
		return std::numeric_limits<int>::max();
	}
	return static_cast<int>(total);
}

}

[[nodiscard]] auto queue_open(Server& server) noexcept -> std::expected<void, WireError> {
	auto queue = Fd{::epoll_create1(EPOLL_CLOEXEC)};
	if (!queue.valid()) {
		return std::unexpected(wire_error(stage_queue, errno));
	}
	auto timer = Fd{::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC)};
	if (!timer.valid()) {
		return std::unexpected(wire_error(stage_queue, errno));
	}
	if (auto const code = arm_epoll(queue.get(), timer.get(), EPOLLIN | EPOLLONESHOT, retry_token); code != 0) {
		return std::unexpected(wire_error(stage_queue, code));
	}
	server.queue = std::move(queue);
	server.aux_timer = std::move(timer);
	return {};
}

auto queue_close(Server& server) noexcept -> void {
	if (server.signals_blocked) {
		std::ignore = ::sigprocmask(SIG_SETMASK, &server.saved_mask, nullptr);
		server.signals_blocked = false;
	}
}

[[nodiscard]] auto arm_read(Server& server, int fd, std::uint64_t token) noexcept -> std::int32_t {
	return arm_epoll(server.queue.get(), fd, EPOLLIN | EPOLLONESHOT, token);
}

[[nodiscard]] auto arm_write(Server& server, int fd, std::uint64_t token) noexcept -> std::int32_t {
	return arm_epoll(server.queue.get(), fd, EPOLLOUT | EPOLLONESHOT, token);
}

[[nodiscard]] auto arm_listen(Server& server, int fd, std::uint64_t token) noexcept -> std::int32_t {
	return arm_read(server, fd, token);
}

[[nodiscard]] auto arm_timer(Server& server, std::chrono::milliseconds delay, std::uint64_t token) noexcept -> std::int32_t {
	std::ignore = token;
	auto spec = ::itimerspec{};
	auto const count = delay.count();
	spec.it_value.tv_sec = represent_as<::time_t>(count / 1000);
	spec.it_value.tv_nsec = represent_as<long>((count % 1000) * 1'000'000);
	if (::timerfd_settime(server.aux_timer.get(), 0, &spec, nullptr) < 0) {
		return errno;
	}
	return arm_epoll(server.queue.get(), server.aux_timer.get(), EPOLLIN | EPOLLONESHOT, retry_token);
}

[[nodiscard]] auto arm_signals(Server& server) noexcept -> std::int32_t {
	auto mask = ::sigset_t{};
	if (::sigemptyset(&mask) < 0 || ::sigaddset(&mask, SIGINT) < 0 || ::sigaddset(&mask, SIGTERM) < 0) {
		return errno == 0 ? EINVAL : errno;
	}
	if (::sigprocmask(SIG_BLOCK, &mask, &server.saved_mask) < 0) {
		return errno;
	}
	server.signals_blocked = true;
	auto signals = Fd{::signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC)};
	if (!signals.valid()) {
		auto const code = errno;
		std::ignore = ::sigprocmask(SIG_SETMASK, &server.saved_mask, nullptr);
		server.signals_blocked = false;
		return code;
	}
	if (auto const code = arm_epoll(server.queue.get(), signals.get(), EPOLLIN, stop_token); code != 0) {
		std::ignore = ::sigprocmask(SIG_SETMASK, &server.saved_mask, nullptr);
		server.signals_blocked = false;
		return code;
	}
	server.aux_signal = std::move(signals);
	return 0;
}

[[nodiscard]] auto wait_events(Server& server, std::span<Event> out, std::optional<timespec> timeout) noexcept
    -> std::expected<std::size_t, std::int32_t> {
	auto events = std::array<::epoll_event, 64>{};
	auto const cap = std::min(out.size(), events.size());
	auto const count = ::epoll_pwait(server.queue.get(), events.data(), static_cast<int>(cap), timeout_milliseconds(timeout), nullptr);
	if (count < 0) {
		return std::unexpected(errno);
	}
	auto written = std::size_t{0};
	for (auto index = std::size_t{0}; index < static_cast<std::size_t>(count); ++index) {
		auto const& event = events[index];
		if (event.data.u64 == stop_token && server.aux_signal.valid()) {
			auto info = ::signalfd_siginfo{};
			auto got = false;
			while (::read(server.aux_signal.get(), &info, sizeof info) == static_cast<ssize_t>(sizeof info)) {
				got = true;
				if (written == out.size()) {
					break;
				}
				out[written] = Event{.token = stop_token, .ready = Ready::Read, .error = false, .error_code = 0};
				++written;
			}
			if (!got && written < out.size()) {
				out[written] = Event{.token = stop_token, .ready = Ready::Read, .error = false, .error_code = 0};
				++written;
			}
			continue;
		}
		if (event.data.u64 == retry_token && server.aux_timer.valid()) {
			auto expirations = std::uint64_t{0};
			std::ignore = ::read(server.aux_timer.get(), &expirations, sizeof expirations);
		}
		auto error_code = std::int32_t{0};
		auto const error = (event.events & (EPOLLERR | EPOLLHUP)) != 0;
		if (error && event.data.u64 == listener_token) {
			error_code = EIO;
			auto so_error = 0;
			auto length = ::socklen_t{sizeof so_error};
			if (::getsockopt(server.listener.get(), SOL_SOCKET, SO_ERROR, &so_error, &length) == 0) {
				error_code = so_error;
			}
		}
		auto ready = Ready::Read;
		if ((event.events & EPOLLOUT) != 0 && (event.events & EPOLLIN) == 0) {
			ready = Ready::Write;
		}
		if (written == out.size()) {
			break;
		}
		out[written] = Event{.token = event.data.u64, .ready = ready, .error = error, .error_code = error_code};
		++written;
	}
	return written;
}

[[nodiscard]] auto configure_connection(int fd) noexcept -> std::int32_t {
	return set_nonblocking(fd);
}

[[nodiscard]] auto accept_connection(int listener) noexcept -> int {
	return ::accept4(listener, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
}

[[nodiscard]] auto connection_write(int fd, std::span<char const> bytes) noexcept -> std::ptrdiff_t {
	return ::send(fd, bytes.data(), bytes.size(), MSG_NOSIGNAL);
}

}