// Original 12-line reduction from #include <stdexec/execution.hpp> in a GMF,
// kept for provenance; repro.cc is the 4-line minimal form.
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
