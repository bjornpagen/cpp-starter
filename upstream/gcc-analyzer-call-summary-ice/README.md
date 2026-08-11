# Analyzer call-summary replay ICEs on callees that return through a hidden reference

Status: analysis complete, reproduction verified under guard, root cause traced to source. Check Linux before filing (a Linux run is being arranged separately).

The ICE is instantaneous and allocates nothing unusual; every compile below was still run under the standard watchdog (`/tmp/buildguard.sh 8192 12288 600 ...`, one compile at a time). The guard never fired.

The original repro material was lost with `/tmp`; this directory rebuilds it from the recorded recipe and reduces it further. The reduction shows two claims in the historical note were incidental: the receiver-struct-with-reference-member indirection is not needed (a plain reference parameter suffices), and the ICE is `-O2`-only, not `-O1+`.

## Symptom

`g++-16 -O2 -fanalyzer -fanalyzer-call-summaries` crashes with an internal compiler error while replaying a call summary, at the `types_compatible_p` assertion in `call_summary_replay::convert_region_from_summary` (`gcc/analyzer/call-summary.cc:550`). The trigger is a summarized callee that returns a class by hidden reference (return slot optimization): the summary's `RESULT_DECL` region is reference-typed, the caller-side region it is mapped to is the value-typed return slot, and the two types are not compatible.

The code being analyzed is valid; the crash is analyzer-internal state, not a diagnosis. Originally hit while running the analyzer over `foreign/exec.backend.cc` (stdexec), which ICEs the same way; `repro.cc` reproduces it from `<tuple>`/`<variant>` alone, and `repro-standalone.cc` reproduces it with no library headers at all.

## Environment

- GCC 16.1.0, self-built, `/Users/bjorn/.gcc/current`, target `aarch64-apple-darwin24`
- macOS arm64 (Darwin 24.6.0), Apple Silicon, 96 GB RAM
- CMake 4.2.1, Ninja 1.13.2 (not used by this reproduction; single-TU compiles only)
- Current GCC master still contains the identical assert and the identical unreconciled `RESULT_DECL` replay path (read from the gcc-mirror source on 2026-08-11; trunk was not run locally)

## Files

- `repro.cc` — 18-line primary reproduction: `std::tuple<int>` extracted from a `std::variant` alternative by a helper, called from two call sites; needs only the two analyzer flags
- `repro-standalone.cc` — 21-line library-free reduction: any class with a user-provided copy constructor; needs one extra `--param` to force summarization of the tiny callee

## Reproduction (verified under guard)

Primary form:

```sh
g++-16 -O2 -std=c++17 -fanalyzer -fanalyzer-call-summaries -c repro.cc
```

Verbatim result:

```text
during IPA pass: analyzer
In function 'void run(std::tuple<int>&, std::variant<std::tuple<int> >&)',
    inlined from 'void caller1(std::tuple<int>&, std::variant<std::tuple<int> >&)' at repro.cc:13:5:
repro.cc:9:19: internal compiler error: in convert_region_from_summary, at analyzer/call-summary.cc:550
    9 |         out = take(v);
      |               ~~~~^~~
```

Library-free form (the callee is too small for the default summarization threshold, so lower it):

```sh
g++-16 -O2 -std=c++17 -fanalyzer -fanalyzer-call-summaries \
  --param analyzer-min-snodes-for-call-summary=0 -c repro-standalone.cc
```

```text
during IPA pass: analyzer
In function 'void run(payload&, payload&)',
    inlined from 'void caller1(payload&, payload&)' at repro-standalone.cc:16:5:
repro-standalone.cc:12:19: internal compiler error: in convert_region_from_summary, at analyzer/call-summary.cc:550
   12 |         out = take(v);
      |               ~~~~^~~
```

Expected result: the analyzer finishes and the compile exits 0 with no diagnostic, as it does with `-fanalyzer` alone on the same files.

## Trigger matrix (each cell verified under guard)

Standard version is irrelevant: `repro.cc` ICEs identically at `-std=c++17/20/23/26`, each across the full `-O` matrix below.

| Variation | Result |
|---|---|
| `-O2`, both analyzer flags (repro.cc) | ICE at call-summary.cc:550 |
| `-O0`, `-O1`, `-O3`, or `-Os` | compiles |
| `-O2 -fanalyzer` without `-fanalyzer-call-summaries` | compiles |
| `-O2`, no analyzer | compiles |
| one call site instead of two | compiles |
| payload is a trivially copyable struct (returned in registers) | compiles |
| same struct given a user-provided copy constructor (returned via hidden reference) | ICE (`repro-standalone.cc`, with the `--param`) |
| receiver struct holding the destination by reference, or by pointer, or plain reference parameter | ICE in all three forms — the indirection shape is irrelevant |
| no `std::variant`, `noinline` callee, default params | compiles (callee below `analyzer-min-snodes-for-call-summary`, so never summarized) |
| no `std::variant`, `noinline` callee, `--param analyzer-min-snodes-for-call-summary=0` | ICE |

Necessary combination: a callee that (a) returns a class by hidden reference (non-trivially-copyable return type, so the call uses return slot optimization), (b) is actually summarized (`-fanalyzer-call-summaries`, at least `analyzer-min-snodes-for-call-summary` supernodes — `std::get` on a variant supplies the mass via the `bad_variant_access` throw path), and (c) has more than one call site as the analyzer sees the IL, so a summary gets replayed. `-O2` is what produces exactly that shape here: early inlining folds `run` into both callers, leaving two direct return-slot calls to `take`; at `-O1` `run` stays out of line so `take` has a single call site, and at `-O3` `take` is inlined away entirely.

## Analysis

The analyzer's emergency dump at the point of the ICE shows the caller's IL (`-fdump-ipa-analyzer`, file `repro.cc.084i.analyzer`):

```text
D.20773 = take (v_3(D)); [return slot optimization]
```

and the callee returns through the hidden slot — its `RESULT_DECL` is `DECL_BY_REFERENCE`, appearing in GIMPLE as a reference-typed SSA name:

```text
struct tuple take (struct variant & v)
{
  struct tuple & _3(D);
  ...
  *_3(D) = MEM[(const struct tuple &)v_2(D)];
  return _3(D);
}
```

During replay of `take`'s summary at the `caller1` call site, the summary's stores through `*_3(D)` require converting the initial value of the `RESULT_DECL` region (the hidden slot pointer). `call_summary_replay::convert_svalue_from_summary_1` (`SK_INITIAL` case, call-summary.cc:273) converts that svalue's region; `convert_region_from_summary_1` handles it here (call-summary.cc:632):

```c
case RESULT_DECL:
  return m_cd.get_lhs_region ();
```

This is the only conversion path in the function that performs no type reconciliation — every other case reuses the region, passes the summary region's type into the new region, or wraps the result in `get_cast_region`. It equates the summary's `RESULT_DECL` region (type `std::tuple<int>&`, because the decl is `DECL_BY_REFERENCE`) with the caller's value-typed return slot `D.20773` (type `std::tuple<int>`). The wrapper's consistency assertion then fires:

```c
if (caller_reg)
  if (summary_reg->get_type () && caller_reg->get_type ())
    gcc_assert (types_compatible_p (summary_reg->get_type (),
                                    caller_reg->get_type ()));   /* <- line 550 */
```

The assertion is correctly reporting a real modeling error, not a stale invariant: for a by-reference `RESULT_DECL`, the initial value of the decl region is the *address of* the caller's LHS region, so replaying it as the LHS region itself would keep being wrong even with the assert deleted (the slot's contents would be used where its address is meant). The `SK_INITIAL` machinery comments that "Params should already be in the cache, courtesy of the ctor" — the replay constructor pre-maps `PARM_DECL` initial values to the actual arguments, but a `DECL_BY_REFERENCE` `RESULT_DECL` is a hidden parameter that is not a `PARM_DECL`, so it misses that cache and falls into the mismatched decl-region path. A plausible fix is to special-case by-reference `RESULT_DECL`s in the `RK_DECL`/`SK_INITIAL` replay paths, mapping the initial pointer value to `&lhs_region` (a `region_svalue` for `get_lhs_region ()`).

`take` never gets past `convert_region_from_summary`'s cache warm-up, so the failure is deterministic and immediate; nothing about the crash is Darwin-specific on its face, but only the Darwin build was run.

## Suggested upstream destination

GCC Bugzilla, product `gcc`, component `analyzer`, version `16.1.0`, keywords `ice-on-valid-code`. Title suggestion: `[analyzer] ICE in call_summary_replay::convert_region_from_summary on call summaries for functions returning by invisible reference`. Attach `repro.cc` (primary, flags only) and `repro-standalone.cc` (library-free, needs the `--param`); both bodies are small enough to inline in the report. State that the assert and the unreconciled `RESULT_DECL` path are unchanged on current master.

Duplicate check: Bugzilla and web searches for `convert_region_from_summary` (during the original probe and again on 2026-08-11) found no existing report naming this function or this assert.

Before filing: rerun both commands on Linux to confirm the crash is target-independent, as expected from the code path (a Linux run is being arranged separately).

## Linux check (done)

Not Darwin-specific. The 21-line `repro-standalone.cc` reduction ICEs identically on
aarch64-unknown-linux-gnu (same self-built GCC 16.1.0, Debian trixie): cc1plus crashes
with the analyzer backtrace through `engine.cc` under the exact documented command. The
larger `repro.cc` compiles clean on Linux at the same flags — mention both facts in the
report; the reduction is the cross-platform testcase.

## Local workaround

None needed. No build configuration in this repository enables `-fanalyzer-call-summaries`; the blocker arose in an exploratory analyzer run over `foreign/exec.backend.cc`. No `PINS.md` entry.
