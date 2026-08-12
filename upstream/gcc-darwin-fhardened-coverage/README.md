# `-fhardened` unsupported-target diagnostics

Status: the analysis and reproduction are complete. [GCC PR 126822](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126822) covers the warning-class problem and awaits triage. [GCC PR 126823](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126823) covered partial behavior on Darwin. A maintainer said that PR 126823 works as designed because `-fhardened` is supported only on GNU/Linux. We closed that work locally and will not propose Darwin enablement or partial-application patches.

The Linux control is complete: on aarch64-unknown-linux-gnu (same self-built GCC 16.1.0), `g++ -fhardened -Whardened -O2` compiles with exit 0 and no warning, and `-E -dM` shows `_FORTIFY_SOURCE 3` and `__SSP_STRONG__ 3` defined. The umbrella applies fully on Linux. Trunk source at commit 475e9eff retains the documented target gate.

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

## Related reports

We verified each reference below on 2026-08-11. We used the GCC Bugzilla REST API and the inbox.sourceware.org gcc-patches archive.

- [PR 117967](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=117967) (driver, NEW) — "`-Wno-hardened` does not disable all `-fhardened` warnings". This is the nearest existing report. It covers the class-0 `-fhardened` warning in the driver (`gcc.cc`, link time). Marek Polacek confirms the defect there and quotes the code comment "We can't use OPT_Whardened yet. Sigh." Our defect 3 is the separate class-0 rejection in `cc1` (`toplev.cc`). No existing report covers that instance. Cite PR 117967 in the "See Also" field.
- [PR 117992](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=117992) (driver, NEW) — `-fhardened` warns about linker hardening options on `--enable-default-pie` builds. Same theme: the umbrella warns where it should stay silent, and the user cannot suppress the warning. It has no target-coverage angle.
- [PR 117739](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=117739) (driver, RESOLVED FIXED) — `-fhardened -Wl,-z,lazy` still passed `-z now` to the linker. A link-side constituent bug, now fixed. It does not touch the compile-side application order that our defect 2 describes.
- [PR 122710](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=122710) (driver, UNCONFIRMED) — `-fhardened` breaks `--help=topic` unless a file is passed. An adjacent `-fhardened` driver defect. It is unrelated to target coverage.
- gcc-patches, ["\[PATCH v4\] gcc: Introduce -fhardened"](https://inbox.sourceware.org/gcc-patches/ZUV5ZDgmrPJlLSdd@redhat.com/) (Marek Polacek, 2023-11-03) — the origin of the `linux*|gnu*` gate. Richard Biener suggested a static target list ("eventually even not support -fhardened for targets not listed"). Marek added `HAVE_FHARDENED_SUPPORT` in response and wrote: "If other OSs want to use -fhardened, they need to update the configure test." So the gate is an explicit invitation to extend, not a rejection of Darwin on merit.
- Same thread, review of v3 (Richard Biener, 2023-10-19) — the stated design intent: "the default configuration for a target should with -fhardened _not_ have any -Whardened diagnostics". The current Darwin behavior (warn, then half-apply, with no report) contradicts this intent twice.
- Same thread (Marek Polacek, 2023-10-23) — Darwin went untested, not unsupported by decision. Marek tried compile-farm machine 104 and hit "*** Configuration aarch64-apple-darwin21.6.0 not supported": GCC had no aarch64 Darwin port in 2023. GCC 16 has that port (this report runs on it), so the original reason to defer is gone.
- gcc-patches, ["\[COMMITTED\] testsuite: i386: Restrict gcc.target/i386/fhardened-1.c etc. to Linux/GNU"](https://inbox.sourceware.org/gcc-patches/ydd7cgzc7e2.fsf@CeBiTec.Uni-Bielefeld.DE/) (Rainer Orth, 2024-04-15) — the `fhardened-1.c`/`fhardened-2.c` tests failed on Solaris/x86 and Darwin/x86 with exactly our diagnostic ("cc1: warning: '-fhardened' not supported for this target"). The fix restricted the tests to Linux/GNU. It did not classify the warning and it did not extend coverage. This is prior on-list evidence that the warning fires on Darwin.
- gcc-patches, ["\[COMMITTED\] testsuite: i386: Restrict gcc.target/i386/pr124759.c to Linux"](https://inbox.sourceware.org/gcc-patches/yddqzooo95u.fsf@CeBiTec.Uni-Bielefeld.DE/) (Rainer Orth, 2026-04) — the same warning broke another test on Solaris, and the same restriction pattern repeated two years later. The class-0 warning keeps producing testsuite noise on every non-Linux target.
- Our [PR 126782](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126782) ([Darwin] `rsize_t`, target) — our prior Darwin-target report; relevant only as the filing precedent that this entry already cites. Our PRs 126783 and 126786 cover C++ modules and do not relate.

Duplicate searches with no result (GCC Bugzilla quicksearch, 2026-08-11):

- "hardened darwin" — one hit, an unrelated COBOL bug. Nothing on `-fhardened` Darwin coverage.
- "hardened target" — 20 hits, none about `-fhardened` coverage or the unclassifiable warning.
- "fhardened bsd" — zero hits.
- "fhardened musl" — zero hits.

Conclusion of the search at filing time: PR 117967 covered only the driver-side
sibling of the class-0 diagnostic.

## Public reports and outcome

1. [GCC PR 126822](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126822)
   covers the unclassifiable cc1 warning. It remains UNCONFIRMED. The report
   asks only whether `-Whardened` must control this diagnostic.
2. [GCC PR 126823](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126823)
   covered partial application after target rejection. A maintainer said that
   this behavior works as designed because `-fhardened` is supported only on
   GNU/Linux. Do not pursue this report.

The original investigation proposed Darwin enablement. That proposal did not
respect the documented support boundary. Do not write the `configure.ac`
enablement patch. Keep the evidence for the process record.

## Local workaround

The repository never uses `-fhardened`. The language profile in
`CMakeLists.txt` spells the selected hardening options individually:
`-ftrivial-auto-var-init=zero`, `-fstack-protector-strong`,
`-fzero-call-used-regs=used-gpr`, and GNU-scoped
`-fstack-clash-protection`.
