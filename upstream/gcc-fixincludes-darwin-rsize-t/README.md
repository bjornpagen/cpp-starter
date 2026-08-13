# gcc-fixincludes-darwin-rsize-t — filed as PR 126782; patch strategy changed

## Status

- We filed the Bugzilla report as [PR target/126782](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126782) on 2026-08-10. The "See Also" field points to PR 116827, which reports the same SDK assumption for `ptrdiff_t` and `size_t`.
- Bugzilla status: UNCONFIRMED. Local state: patch investigation. Last checked: 2026-08-12 UTC.
- Maintainer feedback from Drea Pinski (comments 3–4) changed the plan:
  1. The maintainer discourages fixincludes rules on Darwin. A rule binds to one SDK, but users select a different SDK at build time.
  2. The maintainer prefers support for `__need_rsize_t` in GCC's own `<stddef.h>`.
- The reporter agreed in comment 5. GCC's `<stddef.h>` already supports the selective-inclusion protocol for `size_t`, `ptrdiff_t`, and `wchar_t`. The proposed first patch adds only the explicit `__need_rsize_t` protocol. It must not add full Annex K exposure unless local trunk research shows that the broader change is required.
- The fixincludes patch in this directory does not go to gcc-patches in its current form. The patch stays here as a reference and as the source of the local workaround header.

## Next work

1. Follow [`LOCAL-AGENT-HANDOFF.md`](LOCAL-AGENT-HANDOFF.md) in a clean local GCC trunk clone.
2. Verify that current trunk still lacks the explicit `__need_rsize_t` protocol.
3. Implement the smallest generic `<stddef.h>` change and add target-independent tests.
4. Test the exact patch on Linux and macOS. Record the trunk revision and every command.
5. Prepare a private patch package. Do not send it until the exact public artifact and legal route receive approval.
6. File the Apple report (`APPLE-FEEDBACK.md`) only if current evidence still supports it.

## Files

- `fixed-header.h` — the live local workaround. Every toolchain rebuild copies
  this file into GCC's `include-fixed/sys/_types/`. This file stays until the
  upstream fix is in GCC. The patch strategy does not change this.
- `0001-fixincludes-Fix-rsize_t-with-Darwin-modules.patch` — the superseded fixincludes patch. This file is a reference only.
- `inclhack-entry.def` — the fixincludes rule from the superseded patch. This file is a reference only.
- `rsize.cc`, `rsize.ii` — the minimal source and the preprocessed output. We attached these files to the PR.
- `SUBMIT.md` — the work plan for the replacement patch.
- `LOCAL-AGENT-HANDOFF.md` — the self-contained prompt for local GCC trunk work.
- `APPLE-FEEDBACK.md` — the pending Apple report about the SDK guard.
