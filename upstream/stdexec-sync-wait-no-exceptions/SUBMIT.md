# stdexec: `sync_wait` misreports typed errors as stopped when exceptions are unavailable

- **Where:** pull request against https://github.com/NVIDIA/stdexec
- **Kind:** behavioral fix — **open the PR from the fork topic branch**
  `bjornpagen/stdexec` `fix/sync-wait-no-exceptions` @ `4c7a5667`
  (`gh pr create --repo NVIDIA/stdexec --head bjornpagen:fix/sync-wait-no-exceptions`)
- **Verified:** GCC 16.1.0 aarch64-apple-darwin24, pin
  `fd60b20af78bf108709bc97183b3941da569f68c`, both modes

## PR title

```
sync_wait: terminate instead of misreporting errors as stopped when exceptions are unavailable
```

## PR body (paste)

With exceptions disabled (`-fno-exceptions`), `sync_wait`'s receiver funnels
typed error completions through `std::make_exception_ptr`, whose non-null
paths require RTTI (fast path) or a `throw` (fallback) — so it returns a
null `exception_ptr`. The rethrow site's null guard then silently skips,
and `sync_wait` returns an empty `optional`: the *stopped* spelling. A
sender that completed with `set_error(E)` is indistinguishable from one
that was cancelled, with zero diagnostics.

Repro: the topic-branch commit carries the verification matrix — a chain
completing with a typed error returns `nullopt` under
`-fno-exceptions -fno-rtti` (silently, as if stopped) at HEAD; with the fix
it terminates; the same TU built with exceptions rethrows identically
before and after.

This change routes the error channel to `STDEXEC_TERMINATE()` when
`STDEXEC_NO_STDCPP_EXCEPTIONS()` — matching the library's existing
convention (`STDEXEC_THROW` already degrades to `__terminate()` in this
mode). With exceptions enabled, behavior is unchanged (verified: the error
is rethrown and catchable exactly as before). A `static_assert` was
considered and rejected: error completion signatures are routinely present
but never taken, so compile-time rejection would make `sync_wait`
unusable in exceptions-off builds rather than safe.
