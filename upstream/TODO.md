# TODO — work that starts only after the reports are filed

Rule: write no patch now. Each patch below waits for its report to have a PR number
and initial maintainer feedback. Feedback can change a design; a patch written before
feedback wastes the work twice.

## Patch queue (in priority order)

1. `__need_rsize_t` support in `gcc/ginclude/stddef.h` — for PR target/126782.
   The full researched design, testsuite shape, review risks, and reading list are in
   `gcc-fixincludes-darwin-rsize-t/SUBMIT.md`. Wait state: the design already matches
   the maintainer's stated preference (comments 3–5); write it after the five pending
   reports are filed, since its review discussion may pull in the same people.
2. `-fhardened` Darwin enablement — follow-up to the `gcc-darwin-fhardened-coverage`
   report. Two candidate patches; decide the split after maintainer feedback:
   a. `gcc/configure.ac`: extend the `fhardened_support` case beyond `linux*|gnu*`
      for the constituents that demonstrably work on Darwin.
   b. Diagnostic classification: give the unsupported-target warning a proper class
      so `-Wno-hardened` / `-Wno-error=hardened` can demote it.
3. Candidate, only if the maintainer welcomes it in the fd-leak report thread:
   populate the analyzer assumed-not-to-throw list
   (`gcc/analyzer/region-model.cc`, the `fclose`-only list above the
   "TODO: populate this list more fully" comment) with the POSIX fd functions.
4. Follow-up protocol gaps, mention-first-patch-later: `__need_offsetof` (GCC's
   `stddef.h` has no such protocol) and the `__need_va_list` spelling
   (GCC's `stdarg.h` only knows `__need___va_list`). Raise them in the PR 126782
   patch mail; patch only if a maintainer agrees they are wanted.
5. `dwarf2out.cc` Mach-O early-debug exclusion — mirror the existing PE-COFF
   exclusion (the FIXME naming `copy_lto_debug_sections`) for Mach-O, so
   `-flto -g` on Darwin stops emitting invalid `__DWARF` silently. Raise it in
   the PR 82005 comment first.

## Apple reports (Feedback Assistant)

1. dsymutil unbounded growth on invalid DWARF input — file after the PR 82005
   comment is posted, and reference PR 82005 and llvm-project #102965 in it.
   Material: `gcc-lto-modules-debug-oom/` (growth curves, warning-flood samples).
2. The SDK `_rsize_t.h` guard assumes `__has_feature(modules)` implies Clang — file
   after the PR 126782 fix direction is fully settled. Material:
   `gcc-fixincludes-darwin-rsize-t/APPLE-FEEDBACK.md`. Reference FB15255066
   (fxcoudert's existing report on the `defined(__has_feature)` misuse) — do not
   duplicate it; ours is the modules-implies-Clang half.

## Post-filing hygiene

1. When each report gets a PR number: record it in the entry's README, in the
   `upstream/README.md` queue table, and in the matching `PINS.md` entry.
2. Ping cadence: one polite reply-to-thread ping after approximately two weeks of
   silence, per the policy notes in `upstream/README.md`.
3. On every toolchain bump (the `PINS.md` ritual): re-run each entry's reproduction;
   close out entries whose fixes landed; update Known-to-fail versions on the PRs.
4. Keep the trunk-verification results current: re-run the trunk matrix (see the
   entry READMEs' trunk sections) before any ping or patch submission, so every
   claim about master stays true on the day it is read.
5. Draft the PR 82005 comment and the llvm #102965 comment from the lto-oom
   entry.
