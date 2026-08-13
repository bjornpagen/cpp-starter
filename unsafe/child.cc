#include "child.internal.h"

#include <array>
#include <cerrno>
#include <concepts>
#include <csignal>
#include <ctime>
#include <fcntl.h>
#include <limits>
#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>

extern char** environ;

namespace starter::child_backend {

namespace {

template<class To, class From>
[[nodiscard]] constexpr auto represent_as(From value) -> To {
	if constexpr (std::same_as<To, From>) {
		return value;
	} else {
		return static_cast<To>(value);
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

	auto release() noexcept -> int {
		return std::exchange(value_, -1);
	}

private:
	auto reset() noexcept -> void {
		if (value_ >= 0) {
			std::ignore = ::close(std::exchange(value_, -1));
		}
	}

	int value_ = -1;
};

[[nodiscard]] auto remaining_milliseconds(std::chrono::steady_clock::time_point deadline) noexcept -> int {
	auto const now = std::chrono::steady_clock::now();
	if (deadline <= now) {
		return 0;
	}
	auto const count = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
	if (count >= std::numeric_limits<int>::max()) {
		return std::numeric_limits<int>::max();
	}
	return represent_as<int>(count);
}

[[nodiscard]] auto cloexec(int fd) noexcept -> std::int32_t {
	auto const flags = ::fcntl(fd, F_GETFD);
	if (flags < 0 || ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC) < 0) {
		return errno;
	}
	return 0;
}

[[nodiscard]] auto exit_status(int status) noexcept -> int {
	if (WIFEXITED(status)) {
		return WEXITSTATUS(status);
	}
	if (WIFSIGNALED(status)) {
		return 128 + WTERMSIG(status);
	}
	return status;
}

}

struct Process {
	pid_t pid = -1;
	Fd out{};
	bool reaped = false;
};

[[nodiscard]] auto process_spawn(std::string const& path, std::vector<std::string> const& args) noexcept
    -> std::expected<ProcessOwner, WireError> {
	auto pipe_fds = std::array<int, 2>{-1, -1};
	if (::pipe(pipe_fds.data()) < 0) {
		return std::unexpected(wire_error(stage_pipe, errno));
	}
	auto reader = Fd{pipe_fds[0]};
	auto writer = Fd{pipe_fds[1]};
	if (auto const code = cloexec(reader.get()); code != 0) {
		return std::unexpected(wire_error(stage_pipe, code));
	}
	if (auto const code = cloexec(writer.get()); code != 0) {
		return std::unexpected(wire_error(stage_pipe, code));
	}

	posix_spawn_file_actions_t actions{};
	if (::posix_spawn_file_actions_init(&actions) != 0) {
		return std::unexpected(wire_error(stage_spawn, errno == 0 ? ENOMEM : errno));
	}

	auto fail = [&](std::int32_t code) -> std::expected<ProcessOwner, WireError> {
		std::ignore = ::posix_spawn_file_actions_destroy(&actions);
		return std::unexpected(wire_error(stage_spawn, code));
	};

	if (::posix_spawn_file_actions_adddup2(&actions, writer.get(), STDOUT_FILENO) != 0) {
		return fail(errno == 0 ? EINVAL : errno);
	}
	if (::posix_spawn_file_actions_adddup2(&actions, writer.get(), STDERR_FILENO) != 0) {
		return fail(errno == 0 ? EINVAL : errno);
	}
	if (::posix_spawn_file_actions_addclose(&actions, reader.get()) != 0) {
		return fail(errno == 0 ? EINVAL : errno);
	}
	if (::posix_spawn_file_actions_addclose(&actions, writer.get()) != 0) {
		return fail(errno == 0 ? EINVAL : errno);
	}
	if (::posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0) != 0) {
		return fail(errno == 0 ? EINVAL : errno);
	}

	auto argv_store = std::vector<std::string>{};
	argv_store.reserve(args.size() + 1);
	argv_store.push_back(path);
	for (auto const& arg : args) {
		argv_store.push_back(arg);
	}
	auto argv = std::vector<char*>{};
	argv.reserve(argv_store.size() + 1);
	for (auto& item : argv_store) {
		argv.push_back(item.data());
	}
	argv.push_back(nullptr);

	pid_t pid = -1;
	/* SAFETY: argv pointers address argv_store until posix_spawn returns */
	auto const rc = ::posix_spawn(&pid, path.c_str(), &actions, nullptr, argv.data(), environ);
	std::ignore = ::posix_spawn_file_actions_destroy(&actions);
	if (rc != 0) {
		return std::unexpected(wire_error(stage_spawn, rc));
	}

	auto process = ProcessOwner{new Process{}};
	process->pid = pid;
	process->out = std::move(reader);
	return process;
}

[[nodiscard]] auto process_read(Process& process, std::span<char> buffer, std::chrono::steady_clock::time_point deadline) noexcept
    -> std::expected<std::size_t, WireError> {
	if (process.out.get() < 0) {
		return std::unexpected(wire_error(stage_read, EBADF));
	}
	for (;;) {
		auto fd = process.out.get();
		auto pollfd = ::pollfd{.fd = fd, .events = POLLIN, .revents = 0};
		auto const ready = ::poll(&pollfd, 1, remaining_milliseconds(deadline));
		if (ready < 0) {
			if (errno == EINTR) {
				continue;
			}
			return std::unexpected(wire_error(stage_read, errno));
		}
		if (ready == 0) {
			return std::unexpected(wire_error(stage_read, ETIMEDOUT));
		}
		auto const n = ::read(fd, buffer.data(), buffer.size());
		if (n < 0) {
			if (errno == EINTR) {
				continue;
			}
			return std::unexpected(wire_error(stage_read, errno));
		}
		return represent_as<std::size_t>(n);
	}
}

[[nodiscard]] auto process_wait(Process& process, std::chrono::steady_clock::time_point deadline) noexcept
    -> std::expected<int, WireError> {
	if (process.reaped) {
		return std::unexpected(wire_error(stage_wait, ECHILD));
	}
	for (;;) {
		int status = 0;
		auto const pid = ::waitpid(process.pid, &status, WNOHANG);
		if (pid == process.pid) {
			process.reaped = true;
			return exit_status(status);
		}
		if (pid < 0) {
			if (errno == EINTR) {
				continue;
			}
			return std::unexpected(wire_error(stage_wait, errno));
		}
		if (remaining_milliseconds(deadline) == 0) {
			return std::unexpected(wire_error(stage_wait, ETIMEDOUT));
		}
		auto const pause = timespec{.tv_sec = 0, .tv_nsec = 20'000'000L};
		auto const slept = ::nanosleep(&pause, nullptr);
		if (slept < 0 && errno != EINTR) {
			return std::unexpected(wire_error(stage_wait, errno));
		}
	}
}

[[nodiscard]] auto process_alive(Process const& process) noexcept -> bool {
	if (process.reaped || process.pid <= 0) {
		return false;
	}
	if (::kill(process.pid, 0) == 0) {
		return true;
	}
	return errno == EPERM;
}

auto process_signal(Process& process, int sig) noexcept -> std::int32_t {
	if (process.reaped || process.pid <= 0) {
		return ESRCH;
	}
	if (::kill(process.pid, sig) == 0) {
		return 0;
	}
	return errno;
}

[[nodiscard]] auto err_transient(std::int32_t code) noexcept -> bool {
	return code == EINTR || code == EAGAIN || code == ETIMEDOUT;
}

[[nodiscard]] auto interrupt_signal() noexcept -> int {
	return SIGINT;
}

[[nodiscard]] auto timeout_code() noexcept -> std::int32_t {
	return ETIMEDOUT;
}

[[nodiscard]] auto io_code() noexcept -> std::int32_t {
	return EIO;
}

auto ProcessDeleter::operator()(Process* process) const noexcept -> void {
	if (process == nullptr) {
		return;
	}
	if (!process->reaped && process->pid > 0) {
		std::ignore = ::kill(process->pid, SIGKILL);
		int status = 0;
		std::ignore = ::waitpid(process->pid, &status, 0);
	}
	delete process;
}

}
