import std;
import starter;

namespace {

[[nodiscard]] auto code(std::errc value) -> std::int32_t {
	return static_cast<std::int32_t>(value);
}

}

auto main() -> int {
	auto failures = 0;
	auto expect = [&](bool condition, std::string_view label) {
		if (!condition) {
			std::println("FAIL: {}", label);
			++failures;
		}
	};

	using starter::NetError;
	using starter::NetStage;

	expect(NetError{NetStage::QueueCreate, code(std::errc::interrupted)}.is_transient(), "EINTR transient");
	expect(NetError{NetStage::Accept, code(std::errc::resource_unavailable_try_again)}.is_transient(), "EAGAIN transient");
	expect(NetError{NetStage::Accept, code(std::errc::connection_reset)}.is_transient(), "ECONNRESET transient");
	expect(NetError{NetStage::Accept, code(std::errc::connection_aborted)}.is_transient(), "ECONNABORTED transient");
	expect(NetError{NetStage::Accept, code(std::errc::broken_pipe)}.is_transient(), "EPIPE transient");
	expect(NetError{NetStage::Accept, code(std::errc::timed_out)}.is_transient(), "ETIMEDOUT transient");
	expect(NetError{NetStage::Accept, code(std::errc::too_many_files_open)}.is_transient(), "EMFILE transient");
	expect(NetError{NetStage::Accept, code(std::errc::not_enough_memory)}.is_transient(), "ENOMEM transient");
	expect(NetError{NetStage::SocketBind, code(std::errc::address_in_use)}.is_transient(), "EADDRINUSE transient");

	expect(!NetError{NetStage::Accept, code(std::errc::bad_file_descriptor)}.is_transient(), "EBADF permanent");
	expect(!NetError{NetStage::Accept, code(std::errc::invalid_argument)}.is_transient(), "EINVAL permanent");
	expect(!NetError{NetStage::SocketBind, code(std::errc::permission_denied)}.is_transient(), "EACCES permanent");
	expect(!NetError{NetStage::Accept, 999999}.is_transient(), "unknown errno permanent");
	expect(!NetError{NetStage::Accept, 0}.is_transient(), "zero errno permanent");

	auto const invalid = starter::open_http(starter::HttpServerConfig{
	    .port = 0,
	    .max_connections = 0,
	    .request_timeout = std::chrono::seconds{1},
	    .response_timeout = std::chrono::seconds{1},
	});
	expect(!invalid && invalid.error().stage == NetStage::Configuration, "zero connection capacity rejected");

	if (failures != 0) {
		return 1;
	}
	std::println("pass: net classification");
	return 0;
}
