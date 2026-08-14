# GCC bug ledger

This file is the canonical status ledger for public GCC reports from this
investigation. Update it after each report, maintainer response, status change,
patch submission, fix, or verification run.

`Bugzilla status` is the literal upstream field. `Local state` records the next
action in this repository. Do not infer one from the other.

Rows are public upstream pointers. Do not recreate deleted investigation or
reproduction trees.

Last full read-only review: 2026-08-14 UTC.

## Active reports

| PR | Summary | Component | Bugzilla status | Local state | Next action |
|---|---|---|---|---|---|
| [126782](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126782) | `[Darwin] sys/_types/_rsize_t.h does not define rsize_t with -fmodules` | target | UNCONFIRMED | `retired-no-local-work` | [Patch submitted](https://inbox.sourceware.org/gcc-patches/20260814032423.29082-1-hello@bjornpagen.com/) on 2026-08-14. GCC trunk commit [`08ede4f`](https://gcc.gnu.org/git/?p=gcc.git;a=commit;h=08ede4fbbe6d38e08bcc72c4df2cf6720b9f4717) had already removed the incorrect Clang-modules feature claim and the active trigger. Do no more work unless a maintainer responds. |
| [126783](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126783) | `[16/17 Regression] [modules] ICE when a GMF variable is later defined inline` | c++ | ASSIGNED | `pointer-only` | None. Patrick Palka has the assignment. Pin: `gcc-gmf-stdexec-ice`. |
| [126786](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126786) | `[libstdc++] Module fallback installs empty interface files but the manifest lists them` | libstdc++ | UNCONFIRMED | `pointer-only` | None. |
| [126805](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126805) | `[analyzer] ICE in convert_region_from_summary for a class return slot` | analyzer | UNCONFIRMED | `pointer-only` | None. |
| [126806](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126806) | `[analyzer] false fd-leak warning for a caller-owned struct member` | analyzer | UNCONFIRMED | `pointer-only` | None. |
| [126819](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126819) | `[analyzer] false fd-leak warning on a throwing edge in a noexcept destructor` | analyzer | UNCONFIRMED | `pointer-only` | None. |
| [126822](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126822) | `-fhardened target warning cannot be controlled by -Whardened` | middle-end | UNCONFIRMED | `pointer-only` | None. Pin: `gcc-darwin-fhardened`. |

## Completed or dispositioned reports

| PR | Summary | Bugzilla status | Local state | Outcome |
|---|---|---|---|---|
| [126823](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126823) | `-fhardened applied partially after target rejection, with no per-constituent report` | UNCONFIRMED | `closed-no-action` | A maintainer said that the behavior works as designed. The option is documented as GNU/Linux-only. Do not pursue the unsupported-target semantics. |

## Existing reports

| Upstream item | Local state | Note |
|---|---|---|
| [GCC PR 125595](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=125595) | `pointer-only` | Pin `gcc-partition-bmi-inplace-vector`. Do not file or comment. Re-test `inplace_vector` on every toolchain bump. |
| [GCC PR 82005](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=82005) | `pointer-only` | Pin `gcc-darwin-lto-debug-dsymutil`. Do not file a new GCC report. |
| [LLVM issue 102965](https://github.com/llvm/llvm-project/issues/102965) | `pointer-only` | Same pin. Do not comment without approval. |
| [GCC PR 124582](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=124582) | `fixed-upstream` | Fixed for GCC 16.2. |
| [GCC PR 124197](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=124197) | `pointer-only` | Pin `gcc-template-for-wshadow`. |

## Update procedure

1. Read the report and its activity log without changing it.
2. Update the literal Bugzilla status.
3. Update the local state only when the next action changes.
4. Record the current UTC date in the review line.
5. Do not recreate retired technical-evidence directories.
6. Do not post, comment, upload, or change fields as part of a ledger refresh.
