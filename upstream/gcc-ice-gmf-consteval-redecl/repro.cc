module;
namespace stdexec {
  struct forwarding_query_t;
  extern forwarding_query_t const forwarding_query;
}
namespace stdexec {
  struct forwarding_query_t {
    consteval auto operator()(int) const noexcept -> bool { return true; }
  };
  inline constexpr forwarding_query_t forwarding_query{};
}
export module m;
