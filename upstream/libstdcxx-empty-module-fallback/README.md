# Empty libstdc++ module fallback

## Public report

- Report: [GCC PR 126786](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126786)
- Summary: `[libstdc++] Module fallback installs empty interface files but the manifest lists them`
- Component: `libstdc++`
- Bugzilla status: UNCONFIRMED
- Assignee: unassigned
- Last checked: 2026-08-12 UTC

The build can install empty module interface files after a module compile
failure while the manifest still lists those interfaces. PR 126782 triggered
the observed fallback on Darwin, but any module compile failure can reach the
same path.

The report asks GCC to keep the installed interface files and the manifest
consistent. No maintainer has selected the expected fallback behavior.

## Next action

Wait for libstdc++ maintainer direction before writing a patch. Restore
additional reproduction material from repository history or the Bugzilla
attachments only when it is needed for testing.

