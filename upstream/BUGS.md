# GCC bug ledger

This file is the canonical status ledger for public GCC reports from this
investigation. Update it after each report, maintainer response, status change,
patch submission, fix, or verification run.

`Bugzilla status` is the literal upstream field. `Local state` records the next
action in this repository. Do not infer one from the other.

Last full read-only review: 2026-08-12 UTC.

## Active reports

| PR | Summary | Component | Bugzilla status | Local state | Local evidence | Next action |
|---|---|---|---|---|---|---|
| [126782](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126782) | `[Darwin] sys/_types/_rsize_t.h does not define rsize_t with -fmodules` | target | UNCONFIRMED | `patch-investigation` | [`gcc-fixincludes-darwin-rsize-t`](gcc-fixincludes-darwin-rsize-t/) | Implement and test the maintainer-preferred `__need_rsize_t` approach before proposing a patch. |
| [126783](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126783) | `[16/17 Regression] [modules] ICE when a GMF variable is later defined inline` | c++ | ASSIGNED | `assigned-upstream` | [`gcc-modules-gmf-inline-variable-ice`](gcc-modules-gmf-inline-variable-ice/) | Wait for Patrick Palka's work. Respond if he requests evidence or testing. |
| [126786](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126786) | `[libstdc++] Module fallback installs empty interface files but the manifest lists them` | libstdc++ | UNCONFIRMED | `awaiting-triage` | [`libstdcxx-empty-module-fallback`](libstdcxx-empty-module-fallback/) | Wait for a libstdc++ maintainer to choose the expected fallback behavior. |
| [126805](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126805) | `[analyzer] ICE in convert_region_from_summary for a class return slot` | analyzer | UNCONFIRMED | `awaiting-triage` | [`gcc-analyzer-call-summary-ice`](gcc-analyzer-call-summary-ice/) | Monitor for triage. Prepare a patch only after the fix and regression test are validated. |
| [126806](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126806) | `[analyzer] false fd-leak warning for a caller-owned struct member` | analyzer | UNCONFIRMED | `awaiting-triage` | [`gcc-analyzer-fd-leak-raii-fp`](gcc-analyzer-fd-leak-raii-fp/) | Wait for analyzer maintainer feedback on the ownership model. |
| [126819](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126819) | `[analyzer] false fd-leak warning on a throwing edge in a noexcept destructor` | analyzer | UNCONFIRMED | `awaiting-triage` | [`gcc-analyzer-fd-leak-raii-fp`](gcc-analyzer-fd-leak-raii-fp/) | Wait for analyzer maintainer feedback on exception and `noexcept` modeling. |
| [126822](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126822) | `-fhardened target warning cannot be controlled by -Whardened` | middle-end | UNCONFIRMED | `awaiting-triage` | [`gcc-darwin-fhardened-coverage`](gcc-darwin-fhardened-coverage/) | Wait for a decision on whether the unsupported-target diagnostic must use the `-Whardened` class. |

## Completed or dispositioned reports

| PR | Summary | Bugzilla status | Local state | Outcome |
|---|---|---|---|---|
| [126823](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126823) | `-fhardened applied partially after target rejection, with no per-constituent report` | UNCONFIRMED | `closed-no-action` | A maintainer said that the behavior works as designed. The option is documented as GNU/Linux-only. Do not pursue the unsupported-target semantics. |

## Existing reports and issues that need evidence

| Upstream item | Local evidence | Planned action |
|---|---|---|
| [GCC PR 82005](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=82005) | [`gcc-lto-modules-debug-oom`](gcc-lto-modules-debug-oom/) | Add one evidence comment after approval. Do not file a new GCC report. |
| [LLVM issue 102965](https://github.com/llvm/llvm-project/issues/102965) | [`gcc-lto-modules-debug-oom`](gcc-lto-modules-debug-oom/) | Add one comment with the measured `dsymutil` growth after approval. |

## Retired investigations

| Investigation | Upstream result | Local state |
|---|---|---|
| [`gcc-modules-freflection-typedef-merge`](gcc-modules-freflection-typedef-merge/) | [GCC PR 124582](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=124582), fixed for GCC 16.2 | `fixed-upstream` |

## Update procedure

1. Read the report and its activity log without changing it.
2. Update the literal Bugzilla status.
3. Update the local state only when the next action changes.
4. Record the current UTC date in the review line.
5. Update the related investigation README.
6. Keep technical evidence in the investigation directory, not in this ledger.
7. Do not post, comment, upload, or change fields as part of a ledger refresh.
