# Compliance list for PR 126782 gcc-patches

Policy page: https://gcc.sourceware.org/contribute.html (read 2026-08-13).
Do not send until every open item is done.
A Bugzilla attachment does not replace gcc-patches.

## Legal

- [x] The contributor owns the work (user confirmed 2026-08-13)
- [x] From identity: Bjorn Pagen <hello@bjornpagen.com> (matches Bugzilla)
- [x] Signed-off-by line is in COMMIT-MESSAGE.txt
- [ ] The user approves the exact public email before send

## Maintainer direction (PR 126782 comments 3 and 4)

- [x] Not a Darwin fixincludes rule
- [x] GCC `<stddef.h>` answers explicit `__need_rsize_t`
- [x] Not full Annex K
- [x] Does not follow comment 5's optional Clang full-inclusion path
- [x] EMAIL.txt states that last point in plain text

## Patch form

- [x] One logical change
- [x] Regression tests: four `gcc.dg` files and one `g++.dg` file
- [x] ChangeLog lives in the commit message
- [x] Subject after `[PATCH]` is 52 characters and uses `[PR126782]`
- [x] Body uses `PR target/126782`
- [x] `git diff --check` is clean
- [x] `git apply --check` is clean on HEAD `c5d147d7370fb36834c9348c5d3bab229d89fb3e`
- [x] `check_GNU_style.sh` hits match the existing `__need_*` and `_SIZE_T` style
- [x] `mklog.py` ran. Keep the handwritten `stddef.h` ChangeLog line.
- [ ] `git format-patch` from a local commit, after bootstrap and check
- [ ] `contrib/gcc-changelog` check on that commit

## Testing (contribute.html)

- [ ] 3-stage bootstrap, default languages, at least one target
- [ ] `make -k check` on that build
- [ ] Test the exact patch that the mail will send
- [x] Darwin protocol probes recorded
- [x] Original `rsize.cc -fmodules` recorded as already passing on Darwin GCC 17
- [ ] Do not call the work a bootstrap until Finch prints `bootstrap finished`
- [x] Trunk only. No GCC 16 backport.

Finch container `gcc-rsize-bootstrap` still runs.
PID 1 is `sleep`. The exec runs `bootstrap.sh`.
Languages: `c,c++,fortran,lto,objc`.
`stage_final` is stage3.
Host, build, and target: `aarch64-unknown-linux-gnu`.

## Routing

- [x] To: `gcc-patches@gcc.gnu.org` only
- [x] Not libstdc++, Fortran, Rust, jit, or bpf lists
- [x] Plain text, not HTML
- [x] Write-access sentence is in EMAIL.txt
- [x] This packet does not comment on Bugzilla, change fields, or push to GCC

## After send (submission agent, after approval)

- [ ] Record the gcc-patches archive URL
- [ ] Then, and only then, an optional Bugzilla note with that URL
