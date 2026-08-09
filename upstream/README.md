# upstream/ — GCC submissions

Everything this project sends to the GCC project, one directory per item:
verified repros, the fixincludes patch, and paste-ready `SUBMIT.md` text.
All claims re-verified against the pinned toolchain (g++-16 = GCC 16.1.0,
aarch64-apple-darwin24) and validated against trunk @ `0621cf67366`
(2026-08-09). NVIDIA/stdexec material does NOT live here: those fixes are
commits on the maintained fork (github.com/bjornpagen/stdexec — branches
`fix/arm64-duplicate-inline`, `fix/sync-wait-no-exceptions`; PR briefs in
the fork's `FORK.md`).

| Item | Kind | Target | Status | Summary |
|---|---|---|---|---|
| `gcc-fixincludes-darwin-rsize-t` | patch (`0001-…rsize_t….patch`, format-patch + DCO) | gcc-patches mailing list (+ Bugzilla PR first) | **SEND** — applies clean to trunk; no existing hack; fixincl self-test replicated (autogen caveat disclosed in the email text) | SDK `_rsize_t.h` assumes `__has_feature(modules)` ⇒ clang stddef protocol; `rsize_t` never defines under GCC `-fmodules`, breaking the libstdc++ std module on darwin |
| `gcc-ice-modules-defaulted-friend-eq` | bug report | Bugzilla, Component `c++`, keyword ice-on-valid-code | **SEND** — 2-file repro re-verified; dupe search clean (PR 122822 is see-also, not dup) | Importer ICEs (segfault) using an imported class with a defaulted hidden-friend `operator==` |
| `gcc-ice-gmf-consteval-redecl` | bug report | Bugzilla, Component `c++`, keyword ice-on-valid-code | **SEND** — 4-line repro (no consteval needed; directory name is historical); triage matrix verified; dupe search clean | GMF variable declared `extern`, then defined `inline`, segfaults cc1plus; blocks stdexec in any GMF |
| `libstdcxx-silent-empty-std-module` | bug report | Bugzilla, Component `libstdc++` | **SEND** — fallback confirmed on trunk (line numbers cited are trunk's); corroboration: Homebrew/homebrew-core#289142 | Failed std-module compile installs a 1-byte `bits/std.cc` plus its `modules.json` entry with exit 0 |
| `gcc-modules-partition-bmi-expected` | bug report | Bugzilla, Component `c++` (modules) | **not yet filed — no action needed** (no standalone repro; attempts + reduction plan in NOTES.md; related: PR 125595, 125144, 125356) | Non-template member body instantiating `std::expected` corrupts partition BMI for re-export ("Bad file data") |

Comment on existing upstream reports instead (no directories needed): CC +
comment on GCC **PR 124197** with our `template for`/`-Wshadow` evidence
(fires once per element; 16.1.0 + reflection variant), and on GCC
**PR 71962** with the reflection-era impact of UBSan refusing to
constant-fold `std::string(ptr, size)` over vague-linkage storage.

## Suggested submission order

1. `gcc-fixincludes-darwin-rsize-t` — mechanical, self-contained, tested.
2. `gcc-ice-modules-defaulted-friend-eq` — small segfault repro on valid code.
3. `gcc-ice-gmf-consteval-redecl` — 4-line segfault, full triage matrix.
4. `libstdcxx-silent-empty-std-module` — asks maintainers to change a
   deliberate fallback, mildly discussion-prone; send after (1) so it can
   link the posted fixincludes patch it references.

## How to send these (from gcc.gnu.org/bugs/ and /contribute.html)

### Bug reports → GCC Bugzilla

1. **Account**: create one at https://gcc.gnu.org/bugzilla/ — a valid
   email address is required (anti-spam policy, stated on the bugs page).
2. **Before filing**, the bugs page asks that you check the well-known
   bugs list and, if possible, a current snapshot. Done for every SEND
   item here: each `SUBMIT.md` records the dupe searches and the trunk
   check date — mention both in the report.
3. **What they need** (all in each `SUBMIT.md` already): the exact GCC
   version, system type, and configure options (all three come from
   `g++-16 -v` — paste its output verbatim); the complete command line;
   the complete compiler output; and the testcase. The preprocessed-file
   requirement is waived for exactly our case — their stated excuse (ii):
   "if you've reduced the testcase to a small file that doesn't include
   any other file." Every SEND repro here is such a file.
4. **Formatting**: paste version/command/output as plain text in the
   report body — never only inside an attachment. No archives. The
   modules ICEs legitimately need two source files (an exporter and an
   importer); that is the sanctioned multiple-source-files exception —
   attach both files individually, still quoting everything essential in
   the body.
5. **Fields**: Product `gcc`; Component per the table above; Version
   `16.1.0`; add host/target `aarch64-apple-darwin24` in the body (and
   the ICE keyword where the table says so).
6. **After filing**: expect triage to confirm/reduce; respond with the
   NOTES.md material if a maintainer asks for variants — the triage
   matrices in the SUBMIT files anticipate the common questions.

### The fixincludes patch → gcc-patches mailing list

1. **Legal**: covered by the DCO route — contribute.html allows
   certifying the Developer Certificate of Origin with a
   `Signed-off-by:` tag instead of an FSF copyright assignment; the
   prepared commit in `0001-…rsize_t….patch` already carries it (small
   fixes may not strictly need either, but the tag costs nothing).
2. **File the Bugzilla PR first** (item 1 of the order above can carry
   both hats: report + patch), then email the patch to
   `gcc-patches@gcc.gnu.org` referencing the PR number in the subject,
   e.g. `[PATCH] fixincludes: darwin: define rsize_t under GCC modules
   (PR nnnnn)`.
3. **Format**: send the `git format-patch` output inline or as an
   attachment; the ChangeLog entry lives in the commit message per
   current convention (the prepared patch follows recent fixincludes
   commits' style); include the testing statement (what was run, on what
   target — ours discloses the autogen caveat and invites the maintainer
   to re-run `./genfixes`).
4. **The patch branch** also lives at github.com/bjornpagen/gcc, branch
   `fixincludes-darwin-rsize-t`, for your own reference — GCC does not
   take GitHub PRs; the mailing list is the submission channel.
5. **Ping cadence**: if there is no response, a polite ping on the same
   thread after a week or two is the accepted practice.

## Checked, conforming, not filed

- **`^^` on names introduced by using-declarations** (`^^std::uint64_t`
  → "cannot be applied to a using-declaration"): P2996R13 makes
  reflect-expressions ill-formed on using-declarators — GCC is
  conforming. Verified workarounds recorded in the in-tree pins:
  `^^::uint64_t`, a local alias, or resolution through a template
  parameter.
- **`inplace_vector::try_push_back` returning `optional<T&>`**: P3981R2
  (adopted March 2026) — libstdc++ tracks the working draft; our
  P0843R14-era comments were corrected instead.

## Not upstream material

- The `db_impl.cc` implementation-unit split (bumbledb) works around
  `gcc-modules-partition-bmi-expected` (not yet filed, above); the
  scoped `-Wno-shadow` works around PR 124197; the iterator-pair
  `std::string` construction works around PR 71962. Each gets deleted
  when its upstream fix lands.
- stdexec fixes live on the maintained fork
  (github.com/bjornpagen/stdexec, pinned by the top-level CMakeLists):
  every fork commit has a pending upstream PR (briefs in FORK.md), and
  the fork's success condition is its own emptiness.
