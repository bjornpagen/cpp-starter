export module starter:handle;

import std;

namespace starter {

export inline constexpr std::uint64_t handle_index_bits = 8;
export inline constexpr std::uint64_t handle_index_mask = (std::uint64_t{1} << handle_index_bits) - 1;
export inline constexpr std::uint64_t handle_max_generation = std::numeric_limits<std::uint64_t>::max() >> handle_index_bits;

export struct ConnectionHandle {
	std::uint64_t bits{};

	[[nodiscard]] constexpr auto operator==(ConnectionHandle const&) const -> bool = default;
};

template<class To, class From>
[[nodiscard]] constexpr auto represent_as(From value) -> To {
	if constexpr (std::same_as<To, From>) {
		return value;
	} else {
		return static_cast<To>(value);
	}
}

export [[nodiscard]] constexpr auto pack_connection_handle(std::size_t index, std::uint64_t generation) -> ConnectionHandle {
	return ConnectionHandle{.bits = (generation << handle_index_bits) | represent_as<std::uint64_t>(index)};
}

export [[nodiscard]] constexpr auto connection_index(ConnectionHandle handle) -> std::size_t {
	return represent_as<std::size_t>(handle.bits & handle_index_mask);
}

export [[nodiscard]] constexpr auto connection_generation(ConnectionHandle handle) -> std::uint64_t {
	return handle.bits >> handle_index_bits;
}

/**
 * Bounded slot map. A looked-up reference is valid only for the call that
 * obtained it and must not be stored. Generation exhaustion retires the slot.
 */
export template<class T, std::size_t Capacity, std::uint8_t GenerationBits>
class ConnectionTable {
public:
	static_assert(Capacity > 0);
	static_assert(Capacity <= (std::size_t{1} << handle_index_bits));
	static_assert(GenerationBits >= 2);
	static_assert(GenerationBits <= 56);

	[[nodiscard]] auto insert(T value) -> std::optional<ConnectionHandle> {
		for (auto index = std::size_t{0}; index < Capacity; ++index) {
			auto& slot = slots_[index];
			if (slot.retired || slot.occupied) {
				continue;
			}
			slot.value = std::move(value);
			slot.occupied = true;
			return pack(index, slot.generation);
		}
		return std::nullopt;
	}

	[[nodiscard]] auto try_get(ConnectionHandle handle) -> std::optional<T&> {
		auto const index = unpack_index(handle);
		if (index >= Capacity) {
			return std::nullopt;
		}
		auto& slot = slots_[index];
		if (!slot.occupied || slot.retired || slot.generation != unpack_generation(handle)) {
			return std::nullopt;
		}
		return slot.value;
	}

	[[nodiscard]] auto try_get(ConnectionHandle handle) const -> std::optional<T const&> {
		auto const index = unpack_index(handle);
		if (index >= Capacity) {
			return std::nullopt;
		}
		auto const& slot = slots_[index];
		if (!slot.occupied || slot.retired || slot.generation != unpack_generation(handle)) {
			return std::nullopt;
		}
		return slot.value;
	}

	[[nodiscard]] auto get(ConnectionHandle handle) -> T& {
		auto found = try_get(handle);
		contract_assert(found.has_value());
		return *found;
	}

	auto release(ConnectionHandle handle) -> void {
		auto found = try_get(handle);
		contract_assert(found.has_value());
		auto const index = unpack_index(handle);
		auto& slot = slots_[index];
		slot.occupied = false;
		if (slot.generation >= generation_limit) {
			slot.retired = true;
			return;
		}
		++slot.generation;
	}

	[[nodiscard]] auto retired_count() const -> std::size_t {
		auto count = std::size_t{0};
		for (auto const& slot : slots_) {
			if (slot.retired) {
				++count;
			}
		}
		return count;
	}

private:
	static constexpr std::uint64_t generation_limit = (std::uint64_t{1} << GenerationBits) - 1;
	static constexpr std::uint64_t index_bits = handle_index_bits;

	struct Slot {
		T value{};
		std::uint64_t generation = 1;
		bool occupied = false;
		bool retired = false;
	};

	[[nodiscard]] static constexpr auto pack(std::size_t index, std::uint64_t generation) -> ConnectionHandle {
		return ConnectionHandle{.bits = (generation << index_bits) | represent_as<std::uint64_t>(index)};
	}

	[[nodiscard]] static constexpr auto unpack_index(ConnectionHandle handle) -> std::size_t {
		return represent_as<std::size_t>(handle.bits & handle_index_mask);
	}

	[[nodiscard]] static constexpr auto unpack_generation(ConnectionHandle handle) -> std::uint64_t {
		return handle.bits >> index_bits;
	}

	std::array<Slot, Capacity> slots_{};
};

}