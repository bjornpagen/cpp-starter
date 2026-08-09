module;
#include <expected>
export module Q;

export namespace q {
struct handle {
	int fd;
};

inline auto open(int fd) -> std::expected<handle, int> {
	return handle{fd};
}
} // namespace q
