#ifndef STARTER_CHILD_INTERNAL_H
#define STARTER_CHILD_INTERNAL_H

#include <array>
#include <chrono>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace starter::child_backend {

using WireError = std::array<std::int32_t, 2>;

inline constexpr std::int32_t stage_pipe = 1;
inline constexpr std::int32_t stage_spawn = 2;
inline constexpr std::int32_t stage_read = 3;
inline constexpr std::int32_t stage_wait = 4;
inline constexpr std::int32_t stage_signal = 5;

struct Process;

struct ProcessDeleter {
	auto operator()(Process* process) const noexcept -> void;
};

using ProcessOwner = std::unique_ptr<Process, ProcessDeleter>;

[[nodiscard]] constexpr auto wire_error(std::int32_t stage, std::int32_t code) -> WireError {
	return {stage, code};
}

[[nodiscard]] auto process_spawn(std::string const& path, std::vector<std::string> const& args) noexcept
    -> std::expected<ProcessOwner, WireError>;

[[nodiscard]] auto process_read(Process& process, std::span<char> buffer, std::chrono::steady_clock::time_point deadline) noexcept
    -> std::expected<std::size_t, WireError>;

[[nodiscard]] auto process_wait(Process& process, std::chrono::steady_clock::time_point deadline) noexcept
    -> std::expected<int, WireError>;

[[nodiscard]] auto process_alive(Process const& process) noexcept -> bool;

auto process_signal(Process& process, int sig) noexcept -> std::int32_t;

[[nodiscard]] auto err_transient(std::int32_t code) noexcept -> bool;
[[nodiscard]] auto interrupt_signal() noexcept -> int;
[[nodiscard]] auto timeout_code() noexcept -> std::int32_t;
[[nodiscard]] auto io_code() noexcept -> std::int32_t;

}

#endif
