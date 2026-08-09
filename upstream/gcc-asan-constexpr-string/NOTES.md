# NOT YET REDUCED — do not file as-is

## Observed behavior (real, in production tree)

GCC 16.1.0 with `-fsanitize=address,undefined`: inside consteval
evaluation, `std::string`'s (pointer, size) constructor fails to
constant-fold against certain storage (the null-pointer check does not
evaluate against ASan-instrumented objects), while the iterator-pair
constructor folds fine over the same storage. In the production tree this
broke every reflected member name computed via `data_member_spec` under
the sanitizer presets, until all such names were routed through the
iterator-pair constructor.

## Where it lives

- Pin + workaround: `bumbledb` repo, `cpp/src/relation/name.cc`
  (`detail::spec_name` — comment documents the failing storage classes:
  template parameter objects, string literals, `define_static_string`
  globals).
- Faithful reproduction: in the bumbledb tree, change `spec_name` to
  `return std::string(text.data(), text.size());` and build the
  `asan-ubsan` preset — the reflected-name consteval evaluations fail.

## Reduction attempts in this directory (all compile CLEAN under ASan — none reproduce)

- `t.cc`: namespace-scope `constexpr char[]` through both constructors.
- `t2.cc`: template-parameter-object (structural NTTP with char array)
  through both constructors.
- `t3.cc`: `std::define_static_string` storage through both constructors.

The isolated shapes fold; the production failure evidently needs the
fuller context (names flowing into `std::meta::data_member_spec` inside
class-scope consteval blocks, under the full flag set). Next reduction
step: extract the actual failing TU from bumbledb's asan preset at the
pre-workaround revision and shrink from there.

Judgment call to make before filing: if it reduces, this is likely a real
GCC bug (constant evaluation must not observe sanitizer instrumentation);
file under component `sanitizer` with the c++ front end CC'd. Do not file
on the current evidence.
