# stdexec: duplicate `inline` in the ARM branch of `__spin_loop_pause.hpp`

- **Where:** pull request against https://github.com/NVIDIA/stdexec
- **Kind:** 1-line fix — **open the PR from the fork topic branch**
  `bjornpagen/stdexec` `fix/arm64-duplicate-inline` @ `9d586ca0`
  (`gh pr create --repo NVIDIA/stdexec --head bjornpagen:fix/arm64-duplicate-inline`);
  the commit carries the full rationale. The attached `.patch` is the same
  change kept for reference.
- **Verified:** GCC 16.1.0 aarch64-apple-darwin24 (hard error at upstream
  HEAD, clean with fix) AND Clang 22 (warn-only, which is why upstream arm
  CI never caught it), pin `fd60b20af78bf108709bc97183b3941da569f68c`

## PR title

```
Remove duplicate 'inline' from the ARM __spin_loop_pause
```

## PR body (paste)

Since f0e8ae6f ("Create a stdexec module", #2138), the ARM branch of
`include/stdexec/__detail/__spin_loop_pause.hpp` spells

```cpp
STDEXEC_ATTRIBUTE(always_inline) inline void __spin_loop_pause() noexcept
```

`STDEXEC_ATTRIBUTE(always_inline)` already expands to
`__attribute__((__always_inline__, __artificial__)) inline` on GCC/Clang
(`__config.hpp`, `STDEXEC_ATTRIBUTE_CASE_ALWAYS_INLINE`), so the explicit
`inline` produces `inline inline`, which GCC rejects:

```
include/stdexec/__detail/__spin_loop_pause.hpp:42:36: error: duplicate 'inline'
```

Every ARM target compiling with GCC is broken by this; the x86 branch is
unaffected because it says `static` instead. The fix drops the explicit
`inline`, matching what the macro already provides.

Repro (any aarch64 host, GCC):

```sh
g++ -std=c++26 -I include -c repro.cc   # repro.cc: includes the header, calls __spin_loop_pause()
```
