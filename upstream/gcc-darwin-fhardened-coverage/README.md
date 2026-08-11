# `-fhardened` on aarch64-apple-darwin: unsupported-target warning is unclassifiable, umbrella silently partial

Status: analysis complete, reproduction verified. Check Linux before filing (a Linux run is being arranged separately; on Linux the target gate never fires, so Linux serves as the control that the umbrella applies fully there).

Not a memory issue: every command here compiles a one-line file and finishes in under a second. All commands were still run under the standard guard (`/tmp/buildguard.sh 8192 12288 600 ...`) per house policy; the guard never fired.

## Symptom

Three separable defects, all from one option on one target:

1. `gcc/configure.ac` hardcodes `-fhardened` support to `linux* | gnu*` targets. On `aarch64-apple-darwin24` the constituents demonstrably work — `-fstack-protector-strong`, `-fstack-clash-protection`, `-ftrivial-auto-var-init=zero`, and `_GLIBCXX_ASSERTIONS` are all verified working below — so the blanket "not supported" is an enablement gap, not a codegen truth.
2. The rejection is applied inconsistently. Option finishing (`gcc/opts.cc`) enables `-fstack-protector-strong` and `-ftrivial-auto-var-init=zero` from `flag_hardened` *before* `process_options` (`gcc/toplev.cc`) warns and zeroes `flag_hardened`; the stack-clash enablement later in `toplev.cc` and the `_FORTIFY_SOURCE`/`_GLIBCXX_ASSERTIONS` macro injection in `c-family/c-opts.cc` test the already-zeroed flag and are silently dropped. The user gets an undocumented half of the umbrella, and `-Whardened` prints zero per-constituent skip messages.
3. The "not supported for this target" warning has warning class 0 (no controlling option), so `-Wno-hardened` does not silence it and `-Wno-error=hardened` does not demote it. Under `-Werror` it becomes a hard error with no `[-W...]` tag and no targeted escape: any hardened-by-default flag injection into a `-Werror` build fails outright.

Side fact for the report: C++ FORTIFY is independently dead in the macOS SDK — `MacOSX.sdk/usr/include/_string.h:226` gates the fortified declarations with `#if defined (__GNUC__) && _FORTIFY_SOURCE > 0 && !defined (__cplusplus)`. The right Darwin enablement therefore turns on the non-FORTIFY constituents and reports the FORTIFY skip accurately through `-Whardened`, rather than claiming full support.

## Environment

- GCC 16.1.0, self-built, `/Users/bjorn/.gcc/current`, target `aarch64-apple-darwin24`
- macOS 15.7.7 (24G720), Apple Silicon (M2 Max), 96 GB RAM
- macOS SDK: MacOSX.sdk from Xcode (`/Applications/Xcode.app/.../MacOSX.sdk`)
- CMake 4.2.1, Ninja 1.13.2 (not needed by the reproduction)
- Source cites are against the release tree `/Users/bjorn/.gcc/build/gcc-16.1.0`

## Files

- `hardened.cc` — one-line minimal reproduction (`int main(){return 0;}`)
- `constituents.cc` — the three codegen constituents spelled directly, for the "works on Darwin" evidence
- `assertions.cc` — `_GLIBCXX_ASSERTIONS` runtime evidence (out-of-bounds `operator[]` must abort)

## Reproduction

### 1. Unclassifiable diagnostic, no `-Werror` escape

```sh
g++-16 -fhardened -Whardened -Werror -c hardened.cc
```

```text
cc1plus: error: '-fhardened' not supported for this target [-Werror]
cc1plus: all warnings being treated as errors
```

Exit 1. Note the bare `[-Werror]` with no warning name. The targeted demotion is ineffective — adding `-Wno-error=hardened` produces the identical error and exit 1. Suppression is equally ineffective — without `-Werror`, `-Wno-hardened` still prints the warning:

```sh
g++-16 -fhardened -Wno-hardened -c hardened.cc
```

```text
cc1plus: warning: '-fhardened' not supported for this target
```

Exit 0. Expected: the warning is in the `-Whardened` class (its own option, documented as the umbrella's reporting channel), so `-Wno-hardened` silences it and `-Wno-error=hardened` survives `-Werror`.

### 2. Silent partial application

```sh
g++-16 -fhardened -O2 -E -dM -x c++ /dev/null | grep -E 'SSP|FORTIFY|GLIBCXX'
```

```text
#define __SSP_STRONG__ 3
```

`__SSP_STRONG__ 3` proves `-fstack-protector-strong` was applied from the "unsupported" umbrella; `_FORTIFY_SOURCE` and `_GLIBCXX_ASSERTIONS` are absent (a plain `-O2` run defines none of the three). `-Q --help=common` with and without `-fhardened` on the same compile confirms the split — the diff shows exactly:

```text
-fhardened                    [disabled] -> [enabled]
-fstack-protector-strong      [disabled] -> [enabled]
-ftrivial-auto-var-init=...   uninitialized -> zero
```

while `-fstack-clash-protection` reads `[disabled]` in both runs. So two of the five compile-side constituents engage (`-fstack-protector-strong`, `-ftrivial-auto-var-init=zero`) and three are dropped (`-fstack-clash-protection`, `_FORTIFY_SOURCE`, `_GLIBCXX_ASSERTIONS`), and `-Whardened` (present in every command above) never says which. Expected: either none engage on a rejected target, or the survivors and casualties are each reported.

### 3. The constituents work on this target

```sh
g++-16 -O2 -fstack-protector-strong -fstack-clash-protection \
  -ftrivial-auto-var-init=zero -Werror -S constituents.cc -o constituents.s
```

Exit 0, no diagnostics. The assembly contains 15 `___stack_chk_guard` references; the 128 KiB frame in `clash()` is built as interleaved page probes (`sub sp, sp, #65536` / `str xzr, [sp, 1024]` pairs); `autoinit()` zero-initializes its local (`str wzr, [x29, -12]`) before the escaping call.

```sh
g++-16 -O2 -D_GLIBCXX_ASSERTIONS -Werror assertions.cc -o assertions && ./assertions
```

```text
.../bits/stl_vector.h:1253: ... std::vector<...>::operator[](size_type) ...: Assertion '__n < this->size()' failed.
```

Exit 134 (SIGABRT), as hardening requires. Every non-FORTIFY constituent of `-fhardened` is therefore functional on `aarch64-apple-darwin24`; only the SDK-side C++ FORTIFY path is genuinely unavailable (see the `_string.h:226` gate above).

## Analysis

Three cooperating causes, in pipeline order:

1. **Configure gate** — `gcc/configure.ac:7982-7995`: `case $target_os in linux* | gnu*) fhardened_support=yes ;; *) fhardened_support=no ;; esac`, comment "`-fhardened` is only supported on GNU/Linux". `HAVE_FHARDENED_SUPPORT` becomes 0 on Darwin regardless of constituent availability.
2. **Ordering** — `finish_options` (`gcc/opts.cc:1165-1224`) runs first and converts `flag_hardened` into `flag_auto_var_init = AUTO_INIT_ZERO` and `flag_stack_protect = SPCT_FLAG_STRONG` (setting `flag_stack_protector_set_by_fhardened_p`). Only later does `process_options` (`gcc/toplev.cc:1642-1646`) hit `if (flag_hardened && !HAVE_FHARDENED_SUPPORT)`, warn, and zero `flag_hardened` — without undoing the two constituents already applied. Everything downstream of the zeroing then sees `flag_hardened == 0`: the stack-clash enablement at `toplev.cc:1658` (`else if (flag_hardened)`) and the `_FORTIFY_SOURCE`/`_GLIBCXX_ASSERTIONS` injection in `c_finish_options` (`gcc/c-family/c-opts.cc:1747-1768`) are skipped with no diagnostic. The skip messages that do exist in those blocks are all conditioned on *other* conflicts (user `-D`, `-O0`, `-fstack-check`), so none fire.
3. **Warning class** — the rejection at `toplev.cc:1643-1645` is `warning_at (UNKNOWN_LOCATION, 0, ...)`. Class 0 diagnostics cannot be suppressed or demoted by name; contrast the neighboring `-fhardened`-related warnings in the same file, which correctly use `OPT_Whardened`. Under `-Werror` the only escapes are dropping `-fhardened` or blanket `-Wno-error` — there is no way to say "I know this target is not on the list, keep the partial hardening and continue".

The minimal fix regardless of Darwin policy: move the rejection into the `OPT_Whardened` class, and make the rejection either happen before `finish_options` applies constituents (full rejection) or report each dropped constituent (honest partial application). The better Darwin fix: extend the `configure.ac` gate to `*-*-darwin*`, keep FORTIFY out of the Darwin injection (the SDK's `!defined (__cplusplus)` gate makes it a no-op for C++ anyway), and let `-Whardened` state that FORTIFY is unavailable on this target.

## Suggested upstream destination

1. GCC Bugzilla, product `gcc`, component `middle-end` (option handling; `-fhardened` lives in the common options machinery — triage may split the Darwin enablement into `target`), version `16.1.0`, keyword `diagnostic`. Title suggestion: `-fhardened on unsupported targets: warning not in -Whardened class (unfixable under -Werror), umbrella applied partially with no per-constituent report`. Attach `hardened.cc` and the verbatim outputs above. Lead with defects 2 and 3 (target-independent for every non-`linux*|gnu*` OS); present the reproduction on `aarch64-apple-darwin24`.
2. The Darwin enablement itself is a feature gap, not a bug: a follow-up `configure.ac` patch to gcc-patches (CC the Darwin maintainers, per the `gcc-fixincludes-darwin-rsize-t` precedent) once the Bugzilla report has a PR number to cite. The constituent evidence in `constituents.cc`/`assertions.cc` is the justification; the `_string.h:226` FORTIFY gate is why the patch must not inject `_FORTIFY_SOURCE` claims for Darwin C++.

Before filing: confirm on Linux that `-fhardened -Whardened -Werror` is clean and enables all constituents there (control), and check whether trunk has since reclassified the warning or reordered the rejection.

## Local workaround

The repository never uses `-fhardened`: the language profile in `CMakeLists.txt` spells the working constituents individually (`-ftrivial-auto-var-init=zero`, `-fstack-protector-strong`, `-fzero-call-used-regs=used-gpr`, GNU-scoped `-fstack-clash-protection`), which is exactly the set the umbrella would silently truncate. See `PINS.md`.
