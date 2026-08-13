export module starter_testkit;

import std;

extern "C++" {
namespace starter::child_backend {

using WireError = std::array<std::int32_t, 2>;

struct Process;

struct ProcessDeleter {
	auto operator()(Process* process) const noexcept -> void;
};

using ProcessOwner = std::unique_ptr<Process, ProcessDeleter>;

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
}

namespace starter {

export enum class ChildStage : std::int32_t {
	Pipe = 1,
	Spawn = 2,
	Read = 3,
	Wait = 4,
	Signal = 5,
};

export struct [[nodiscard]] ChildError {
	ChildStage stage;
	std::int32_t code;

	[[nodiscard]] auto is_transient() const -> bool {
		return child_backend::err_transient(code);
	}

	[[nodiscard]] constexpr auto operator==(ChildError const&) const -> bool = default;
};

export class Child {
public:
	Child(Child&& other) noexcept = default;
	auto operator=(Child&& other) noexcept -> Child& = default;
	Child(Child const&) = delete;
	auto operator=(Child const&) -> Child& = delete;
	~Child() = default;

	[[nodiscard]] static auto spawn(std::string path, std::vector<std::string> args) -> std::expected<Child, ChildError> {
		auto opened = child_backend::process_spawn(path, args);
		if (!opened) {
			return std::unexpected(lift(opened.error()));
		}
		return Child{std::move(*opened)};
	}

	[[nodiscard]] auto read_line_until(std::chrono::steady_clock::time_point deadline) -> std::expected<std::string, ChildError> {
		for (;;) {
			if (auto const newline = pending_.find('\n'); newline != std::string::npos) {
				auto line = pending_.substr(0, newline);
				pending_.erase(0, newline + 1);
				if (!line.empty() && line.back() == '\r') {
					line.pop_back();
				}
				return line;
			}
			auto chunk = std::array<char, 512>{};
			auto const n = child_backend::process_read(*impl_, chunk, deadline);
			if (!n) {
				return std::unexpected(lift(n.error()));
			}
			if (*n == 0) {
				return std::unexpected(ChildError{.stage = ChildStage::Read, .code = child_backend::io_code()});
			}
			pending_.append(chunk.data(), *n);
		}
	}

	[[nodiscard]] auto read_all_until(std::chrono::steady_clock::time_point deadline) -> std::expected<std::string, ChildError> {
		auto output = std::move(pending_);
		pending_.clear();
		for (;;) {
			auto chunk = std::array<char, 4096>{};
			auto const n = child_backend::process_read(*impl_, chunk, deadline);
			if (!n) {
				return std::unexpected(lift(n.error()));
			}
			if (*n == 0) {
				return output;
			}
			output.append(chunk.data(), *n);
		}
	}

	[[nodiscard]] auto alive() const -> bool {
		return child_backend::process_alive(*impl_);
	}

	auto interrupt() -> void {
		std::ignore = child_backend::process_signal(*impl_, child_backend::interrupt_signal());
	}

	[[nodiscard]] auto wait_until(std::chrono::steady_clock::time_point deadline) -> std::expected<int, ChildError> {
		auto const status = child_backend::process_wait(*impl_, deadline);
		if (!status) {
			return std::unexpected(lift(status.error()));
		}
		return *status;
	}

private:
	explicit Child(child_backend::ProcessOwner owner) : impl_{std::move(owner)} {}

	[[nodiscard]] static auto lift(child_backend::WireError error) -> ChildError {
		return ChildError{.stage = static_cast<ChildStage>(error[0]), .code = error[1]};
	}

	child_backend::ProcessOwner impl_;
	std::string pending_{};
};

}
