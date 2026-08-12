# GCC bug-report submission checklist

Sources: https://gcc.gnu.org/bugs/ and https://gcc.gnu.org/bugs/management.html,
read on 2026-08-11. Trunk is GCC 17 in development. The GCC 17 changes page says:
"GCC 17 has not been released yet". Report trunk results as "17.0 (experimental)".

This checklist covers six reports. The `gcc-analyzer-fd-leak-raii-fp` entry
produces two reports, because it contains two independent defects.

## 1. Policy requirements (the checklist)

Each report must satisfy every item below.

1. **Exact version.** Paste the full `g++ -v` output as plain text in the body.
   The policy requires "the exact version of GCC; the system type; the options
   given when GCC was configured/built". One `g++ -v` paste satisfies all three.
2. **System type.** State the target triple. State the build and host triples
   when they differ. Our compiler: build = host = target = `aarch64-apple-darwin24`.
3. **Configure options.** The `Configured with:` line of `g++ -v` supplies them.
   Ours is: `../gcc-16.1.0/configure --prefix=/Users/bjorn/.gcc/versions/16.1.0
   --enable-languages=c,c++ --disable-nls --enable-checking=release
   --program-suffix=-16 --with-system-zlib --build=aarch64-apple-darwin24
   --with-sysroot=/Applications/Xcode.app/.../MacOSX.sdk`.
4. **Complete command line.** Give the exact command that triggers the bug.
   Give the full sequence when more than one compile is necessary.
5. **Compiler output.** Paste the errors and warnings verbatim, as plain text,
   in the body. The policy says: "make sure the compiler version, error message,
   etc, are included in the body of your bug report as plain text".
6. **Preprocessed or self-contained source.** The policy calls the preprocessed
   file "the basic requirement to fix a bug". Generate it with `-save-temps`.
   Exception: a testcase "reduced ... to a small file that doesn't include any
   other file" needs no preprocessing.
   - **Modules caveat.** `-freport-bug` and `.ii` files do not capture module
     dependencies (CMIs, `gcm.cache/`). For a modules bug, attach every module
     source file as its own plain-text attachment. Give the exact compile order.
     State any search-path flag (for example `-fsearch-include-path` for the
     `std` module sources that ship with GCC).
7. **No archives.** The policy says: "Please avoid posting an archive (.tar,
   .shar or .zip); we generally need just a single file to reproduce the bug ...
   you're just making our volunteers' jobs harder." Attach each file separately,
   with content type `text/plain`. Do not attach `.s` files or binary files.
8. **One bug per report.** File one defect per report. Triage sets keywords,
   Known-to-fail, and the resolution per single defect. An omnibus report
   cannot be triaged. Split independent defects before you file.
9. **Check for duplicates first.** The policy lists among unwanted submissions:
   "Duplicate bug reports, or reports of bugs already fixed". Search Bugzilla
   and record the search terms and date in the entry before you file.
10. **Known-to-fail versions, including trunk.** State each version you tested,
    with the result. Management policy: "Update the Known To Fail and Known To
    Work fields to reflect passing and failing versions." Test 16.1.0 and, when
    possible, a GCC 17 snapshot. Say clearly when a trunk claim rests on source
    inspection only, not on a test run.
11. **Component and keywords.** Suggest the component and the keywords
    (`ice-on-valid-code`, `rejects-valid`, `wrong-code`, `diagnostic`, ...).
    A reduced, small testcase lets triage move the report to NEW. Management
    policy: putting C/C++ reports "in state NEW requires that there is a
    reduced, small testcase".
12. **Pre-filing sanity checks.** Run `-Wall -Wextra` on the testcase. For
    suspected wrong-code, test `-fno-strict-aliasing -fwrapv`. For C++, test
    `-D_GLIBCXX_ASSERTIONS`. These checks exclude user error.
13. **Keep the report minimal.** Do not include project build systems, CMake
    presets, or repository paths. The policy rejects "the location (URL) of the
    package that failed to build" as a substitute for a testcase.

## 2. Per-report gap table

Legend: A = `gcc-lto-modules-debug-oom` (GCC report only; the dsymutil half goes
to Apple Feedback). B = `gcc-darwin-fhardened-coverage`.
C = `gcc-analyzer-call-summary-ice`. D = fd-leak defect 1 (caller-owned fd,
plain C). E = fd-leak defect 2 (assumed-throwing libc calls).
F = `gcc-modules-freflection-typedef-merge`.

| Checklist item | A | B | C | D | E | F |
|---|---|---|---|---|---|---|
| Verbatim `g++ -v` recorded | GAP | GAP | GAP | GAP | GAP | GAP |
| Host/Target/Build stated | GAP | GAP | GAP | GAP | GAP | GAP |
| Configure options recorded | GAP | GAP | GAP | GAP | GAP | GAP |
| Complete command line | OK | OK | OK | OK | OK | OK |
| Verbatim compiler output | OK | OK | OK | OK | OK | OK |
| Self-contained or preprocessed source | OK* | OK | GAP | GAP | GAP | OK* |
| Known-to-fail incl. trunk | GAP | GAP | OK* | OK* | OK* | GAP |
| Duplicate check recorded | GAP | GAP | OK | GAP | GAP | OK |
| Component + keywords suggested | OK | OK | OK | OK | OK | OK |
| One bug per report | OK | GAP | OK | OK | OK | OK |
| Plain-text attachments, no archive | OK | OK | OK | OK | OK | OK |

Notes on the starred cells and the fixes:

**All six.** No README contains the verbatim `g++ -v` block. Each README states
only "GCC 16.1.0, self-built, target aarch64-apple-darwin24". Fix: paste the
full `g++ -v` output (it contains the configure line and the triple) into each
report body. This closes the first three rows in one step.

**A (lto/-g invalid DWARF on Darwin).**
- Source: OK with the modules caveat. Attach `p.cc`, `mprim.cc`, `pmain.cc`
  individually. State the `std` module step (`-fsearch-include-path
  bits/std.cc`) and the exact compile order. A `.ii` cannot capture this.
- The plain-TU control (`hello.cc`, `hmain.cc`) includes `<string>`. Fix:
  generate `hello.ii`/`hmain.ii` with `-save-temps` and attach them, or note
  the exception does not apply to these two files.
- Known-to-fail: no trunk statement exists at all. Fix: test or inspect GCC 17
  and state the result. Record Known-to-fail: 16.1.0.
- Duplicate check: none recorded. Fix: search Bugzilla for `dsymutil`,
  `DW_AT_stmt_list`, `darwin lto debug` before filing. Record the date.
- Keep the report scoped to GCC: invalid `__DWARF` in fat LTO objects.
  Describe the dsymutil runaway only as impact. Do not present it as a modules
  bug (the `hello.cc` control proves modules are not necessary).
- Remove the repository-scale section (CMake preset, `starter_httpd`) from the
  report body. Keep the 4-file form.
- Put the memory-guard warning in the report body. Triagers must not run the
  archive link unguarded.

**B (-fhardened on Darwin).**
- One bug per report: the README describes three separable defects and plans
  one report. Fix: split before filing. Report B1: the rejection warning has
  class 0, so `-Wno-hardened` and `-Wno-error=hardened` do not work. Report
  B2: the umbrella applies partially after rejection, with no per-constituent
  report. Keep the Darwin enablement (defect 1) out of Bugzilla; it goes to
  gcc-patches as the planned `configure.ac` patch. Alternative: file one
  report scoped to "rejection handling of -fhardened on unsupported targets"
  and name both symptoms; let triage split. The two-report split is safer.
- Known-to-fail: the README itself lists "check trunk" as an open task. Fix:
  do the trunk check before filing. Record Known-to-fail: 16.1.0.
- Duplicate check: none recorded. Fix: search Bugzilla for `fhardened`
  before filing. Record the date.

**C (analyzer call-summary ICE).**
- Source: `repro-standalone.cc` is include-free and satisfies the exception.
  `repro.cc` includes `<tuple>` and `<variant>`. Fix: attach `repro.ii`
  (generate with `-save-temps`), or lead with the standalone file and give
  `repro.cc` inline as the real-world form.
- Output: the README quotes the ICE line only. Fix: capture the full stderr
  with the "Please submit a full bug report" backtrace and attach it as
  plain text.
- Known-to-fail: OK with a caveat. The trunk claim rests on source inspection
  (assert unchanged on master, read 2026-08-11), not on a test run. State this
  distinction in the report. Better: run a GCC 17 snapshot.
- Duplicate check: OK. Recorded, dated 2026-08-11, terms
  `convert_region_from_summary`.

**D (fd-leak defect 1, caller-owned fd, plain C).**
- Source: `repro.c` includes `<sys/socket.h>` and `<unistd.h>`. The exception
  does not apply. Fix (preferred): reduce further; declare the `accept`,
  `close` prototypes directly and drop both includes. Fix (fallback): attach
  `repro.i` from `-save-temps`.
- Output: `analyzer-output.txt` mixes both defects. Fix: attach only the
  defect-1 warning paths to this report. One report, one defect, one output.
- Duplicate check: GAP by the entry's own admission (Bugzilla was unreachable).
  Fix: run the Bugzilla quicksearch for `-Wanalyzer-fd-leak` at filing time.
- Known-to-fail: 16.1.0; master sites inspected at commit `14d1f0c9858`. State
  the inspection/test distinction. Better: run a snapshot.

**E (fd-leak defect 2, assumed-throwing libc calls).**
- Source: `repro-minimal.cc` includes SDK headers, and the missing nothrow
  annotations in those headers are the trigger. Reviewers without an Apple SDK
  cannot reproduce from the raw source. Fix: attach the Darwin
  `repro-minimal.ii` from `-save-temps`. This is the one report where the
  preprocessed file is essential, not optional.
- Output: attach only the defect-2 path (the "throws an exception" events).
- State the controls in the body: `-fno-exceptions` and
  `-fanalyzer-assume-nothrow` remove the warnings; glibc (`__THROW`) does not
  reproduce.
- Duplicate check: same GAP and fix as D.
- Known-to-fail: same caveat as D (one-entry `fclose` whitelist inspected on
  master, not run).

**F (modules + -freflection typedef merge).**
- Source: OK with the modules caveat. The reduced set (`reduced.h`,
  `reduced-module.cc`, `reduced-main.cc`) is self-contained. A `.ii` cannot
  capture the CMI. Fix: attach the three files individually, give the exact
  two-command compile order, and also inline the 6-line testcase in the body.
- The real-world form (`mr.cc`, `main_refl.cc`) needs `<meta>` and `<string>`.
  Mark it as secondary evidence.
- Known-to-fail: 16.1.0 only. Reflection is under active development on trunk.
  Fix: test a GCC 17 snapshot before filing; the bug may already be fixed.
  Record the result either way.
- Duplicate check: OK. Recorded, dated 2026-08-11. Add PR 122785 to See Also.
  Cite PR 98770 as fixed history, not as a duplicate.

## 3. Duplicate-handling etiquette

The policy text: the bugs page lists among things "we do not want":
"Duplicate bug reports, or reports of bugs already fixed."

The operating rule when an existing PR covers the same defect:

1. Do not file a new report.
2. Add one comment to the existing PR. Put the minimal reproduction in that
   comment. Mark it as an additional testcase and a confirmation.
3. Include the exact version data (`g++ -v`) and the target triple in that
   comment, so the maintainers can extend Known-to-fail.
4. Do not add "me too" comments. Do not add a second comment. One comment
   carries all the new information.
5. If our testcase adds nothing beyond what the PR already shows, add nothing.

The policy does not spell out a comment protocol beyond the duplicate ban. The
one-comment rule is our house rule. It follows from the ban and from the
management policy's use of Known-to-fail data.

## 4. Attachment manifests

General rules for every report:

- Attach each file separately with content type `text/plain`.
- Do not use archives. PR 126783 attached a `.tar.gz`. The policy discourages
  this: "by storing it in an archive, you're just making our volunteers' jobs
  harder." Do not repeat it. Bugzilla accepts many attachments per report.
- Inline any testcase under about 30 lines directly in the report body, in
  addition to the attachment.
- Paste `g++ -v`, the command line, and the verbatim diagnostics in the body.

**A — lto/-g invalid DWARF (component `debug`, keywords `wrong-debug, lto`).**
- `p.cc`, `mprim.cc`, `pmain.cc` — module testcase, three separate text files
- `hello.cc`, `hmain.cc` (plus their `.ii`) — plain-TU control
- `dwarfdump-verify.txt` — new capture of `dwarfdump --verify p.o` output
- `dsymutil-rss-growth.txt` — RSS series, as-is
- `link-warning-sample.txt` — warning-flood sample, as-is
- Body: full command sequence including the `ar` and link steps; guard warning

**B1 — warning class 0 (component `middle-end`, keyword `diagnostic`).**
- `hardened.cc` — inline in body; attach only if required
- Body: the three command/output pairs for `-Werror`, `-Wno-error=hardened`,
  `-Wno-hardened`

**B2 — silent partial application (component `middle-end`, keyword
`diagnostic`).**
- `hardened.cc` — same file, inline
- Body: the `-E -dM` macro evidence and the `-Q --help=common` diff
- `constituents.cc`, `assertions.cc` — attach as constituent-support evidence

**C — call-summary ICE (component `analyzer`, keyword `ice-on-valid-code`).**
- `repro-standalone.cc` — primary, include-free, inline in body
- `repro.cc` — secondary; attach with `repro.ii`
- `ice-backtrace.txt` — new capture of the full crash stderr
- Body: both commands, including the `--param` for the standalone form; the
  Linux result (standalone ICEs, `repro.cc` compiles clean on Linux)

**D — fd-leak, caller-owned purge (component `analyzer`, keyword
`diagnostic`).**
- `repro.c` — primary; reduce to prototype-only, else attach `repro.i` too;
  inline in body
- `repro-minimal.cc` — secondary C++ form
- `analyzer-output-defect1.txt` — defect-1 paths only
- Body: note the sites (`on_liveness_change`, `implicitly_live_p`) and that
  master is unchanged at `14d1f0c9858`

**E — fd-leak, assumed-throwing libc (component `analyzer`, keyword
`diagnostic`).**
- `repro-minimal.cc` — primary, inline in body
- `repro-minimal.ii` — mandatory Darwin preprocessed file
- `analyzer-output-defect2.txt` — defect-2 path with the throw events
- Body: the three necessary conditions; the glibc non-reproduction; the
  `-fanalyzer-assume-nothrow` control; the one-entry whitelist site

**F — modules/-freflection merge (component `c++`, keyword `rejects-valid`).**
- `reduced.h`, `reduced-module.cc`, `reduced-main.cc` — primary, three
  separate text files, also inline (6-line header plus two small TUs)
- `mr.cc`, `main_refl.cc` — real-world form, two separate text files
- Body: exact compile order for both forms; the Darwin and glibc diagnostics;
  the `<meta>`-hypothesis refutation; See Also PR 122785
