# `-fhardened` on aarch64-apple-darwin: unsupported-target warning is unclassifiable, umbrella silently partial

Status: the analysis is complete, and the reproduction is verified. The Linux control is also complete: on aarch64-unknown-linux-gnu (same self-built GCC 16.1.0), `g++ -fhardened -Whardened -O2` compiles with exit 0 and no warning, and `-E -dM` shows `_FORTIFY_SOURCE 3` and `__SSP_STRONG__ 3` defined. The umbrella applies fully on Linux. The report is ready to file.

This is not a memory issue. Each command here compiles a one-line file and completes in less than one second. We still ran all commands under the standard guard (`/tmp/buildguard.sh 8192 12288 600 ...`) to obey house policy. The guard never fired.

## Symptom

One option on one target causes three separable defects:

1. `gcc/configure.ac` hardcodes `-fhardened` support to `linux* | gnu*` targets. On `aarch64-apple-darwin24`, the constituents work. The tests below verify that `-fstack-protector-strong`, `-fstack-clash-protection`, `-ftrivial-auto-var-init=zero`, and `_GLIBCXX_ASSERTIONS` all work. Thus the blanket "not supported" message shows an enablement gap, not a codegen fact.
2. GCC applies the rejection inconsistently. Option finishing (`gcc/opts.cc`) enables `-fstack-protector-strong` and `-ftrivial-auto-var-init=zero` from `flag_hardened` *before* `process_options` (`gcc/toplev.cc`) warns and zeroes `flag_hardened`. The later stack-clash enablement in `toplev.cc` and the `_FORTIFY_SOURCE`/`_GLIBCXX_ASSERTIONS` macro injection in `c-family/c-opts.cc` test the flag after it is zero. Thus GCC silently drops these constituents. The user gets half of the umbrella, and no documentation tells the user this. `-Whardened` prints zero per-constituent skip messages.
3. The "not supported for this target" warning has warning class 0 (no controlling option). Thus `-Wno-hardened` does not silence it, and `-Wno-error=hardened` does not demote it. Under `-Werror`, the warning becomes a hard error with no `[-W...]` tag and no targeted escape. If a build injects hardened-by-default flags and also uses `-Werror`, that build always fails.

Side fact for the report: C++ FORTIFY is dead in the macOS SDK for an independent reason. `MacOSX.sdk/usr/include/_string.h:226` gates the fortified declarations with `#if defined (__GNUC__) && _FORTIFY_SOURCE > 0 && !defined (__cplusplus)`. Thus the correct Darwin enablement turns on the non-FORTIFY constituents. It also reports the FORTIFY skip accurately through `-Whardened`. It does not claim full support.

## Environment

- GCC 16.1.0, self-built, `/Users/bjorn/.gcc/current`, target `aarch64-apple-darwin24`
- macOS 15.7.7 (24G720), Apple Silicon (M2 Max), 96 GB RAM
- macOS SDK: MacOSX.sdk from Xcode (`/Applications/Xcode.app/.../MacOSX.sdk`)
- CMake 4.2.1, Ninja 1.13.2 (the reproduction does not need them)
- Source line references point to the release tree `/Users/bjorn/.gcc/build/gcc-16.1.0`

## Files

- `hardened.cc` — a one-line minimal reproduction (`int main(){return 0;}`)
- `constituents.cc` — spells the three codegen constituents directly; this gives the "works on Darwin" evidence
- `assertions.cc` — gives the `_GLIBCXX_ASSERTIONS` runtime evidence (an out-of-bounds `operator[]` must abort)

## Reproduction

### 1. Unclassifiable diagnostic, no `-Werror` escape

```sh
g++-16 -fhardened -Whardened -Werror -c hardened.cc
```

```text
cc1plus: error: '-fhardened' not supported for this target [-Werror]
cc1plus: all warnings being treated as errors
```

The exit code is 1. Note the bare `[-Werror]` tag with no warning name. The targeted demotion has no effect. When you add `-Wno-error=hardened`, the compiler gives the identical error and exit code 1. Suppression also has no effect. Without `-Werror`, `-Wno-hardened` still prints the warning:

```sh
g++-16 -fhardened -Wno-hardened -c hardened.cc
```

```text
cc1plus: warning: '-fhardened' not supported for this target
```

The exit code is 0. Expected behavior: the warning is in the `-Whardened` class, because that class has its own option and the documentation names it as the umbrella's reporting channel. Then `-Wno-hardened` silences the warning, and `-Wno-error=hardened` survives `-Werror`.

### 2. Silent partial application

```sh
g++-16 -fhardened -O2 -E -dM -x c++ /dev/null | grep -E 'SSP|FORTIFY|GLIBCXX'
```

```text
#define __SSP_STRONG__ 3
```

`__SSP_STRONG__ 3` proves that the "unsupported" umbrella applied `-fstack-protector-strong`. `_FORTIFY_SOURCE` and `_GLIBCXX_ASSERTIONS` are absent. (A plain `-O2` run defines none of the three macros.) A comparison of `-Q --help=common` output, with and without `-fhardened` on the same compile, confirms the split. The diff shows exactly:

```text
-fhardened                    [disabled] -> [enabled]
-fstack-protector-strong      [disabled] -> [enabled]
-ftrivial-auto-var-init=...   uninitialized -> zero
```

In both runs, `-fstack-clash-protection` reads `[disabled]`. Thus two of the five compile-side constituents engage: `-fstack-protector-strong` and `-ftrivial-auto-var-init=zero`. GCC drops the other three: `-fstack-clash-protection`, `_FORTIFY_SOURCE`, and `_GLIBCXX_ASSERTIONS`. `-Whardened` is present in each command above, but it never says which constituents GCC dropped. Expected behavior is one of these two:

1. On a rejected target, no constituents engage.
2. GCC reports each constituent that engaged and each constituent that it dropped.

### 3. The constituents work on this target

```sh
g++-16 -O2 -fstack-protector-strong -fstack-clash-protection \
  -ftrivial-auto-var-init=zero -Werror -S constituents.cc -o constituents.s
```

The exit code is 0, with no diagnostics. The assembly shows three results:

- The assembly contains 15 `___stack_chk_guard` references.
- The compiler builds the 128 KiB frame in `clash()` as interleaved page probes (`sub sp, sp, #65536` / `str xzr, [sp, 1024]` pairs).
- `autoinit()` zero-initializes its local variable (`str wzr, [x29, -12]`) before the escaping call.

```sh
g++-16 -O2 -D_GLIBCXX_ASSERTIONS -Werror assertions.cc -o assertions && ./assertions
```

```text
.../bits/stl_vector.h:1253: ... std::vector<...>::operator[](size_type) ...: Assertion '__n < this->size()' failed.
```

The exit code is 134 (SIGABRT), which hardening requires. Thus each non-FORTIFY constituent of `-fhardened` is functional on `aarch64-apple-darwin24`. Only the SDK-side C++ FORTIFY path is genuinely unavailable (see the `_string.h:226` gate above).

## Analysis

Three causes operate together. In pipeline order:

1. **Configure gate** — `gcc/configure.ac:7982-7995`: `case $target_os in linux* | gnu*) fhardened_support=yes ;; *) fhardened_support=no ;; esac`. The comment there says "`-fhardened` is only supported on GNU/Linux". On Darwin, `HAVE_FHARDENED_SUPPORT` becomes 0. The availability of the constituents has no effect on this result.
2. **Ordering** — `finish_options` (`gcc/opts.cc:1165-1224`) runs first. It converts `flag_hardened` into `flag_auto_var_init = AUTO_INIT_ZERO` and `flag_stack_protect = SPCT_FLAG_STRONG`, and it sets `flag_stack_protector_set_by_fhardened_p`. Only later does `process_options` (`gcc/toplev.cc:1642-1646`) reach `if (flag_hardened && !HAVE_FHARDENED_SUPPORT)`. There it warns and zeroes `flag_hardened`, but it does not undo the two constituents that `finish_options` applied. Each check downstream of the zeroing then sees `flag_hardened == 0`. Thus the compiler skips the stack-clash enablement at `toplev.cc:1658` (`else if (flag_hardened)`), and it skips the `_FORTIFY_SOURCE`/`_GLIBCXX_ASSERTIONS` injection in `c_finish_options` (`gcc/c-family/c-opts.cc:1747-1768`). It gives no diagnostic for these skips. Those blocks do contain skip messages, but each message has a condition on a different conflict (user `-D`, `-O0`, `-fstack-check`). Thus none of these messages fire.
3. **Warning class** — the rejection at `toplev.cc:1643-1645` is `warning_at (UNKNOWN_LOCATION, 0, ...)`. You cannot suppress or demote a class-0 diagnostic by name. Contrast the adjacent `-fhardened`-related warnings in the same file: those warnings correctly use `OPT_Whardened`. Under `-Werror`, only two escapes exist: remove `-fhardened`, or use the blanket `-Wno-error`. No option lets you say "I know this target is not on the list; keep the partial hardening and continue".

The minimal fix applies regardless of Darwin policy:

1. Move the rejection into the `OPT_Whardened` class.
2. Then do one of these two things: make the rejection occur before `finish_options` applies constituents (full rejection), or report each dropped constituent (honest partial application).

The better Darwin fix:

1. Extend the `configure.ac` gate to `*-*-darwin*`.
2. Keep FORTIFY out of the Darwin injection. (The SDK's `!defined (__cplusplus)` gate makes FORTIFY a no-op for C++ in all cases.)
3. Let `-Whardened` state that FORTIFY is unavailable on this target.

## Suggested upstream destination

1. File in GCC Bugzilla: product `gcc`, component `middle-end` (option handling; `-fhardened` lives in the common options machinery — triage may split the Darwin enablement into `target`), version `16.1.0`, keyword `diagnostic`. Title suggestion: `-fhardened on unsupported targets: warning not in -Whardened class (unfixable under -Werror), umbrella applied partially with no per-constituent report`. Attach `hardened.cc` and the verbatim outputs above. Lead with defects 2 and 3, because they are target-independent for each non-`linux*|gnu*` OS. Present the reproduction on `aarch64-apple-darwin24`.
2. The Darwin enablement itself is a feature gap, not a bug. Send a follow-up `configure.ac` patch to gcc-patches after the Bugzilla report has a PR number to cite. CC the Darwin maintainers, per the `gcc-fixincludes-darwin-rsize-t` precedent. The constituent evidence in `constituents.cc`/`assertions.cc` is the justification. The `_string.h:226` FORTIFY gate is the reason the patch must not inject `_FORTIFY_SOURCE` claims for Darwin C++.

Before you file the report, do these two checks:

1. Confirm on Linux (the control) that `-fhardened -Whardened -Werror` is clean and enables all constituents there.
2. Check trunk for a later change that reclassifies the warning or reorders the rejection.

## Local workaround

The repository never uses `-fhardened`. The language profile in `CMakeLists.txt` spells the working constituents individually: `-ftrivial-auto-var-init=zero`, `-fstack-protector-strong`, `-fzero-call-used-regs=used-gpr`, and GNU-scoped `-fstack-clash-protection`. This set is exactly the set that the umbrella would silently truncate. See `PINS.md`.
