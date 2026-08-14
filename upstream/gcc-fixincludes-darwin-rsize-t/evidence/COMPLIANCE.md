# Compliance list for PR 126782 gcc-patches

Policy pages: evidence/logs/policy-read.txt (read 2026-08-13).
Public send is blocked until Bjorn approves `APPROVAL-PACKET.md`.
The packet and public message disclose that no pre-patch full-suite
baseline or `gcc-testresults` comparison was used.
A Bugzilla attachment does not replace gcc-patches.

## Legal

- [x] The contributor owns the work (user confirmed 2026-08-13)
- [x] From identity: Bjorn Pagen <hello@bjornpagen.com> (matches Bugzilla)
- [x] Signed-off-by line is in the format-patch and EMAIL.txt
- [x] Co-authored-by is absent from the public patch
- [ ] The user approves the exact public email before send

## Maintainer direction (PR 126782 comment 4)

- [x] Not a Darwin fixincludes rule
- [x] GCC `<stddef.h>` answers explicit `__need_rsize_t`
- [x] Not full Annex K
- [x] Does not follow comment 5's optional Clang full-inclusion path
- [x] EMAIL.txt states the patch's limited scope in plain text

## Patch form

- [x] One logical change
- [x] Regression tests: four `gcc.dg` files and one `g++.dg` file
- [x] ChangeLog lives in the commit message
- [x] Subject after `[PATCH]` is 52 characters and uses `[PR126782]`
- [x] Body uses `PR target/126782`
- [x] `git diff --check` is clean
- [x] `git apply --check` is clean on parent `c5d147d7370fb36834c9348c5d3bab229d89fb3e`
- [x] Applied tree equals the local commit tree
- [x] `check_GNU_style.sh` hits match the existing `__need_*` and `_SIZE_T` style
- [x] `mklog.py` ran. Keep the handwritten `stddef.h` ChangeLog line.
- [x] Source diff from local commit `63e3cdeb413866b501b102e5e37afcd6a1f510d7`
- [x] Mail patch derived from `git format-patch`; public prose revised
- [x] `contrib/gcc-changelog` check on that commit: OK

## Testing (contribute.html)

- [x] 3-stage bootstrap, default languages, at least one target
- [x] `make -k check` on that build
- [x] Test the exact patch that the mail will send
- [ ] Compare against a pre-patch full testsuite or suitable recent
  `gcc-testresults` results. Not run; disclosed as a deviation.
- [x] Darwin protocol probes recorded
- [x] Original `rsize.cc -fmodules` recorded as already passing on Darwin GCC 17
- [x] Trunk only. No GCC 16 backport request in the public mail.

## Routing

- [x] To: `gcc-patches@gcc.gnu.org` only
- [x] Not libstdc++, Fortran, Rust, jit, Algol 68, or bpf lists
- [x] Plain text, not HTML
- [x] Write-access sentence omitted by contributor; it is not required
- [x] This packet does not comment on Bugzilla, change fields, or push to GCC

## After send (submission agent, after approval)

- [ ] Record the gcc-patches archive URL
- [ ] Then, and only then, an optional Bugzilla note with that URL
