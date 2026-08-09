export module A;

export struct Mask {
	unsigned bits;
	friend constexpr bool operator==(Mask const&, Mask const&) = default;
};
