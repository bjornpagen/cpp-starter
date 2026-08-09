# upstream/ — GCC submissions

Everything this project sends to the GCC project, one directory per item:
verified repros, the fixincludes patch, and paste-ready `SUBMIT.md` text.
NVIDIA/stdexec material does NOT live here: those fixes are commits on the
maintained fork (github.com/bjornpagen/stdexec — PRs #2167/#2168 open,
awaiting maintainer ok-to-test).

| Item | Kind | Target | Status |
|---|---|---|---|
| `gcc-fixincludes-darwin-rsize-t` | patch (format-patch + DCO) | Bugzilla PR first, then gcc-patches@ | **SEND after gates 1, 3, 4** |
| `gcc-ice-modules-defaulted-friend-eq` | bug report (2-file repro) | Bugzilla, `c++`, ice-on-valid-code | **SEND after gate 2** |
| `gcc-ice-gmf-consteval-redecl` | bug report (4-line repro) | Bugzilla, `c++`, ice-on-valid-code | **SEND after gate 2** |
| `libstdcxx-silent-empty-std-module` | bug report (build behavior) | Bugzilla, `libstdc++` | **SEND last** (cites the posted fixincludes PR) |

Comment-only drops (no directories): CC + comment on **PR 124197** with
our `template for`/`-Wshadow` evidence (16.1.0 confirmation, reflection
variant, fires once per element), and on **PR 71962** with the
reflection-era impact of UBSan (`null`/`nonnull-attribute`) refusing to
constant-fold `std::string(ptr, size)` over vague-linkage storage — note
a candidate patch was attached upstream 2026-08-05.

Not filed, deliberately: the partition-BMI `std::expected` corruption has
no standalone repro; its reduction ledger lives with its workaround pin in
the bumbledb repo (`cpp/PINS.md`, entry `gcc-partition-bmi-expected`).
Checked and conforming, never file: `^^` on using-declarations (P2996R13)
and `inplace_vector::try_push_back` returning `optional<T&>` (P3981R2).

## Verified state (2026-08-09, everything re-run cold)

- Patch applies clean with `git am` to trunk master @ 0621cf67366 on a
  freshly reset clone (origin had not advanced past the generation base).
- Passes GCC's own server-side commit gate:
  `contrib/gcc-changelog/git_check_commit.py` → `OK`.
- `contrib/check_GNU_style.sh` flags are all documented-inapplicable
  (generated `fixincl.x`, verbatim SDK match text, `tests/base` wrapper
  convention) — preemption recorded in the SUBMIT.
- fixincludes builds standalone from the patched tree; the self-test
  replication reproduces the fixture **byte-identically**.
- Both ICE repros re-verified cold on the local 16.1.0: outputs match the
  SUBMIT transcripts verbatim (commands quoted as `g++-16`, the literal
  driver).
- libstdc++ report's cited `Makefile.am` line numbers are exact on
  today's trunk; no drift.
- Per-item duplicate searches recorded in each SUBMIT with dates; policy
  compliance audited against the live contribute.html / bugs pages
  2026-08-09.

## The provenance constraint (read before sending anything)

Upstream trunk has **no `aarch64-*-darwin*` target** in `gcc/config.gcc`
— the Apple-Silicon port is out of tree, and this machine's 16.1.0 was
built from the FSF release tarball **plus the darwin-arm64 port series**
(`~/.gcc/build/gcc-16.1.0.diff`). The bugs policy excludes "unofficial
releases or snapshots not issued by the GCC project," so the two ICE
reports must lead with reproduction on an **official FSF build**, citing
darwin as corroborating context with the port named. The module ICEs are
front-end streaming bugs, expected target-independent; gate 2 produces
that evidence. The fixincludes patch is unaffected in substance
(`x86_64-*-darwin*` is an upstream target and hits the same SDK header),
but its bootstrap evidence also comes from Linux (gate 3), since a native
darwin trunk bootstrap is impossible by construction.

## Gates (each gate updates its SUBMIT and then deletes itself here)

- ~~genfixes regeneration~~ **CLEARED 2026-08-09**: AutoGen 5.18.16 via
  the vanilla container; regenerated `fixincl.x` == hand-written hunk
  except the dated header; regenerated version adopted (amended commit
  `64ba9daf`, commit gate re-passed), caveat deleted from the SUBMIT.
- ~~Vanilla ICE evidence~~ **CLEARED 2026-08-09**: both ICEs reproduce
  on official FSF `gcc:16.1.0` on aarch64-linux-gnu (local finch
  container) AND x86_64 + aarch64 (CI) — with symbolic backtraces
  (`module_state::mangle` for friend-eq; `transfer_defining_module` ←
  `duplicate_decls` for GMF), now in the SUBMIT bodies.
- ~~fixincludes real `make check`~~ **CLEARED 2026-08-09**: passed in CI
  with autogen present, over the patched trunk.
1. **Trunk ICE status** (CI trunk-quick, in flight): add the "still
   reproduces on trunk @ <sha>" line to both ICE SUBMITs (the marked
   `[gate 2 — pending]` slots), or the fixed-on-trunk verdict, which
   would turn the reports into regression notes against 16.1.
2. **Bootstrap + regtest** (CI bootstrap-regtest, in flight): full
   default-language bootstrap of the patched trunk plus `make -k check`
   on aarch64-linux-gnu, summary artifact — becomes the one-line
   testing statement in the fixincludes SUBMIT.
3. **Final read-through** of each SUBMIT after gates land: no pending
   markers may remain when sending.

## The send procedure (in order; do not reorder)

**Step 0 — Bugzilla account (start now; 24h lead time).**
Try https://gcc.gnu.org/bugzilla/createaccount.cgi. The live page says:
"Because of spam, account creation through this form is restricted. If
creating an account fails, contact gcc-bugzilla-account-request@gcc.gnu.org
to request a GCC Bugzilla account. You should receive a response within
24 hours." If the form refuses bjorn.pagen@alpha.school, send that email.

**Step 0b — mailing-list prep (optional but recommended).**
Posting to gcc-patches@gcc.gnu.org works without subscribing, but
unsubscribed senders go through spam blocklists; subscribing first avoids
a silent drop. Plain-text mail only (no HTML). Lists cap messages at
400kB on gcc-patches (ours is 6.5kB). The posting address becomes
permanently public — archives are never edited.

**Step 1 — file the fixincludes Bugzilla PR.**
Product `gcc`, Component `other` (no dedicated fixincludes component;
triagers reassign freely), Version `16.1.0`. Title and body: the SUBMIT's
Title/Body sections. Note the assigned number — call it PRnnnnn.

**Step 2 — email the patch to gcc-patches@gcc.gnu.org.**
- Subject WITHOUT a PR: `[PATCH] fixincludes: darwin
  sys/_types/_rsize_t.h breaks under GCC -fmodules` (69 chars after the
  classifier — at the 75 limit).
- Subject WITH the PR from step 1 — **the long form busts the 75-char
  limit**, use the short summary instead:
  `[PATCH] fixincludes: define rsize_t under GCC -fmodules on darwin [PRnnnnn]`
  and put the full `PR other/nnnnn` in the commit body so Bugzilla links
  the post.
- Attach `0001-…rsize_t….patch` as `text/x-patch` (or send with
  `git send-email`); never paste the diff into a wrapping mail client.
- Body: the SUBMIT's Body + "Testing done" sections, plus: "I do not
  have write access to the GCC repository."
- No response after ~two weeks: one polite ping on the same thread with
  a brief summary and the archive URL of the original post.

**Step 3 — file the defaulted-friend-`operator==` ICE.**
Product `gcc`, Component `c++`, Version `16.1.0`, keyword
`ice-on-valid-code`, summary from the SUBMIT Title (`[modules]` prefix
included). Paste the Body; attach `a.cc` and `b.cc` **individually** (the
sanctioned multi-file exception — never an archive); add "See Also"
PR 122822; the dupe-search list and trunk status are already in the body.

**Step 4 — file the GMF redeclaration ICE.**
Same fields. Paste the Body; attach `repro.cc` (and optionally `q.h` +
`repro-include.cc` as the no-warning variant). The directory name
(`consteval-redecl`) is historical; the report text is the truth.

**Step 5 — file the libstdc++ silent-fallback report, last.**
Product `gcc`, Component `libstdc++`, Version `16.1.0`. Paste the Body;
cite the fixincludes PR number from step 1 and the gcc-patches archive
URL from step 2; cite Homebrew/homebrew-core#289142 as shipped-to-users
corroboration. Expect discussion — it asks maintainers to change a
deliberate fallback; the three-option remediation ladder in the body is
the negotiating position.

**Step 6 — comment drops.**
On PR 124197: confirmed on GCC 16.1.0; fires once per expanded element
(not N−1); also triggers via reflection/`consteval` expansion; CC
yourself. On PR 71962: reflection-era impact — UBSan refuses
constant-folding of `std::string(ptr, size)` over vague-linkage storage,
which breaks consteval string machinery under `-fsanitize=undefined`;
the iterator-pair construction works around it; note interest in the
2026-08-05 candidate patch.

**Step 7 — after the dust settles.**
Answer triage with the variant matrices already in the SUBMITs. When a
fix lands upstream for any item, its in-tree workaround pin (PINS.md,
both repos) has a tombstone pointing back here — delete workaround and
pin together.
