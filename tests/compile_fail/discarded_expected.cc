// Compile-fail: discarding an expected is a swallowed failure (AGENTS.md
// §26 — return values are contracts). The call below must fail the build
// with the pinned nodiscard diagnostic; no discard spelling is exempt for
// an expected.
import std;
import starter;

auto main() -> int {
	starter::greeting("world");
	return 0;
}
