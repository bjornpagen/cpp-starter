# The analyzer call-summary replay causes an ICE on callees that return through a hidden reference

Status:

- The analysis is complete.
- The reproduction is verified under guard.
- The root cause is traced to source.
- The report is [GCC PR 126805](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126805). It is UNCONFIRMED and awaiting triage.
- The Linux check is complete. The 21-line reduction causes the same ICE on Linux (see the section "Linux check (done)").
- Trunk check by run: both reproductions ICE on master commit 475e9eff (Linux container build, 2026-08-12).
- Official-image check by run: both reproductions ICE with exit 1 on the official Docker `gcc:16.1.0` image (aarch64-linux, 2026-08-12) — including the primary `repro.cc`, which the self-built Linux 16.1.0 compiled clean. This closes the "unofficial self-built compiler" objection: self-built Darwin, self-built Linux, the official image, and self-built master all show the bug.

The ICE is immediate and allocates nothing unusual. But we still ran every compile below under the standard watchdog (`/tmp/buildguard.sh 8192 12288 600 ...`), one compile at a time. The guard never fired.

The original repro material was lost together with `/tmp`. This directory rebuilds the material from the recorded recipe and reduces it further. The reduction shows that two claims in the historical note were incidental:

- The receiver-struct-with-reference-member indirection is not necessary. A plain reference parameter is sufficient.
- The ICE occurs only at `-O2`, not at `-O1+`.

## Symptom

`g++-16 -O2 -fanalyzer -fanalyzer-call-summaries` crashes with an internal compiler error when it replays a call summary. The crash occurs at the `types_compatible_p` assertion in `call_summary_replay::convert_region_from_summary` (`gcc/analyzer/call-summary.cc:550`). The trigger is a summarized callee that returns a class by hidden reference (return slot optimization). In that condition:

- The summary's `RESULT_DECL` region has a reference type.
- The replay maps that region to the caller-side return slot, which has a value type.
- The two types are not compatible.

The analyzed code is valid. The crash comes from analyzer-internal state; it is not a diagnosis. We first hit the crash in an analyzer run over `foreign/exec.backend.cc` (stdexec), which crashes with the same ICE. `repro.cc` reproduces the ICE from `<tuple>` and `<variant>` alone. `repro-standalone.cc` reproduces the ICE with no library headers.

## Environment

- GCC 16.1.0, self-built, `/Users/bjorn/.gcc/current`, target `aarch64-apple-darwin24`
- macOS arm64 (Darwin 24.6.0), Apple Silicon, 96 GB RAM
- CMake 4.2.1, Ninja 1.13.2 (not used by this reproduction; single-TU compiles only)
- Current GCC master contains the identical assert and the identical unreconciled `RESULT_DECL` replay path. We read this from the gcc-mirror source on 2026-08-11. We then built master commit `475e9efffaf8de781d7e17b687faf1807e104b01` from source on aarch64-linux (container, 2026-08-12) and ran both reproductions there. Both exit 1 with the internal compiler error. The result matrix is at `/Users/bjorn/finch-gcc16/trunkcheck/trunk-matrix.txt`.
- We repeated the reproduction on the official Docker `gcc:16.1.0` image (aarch64-linux, 2026-08-12): both files ICE with exit 1, the testcase compiles clean there under `-Wall -Wextra` without `-fanalyzer` (the user-error check per GCC bug policy: recorded, passed), and the official-build preprocessed source is attached as `ice-repro.official.ii`. The result matrix and logs are at `/Users/bjorn/finch-gcc16/official/`.

## Files

- `repro.cc` — the 18-line primary reproduction. A helper extracts `std::tuple<int>` from a `std::variant` alternative. Two call sites call the helper. The reproduction needs only the two analyzer flags.
- `repro-standalone.cc` — the 21-line library-free reduction. Any class with a user-provided copy constructor is sufficient. The reduction needs one extra `--param` to force summarization of the small callee.
- `ice-full-backtrace.txt` — the complete stderr of the standalone ICE on Darwin. The release-checking build prints a shallow backtrace. This file is the complete output.
- `ice-repro.official.ii` — the preprocessed source of `repro.cc` from the official Docker `gcc:16.1.0` image (aarch64-linux), for the Bugzilla attachment.

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

Library-free form. The callee is smaller than the default summarization threshold, so the command lowers the threshold:

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

Expected result: the analyzer completes, and the compile exits 0 with no diagnostic. This is the result with `-fanalyzer` alone on the same files.

## Trigger matrix (each cell verified under guard)

The standard version is not relevant. `repro.cc` crashes with the identical ICE at `-std=c++17/20/23/26`. We verified each standard version across the full `-O` matrix below.

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

The ICE needs this combination:

1. The callee returns a class by hidden reference. The return type is not trivially copyable, so the call uses return slot optimization.
2. The analyzer summarizes the callee. This needs `-fanalyzer-call-summaries` and at least `analyzer-min-snodes-for-call-summary` supernodes. `std::get` on a variant supplies the necessary supernodes through the `bad_variant_access` throw path.
3. The callee has more than one call site in the IL that the analyzer sees. Then the analyzer replays a summary.

`-O2` produces exactly that shape here:

- At `-O2`, early inlining folds `run` into both callers. This leaves two direct return-slot calls to `take`.
- At `-O1`, `run` stays out of line, so `take` has a single call site.
- At `-O3`, the compiler inlines `take` away entirely.

## Analysis

At the point of the ICE, the analyzer's emergency dump shows the caller's IL (`-fdump-ipa-analyzer`, file `repro.cc.084i.analyzer`):

```text
D.20773 = take (v_3(D)); [return slot optimization]
```

The callee returns through the hidden slot. Its `RESULT_DECL` is `DECL_BY_REFERENCE` and appears in GIMPLE as a reference-typed SSA name:

```text
struct tuple take (struct variant & v)
{
  struct tuple & _3(D);
  ...
  *_3(D) = MEM[(const struct tuple &)v_2(D)];
  return _3(D);
}
```

The analyzer replays `take`'s summary at the `caller1` call site. The summary's stores through `*_3(D)` make the replay convert the initial value of the `RESULT_DECL` region (the hidden slot pointer). `call_summary_replay::convert_svalue_from_summary_1` (`SK_INITIAL` case, call-summary.cc:273) converts that svalue's region. `convert_region_from_summary_1` handles the region here (call-summary.cc:632):

```c
case RESULT_DECL:
  return m_cd.get_lhs_region ();
```

This is the only conversion path in the function that does no type reconciliation. Every other case does one of these:

- It reuses the region.
- It passes the summary region's type into the new region.
- It wraps the result in `get_cast_region`.

This path equates the summary's `RESULT_DECL` region with the caller's value-typed return slot `D.20773`. The summary region has type `std::tuple<int>&`, because the decl is `DECL_BY_REFERENCE`. The return slot has type `std::tuple<int>`. The wrapper's consistency assertion then fires:

```c
if (caller_reg)
  if (summary_reg->get_type () && caller_reg->get_type ())
    gcc_assert (types_compatible_p (summary_reg->get_type (),
                                    caller_reg->get_type ()));   /* <- line 550 */
```

The assertion reports a real modeling error; it is not a stale invariant. For a by-reference `RESULT_DECL`, the initial value of the decl region is the *address of* the caller's LHS region. Thus a replay of that value as the LHS region itself stays wrong even with the assert deleted. The analyzer would use the slot's contents where the code means the slot's address. The `SK_INITIAL` machinery has this comment: "Params should already be in the cache, courtesy of the ctor". The replay constructor pre-maps `PARM_DECL` initial values to the actual arguments. But a `DECL_BY_REFERENCE` `RESULT_DECL` is a hidden parameter that is not a `PARM_DECL`. Thus it misses that cache and falls into the mismatched decl-region path. A plausible fix is to special-case by-reference `RESULT_DECL`s in the `RK_DECL`/`SK_INITIAL` replay paths. The fix maps the initial pointer value to `&lhs_region` (a `region_svalue` for `get_lhs_region ()`).

`take` never gets past the cache warm-up in `convert_region_from_summary`. Thus the failure is deterministic and immediate. No part of the crash looks Darwin-specific on its face. But at that point, we ran only the Darwin build.

## Public report

We filed [GCC PR 126805](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126805)
in the `analyzer` component with the `ice-on-valid-code` keyword. The report
uses `repro-standalone.cc` as the library-free reduction and includes the
official-image preprocessed source. It remains UNCONFIRMED.

The next action is to wait for triage. Prepare a patch only after the fix and
regression test are validated.

Duplicate check: we searched Bugzilla and the web for `convert_region_from_summary`, during the original probe and again on 2026-08-11. The searches found no existing report that names this function or this assert.

The Linux run is complete and confirms the prediction: the crash is target-independent. See the section "Linux check (done)" below for the exact result on both files.

## Related reports (verified 2026-08-11)

We verified each reference below against GCC Bugzilla (REST API) and the gcc-mirror repository. None is a duplicate of this bug. Cite them in the report as follows:

- PR 99390 — the meta-bug tracker for analyzer call summaries (alias `analyzer-call-summaries`). Set `Blocks: 99390` on the new report.
- PR 107072 — the report that produced the current replay code. Commit r13-3077-gbfca9505f6fce6 (David Malcolm, 2022-10-05, GCC 13) created `call-summary.cc` for it. The first version of the file already contains the unreconciled `RESULT_DECL` case (line 619 there).
- PR 114473 (RESOLVED FIXED for 13.3/14) — the nearest match. Its fix, r14-9697-gfdd59818e2abf6 (backport r13-8757), is the commit that added the `types_compatible_p` asserts that fire here. That fix added a type cast only for the symbolic-region deref case. The `RESULT_DECL` case received no equivalent cast. The PR 114473 backtrace passes through `convert_region_from_summary`, but its ICE was in `deref_rvalue`. It is a different bug with a different trigger (C, pointer parameter).
- PR 114798 (UNCONFIRMED, filed 2024-04-22) — a sibling ICE at the companion assert from the same fix (`convert_svalue_from_summary_1`, call-summary.cc:290, GCC 14). The trigger is a C nested function, not a hidden-reference return. Still open; not a duplicate.
- PR 114159 ([13 Regression]) — a different `-fanalyzer-call-summaries` ICE, in `call_info` (call-info.cc:143). Fixed on trunk by commit c0d8a64e7232 (2024-02-29).
- PR 114897 (UNCONFIRMED) — an analyzer ICE bisected to the same revamp commit r13-3077. Together with PR 114798, it shows the replay code is a known source of open ICEs.
- Flag history — the GCC 10.1 manual already documents `-fanalyzer-call-summaries`. The documentation states the two activation conditions (more than one call site, `analyzer-min-snodes-for-call-summary`). It states no limitation about return types.
- Trunk check — gcc-mirror master at commit `475e9efffaf8de781d7e17b687faf1807e104b01` (fetched 2026-08-11) contains the identical assert and the identical `RESULT_DECL` case. Only the line numbers moved: the assert is at call-summary.cc:538 and the `RESULT_DECL` case is at line 620 on trunk, against 550 and 632 in the 16.1.0 release.

Clean searches, for the duplicate-check record: Bugzilla quicksearch finds no report for `convert_region_from_summary`, `analyzer types_compatible_p`, `analyzer invisible reference`, `analyzer hidden reference`, `analyzer NRV`, or `analyzer return slot`. The only `analyzer RESULT_DECL` hit is PR 95039, an unrelated `dump_expr` wording issue.

## Linux check (done)

The crash is not Darwin-specific. The 21-line `repro-standalone.cc` reduction crashes with the identical ICE on aarch64-unknown-linux-gnu (same self-built GCC 16.1.0, Debian trixie). Under the exact documented command, cc1plus crashes with the analyzer backtrace through `engine.cc`. The larger `repro.cc` compiles clean on Linux at the same flags. Include both facts in the report. The reduction is the cross-platform testcase.

## Local workaround

No workaround is necessary. No build configuration in this repository enables
`-fanalyzer-call-summaries`. The defect appeared during an exploratory analyzer
run over `foreign/exec.backend.cc`.
