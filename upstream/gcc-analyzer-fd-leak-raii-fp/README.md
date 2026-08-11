# Analyzer false-positive fd leaks on canonical RAII fd owners

Status: the analysis is complete. We verified the reproduction under the guard. We traced the root causes to source. The Linux check is also complete: defect 1 reproduces on Linux, and defect 2 does not (see the section "Linux check (done)"). The reports are ready to file.

Each compile below completes immediately and allocates nothing unusual. We still ran each compile under the standard guard (`/tmp/buildguard.sh 8192 12288 600 ...`), one compile at a time. The guard did not fire.

The original reproduction material was lost when `/tmp` was cleared. This directory minimizes it again from `unsafe/net.backend.cc` (the `Fd` owner at lines 84–123 and the kqueue accept loop at lines 437–475). The reduction corrects two claims in the historical note:

- The 6 warnings from the approximately 76-line file collapse into 3 distinct reports here. Exactly two independent defects produce these reports.
- The headline defect is not specific to C++. A 17-line plain-C translation unit triggers it.

## Symptom

`g++-16 -O2 -fanalyzer` (no extra analyzer flags) emits spurious CWE-775 `-Wanalyzer-fd-leak` warnings on a minimal RAII fd wrapper. The wrapper code contains:

- an `Fd` class that owns an int, closes it in the destructor, and has move semantics
- a `Server` struct that holds a listener `Fd`
- an accept helper that takes `Server&`

Every descriptor in the file has a provable owner:

- The caller keeps ownership of the listener through the `Server&` parameter, and `Fd::~Fd` closes it.
- The `Fd` destructor closes the accepted fd on the `continue` and return paths, or the code moves the fd into slot ownership.

The code is valid. Every warning is a false positive.

Two independent defects produce the reports:

1. **The analyzer reports a caller-owned fd as a leak (the headline shape).** The call `accept(server.listener.get(), ...)` makes the analyzer attach fd state ("listening stream socket") to the value loaded from `server->listener.value_`. The temporary that holds the loaded value dies at the next statement. The analyzer then purges the state and reports a leak, anchored at the `accept()` callsite: `leak of file descriptor '((const Fd*)server)[1].Fd::value_'`. The caller can still reach the fd through the reference parameter. The fd never leaks. Plain C with a `struct server *` parameter triggers the identical report at every `-O` level (`repro.c`).
2. **The analyzer reports a leak on an exception edge through a noexcept destructor.** With `-fexceptions` (the C++ default), the new exception modeling in GCC 16 assumes that every external call without a `nothrow` mark can throw. Darwin's SDK headers annotate nothing. Thus `close`, `fcntl`, and `accept` each get a throwing edge, and the analyzer reports the fd as a leak on that edge. The report anchors at the `::close(value_)` call that closes the fd, with the path event `if 'int close(int)' throws an exception...`. The enclosing destructor is implicitly `noexcept`. Thus, even under the false premise, the exception can only reach `std::terminate`. It can never reach a running caller.

## Environment

- GCC 16.1.0, self-built, `/Users/bjorn/.gcc/current`, target `aarch64-apple-darwin24`
- macOS arm64 (Darwin 24.6.0), Apple Silicon, 96 GB RAM
- CMake 4.2.1, Ninja 1.13.2 (not used for this reproduction; the reproduction uses single-TU compiles only)
- Current GCC master keeps all three implicated code sites unchanged. We read them from a local master checkout at commit `14d1f0c9858` (2026-08-10). We did not run trunk locally.

## Files

- `repro.cc` — a 91-line faithful minimization of the accept loop: the `Fd` owner, the `Server` with `queue`/`listener`/slots, and the accept helper that takes `Server&`. It causes 3 warnings. These include the historical diagnostic shape `((const Fd*)server)[1].Fd::value_` at the accept callsite.
- `repro-minimal.cc` — a 29-line reduction that shows both defects in one TU (2 warnings)
- `repro.c` — a 17-line plain-C reduction of defect 1 only (1 warning; no C++ anywhere)
- `analyzer-output.txt` — the complete verbatim output of `g++-16 -O2 -fanalyzer -c repro.cc`, with all three event paths

## Reproduction (verified under guard)

```sh
g++-16 -O2 -fanalyzer -c repro.cc
```

The verbatim warning lines follow. The full paths are in `analyzer-output.txt`. The compile still exits with code 0 and produces an object file.

```text
repro.cc:65:35: warning: leak of file descriptor '*(const Fd*)((char*)&*accept_ready::server + offsetof(Server, Server::listener)).Fd::value_' [CWE-775] [-Wanalyzer-fd-leak]
repro.cc:65:35: warning: leak of file descriptor 'descriptor.Fd::value_' [CWE-775] [-Wanalyzer-fd-leak]
repro.cc:77:87: warning: leak of file descriptor '((const Fd*)server)[1].Fd::value_' [CWE-775] [-Wanalyzer-fd-leak]
```

The two `repro.cc:65` reports point into `set_nonblocking` (the `fcntl` call). The first report claims that the caller-owned listener leaks there (defect 1). The second report claims that the accepted fd leaks "if `fcntl` throws" (defect 2). The `repro.cc:77` report claims that the listener leaks at its own `accept()` callsite (defect 1). Its full path has three events and ends in `(3) '((const Fd*)server)[1].Fd::value_' leaks here` at the call.

The reduced forms follow.

```sh
g++-16 -O2 -fanalyzer -c repro-minimal.cc
```

```text
repro-minimal.cc:14:32: warning: leak of file descriptor 'accept(*(const Fd*)server.Fd::value_, 0, 0)' [CWE-775] [-Wanalyzer-fd-leak]
repro-minimal.cc:27:79: warning: leak of file descriptor '((const Fd)*server).Fd::value_' [CWE-775] [-Wanalyzer-fd-leak]
```

Line 14 is `::close(value_)` inside `Fd::~Fd`. The analyzer reports the accepted fd as a leak at the exact statement that closes it. The path goes through the close and appends `leaks here`.

```sh
gcc-16 -O2 -fanalyzer -c repro.c
```

```text
repro.c:12:18: warning: leak of file descriptor '*s.listener' [CWE-775] [-Wanalyzer-fd-leak]
```

The expected result for all three compiles: the analyzer completes with no diagnostic. The analyzer gives this correct result for the same logic in two cases: when the code passes the fd as a plain `int` parameter (defect 1), and when the compile uses `-fno-exceptions` (defect 2).

## Trigger matrix (each cell verified under guard)

| Variation | Result |
|---|---|
| `repro.cc`, `-O0`/`-O1`/`-O2`/`-O3`/`-Os`, `-fanalyzer` | 3 warnings at every level |
| `repro.cc`, `-std=c++17/20/23/26` | identical 3 warnings each |
| `repro.cc`, `-O2 -fanalyzer -fno-exceptions` | 2 warnings — defect 2 gone, defect 1 remains |
| `repro.cc`, `-O2 -fanalyzer -fanalyzer-assume-nothrow` | 2 warnings — same split as `-fno-exceptions` |
| accept helper takes plain `int listener` instead of a class member | defect 1 gone; defect 2 remains at `-O0`..`-O3` (leak of the accepted fd "if close throws", anchored at the close) |
| plain `int` listener AND `-fno-exceptions` | 0 warnings |
| `repro.c` (plain C, `struct server *`), `-O0`/`-O1`/`-O2` | defect 1 fires at every level — no C++ needed |
| `-O2` without `-fanalyzer` | clean |

The necessary conditions for each defect:

1. Defect 1 needs an fd stored in an object that the analyzed function reaches only through a pointer or reference parameter. It also needs a call on that fd which is relevant to the fd state machine (`accept` here).
2. Defect 2 needs three conditions together:
   - `-fexceptions`
   - a libc whose headers do not declare `close`/`fcntl` as non-throwing (Darwin's SDK does not: `unistd.h` declares `int close(int) __DARWIN_ALIAS_C(close);` with no nothrow attribute)
   - an fd owned by an object that has a destructor

   Every RAII fd wrapper in every default-flags C++ program on this target satisfies these conditions.

## Analysis

Both defects are visible in the 16.1.0 analyzer sources. All cited sites are unchanged on current master (commit `14d1f0c9858`).

**Defect 1 — the liveness purge removes fd state that the caller owns.** The sequence:

1. `fd_state_machine::on_accept` (`gcc/analyzer/sm-fd.cc:2061`) transitions the listener svalue to `fd-listening-stream-socket`. Here the svalue is the value loaded from `server->listener.value_`.
2. The analyzer keys sm-state by svalue. Only the SSA temporary that holds the loaded member keeps that svalue alive.
3. The temporary dies immediately after the call. `sm_state_map::on_liveness_change` (`gcc/analyzer/program-state.cc:639`) then finds the svalue non-live.
4. `fd_state_machine::can_purge_p` (`sm-fd.cc:2292`) returns false for socket states, so `on_state_leak` fires. This is why the report anchors at the callsite itself with a near-empty path.

The rescue hook is `initial_svalue::implicitly_live_p` (`gcc/analyzer/svalue.cc:1427`). It keeps an initial value alive only in two cases:

- The store still binds that identical svalue to its region.
- The svalue is the initial value of a parameter of the top-level frame (`initial_value_of_param_p`, which requires the SSA default-def of a `PARM_DECL`).

The initial value of a field reached through a parameter satisfies neither case. Thus the analyzer reports a leak for each fd that a caller-visible object owns and that the analyzed function only uses. A plausible fix: extend the liveness test (or the leak check) to treat sm-state on values reachable from live pointer/reference parameters as caller-owned, not leaked. The analyzer already applies the same reasoning to the parameters themselves.

**Defect 2 — the analyzer assumes that every external call can throw, and does not model terminate.** GCC 16 added exception modeling. `can_throw_p` (`gcc/analyzer/region-model.cc:2220`) returns true for any call under `-fexceptions`, unless the decl is `nothrow` or the callee is on the assumed-not-to-throw list. That list contains exactly one entry, `"fclose"`, above a `// TODO: populate this list more fully` (`region-model.cc:2203`). Darwin's SDK annotates no POSIX function, so `close`, `fcntl`, and `accept` each get a throwing edge. (On glibc, `close` is declared `__THROW`. The Linux check confirmed the consequence: the same source produces the two warning lines on glibc, but with zero throwing-edge events in the paths. Defect 2 is real only on libcs without nothrow annotations. See the section "Linux check (done)".) On the throwing edge, the analyzer never applies the effect of the close. The frame unwinds (`exploded_graph::unwind_from_exception`, `gcc/analyzer/engine.cc`), the `Fd` dies with the frame, and the analyzer reports the fd as leaked. The unwinder walks CFG EH edges and caller frames. Nothing in `gcc/analyzer/` models that the destructor is implicitly `noexcept`. A throw from `close` can only reach `std::terminate`. At that point the OS tears down the whole process and every fd in it. Thus a leak report is unactionable even under the false throwing premise. `-fanalyzer-assume-nothrow` (added in GCC 16 precisely as a workaround for this class of noise) silences all of it. The cost: it also assumes that user C++ callees do not throw.

The GCC 16 release notes say that the analyzer is "now usable on simple C++ examples". But `invoke.texi` still says that the analyzer "is only suitable for use on C code in this release". `repro.c` keeps defect 1 fully inside the supported C scope. Thus triage cannot dismiss the primary report as a C++ limitation.

## Suggested upstream destination

File two Bugzilla reports, because the defects are independent and one is C-scoped:

1. GCC Bugzilla, product `gcc`, component `analyzer`, version `16.1.0`, keywords `diagnostic`. Title suggestion: `[analyzer] -Wanalyzer-fd-leak false positive on fd owned by caller through pointer/reference parameter (state purged when loading temporary dies)`. Attach `repro.c` (primary, plain C) and `repro-minimal.cc`. Both are small enough to inline. Point at the `sm_state_map::on_liveness_change` / `initial_svalue::implicitly_live_p` interaction. Note that the sites are unchanged on master.
2. GCC Bugzilla, product `gcc`, component `analyzer`, version `16.1.0`, keywords `diagnostic`. Title suggestion: `[analyzer] fd/resource leak false positives from assumed-throwing libc calls (close, fcntl) on targets without nothrow header annotations; noexcept destructors not modeled`. Attach `repro-minimal.cc`. Reference the one-entry `get_fns_assumed_not_to_throw` whitelist. Ask whether the analyzer must assume that sm-fd's own known functions do not throw. Ask whether unwinding must stop at noexcept frames. Note `-fanalyzer-assume-nothrow` as the existing blunt workaround.

Duplicate check: on 2026-08-11 we did web searches for the diagnostic shapes (`-Wanalyzer-fd-leak` false positive with struct members / pointer parameters / RAII destructors, and the anchored-at-close shape). The searches found no existing report. GCC Bugzilla's own search UI was unreachable from this network (bot-check interstitial). Thus run a Bugzilla quicksearch for `-Wanalyzer-fd-leak` at filing time.

Linux check (done): we compiled `repro-minimal.cc` on aarch64-unknown-linux-gnu (same
self-built GCC 16.1.0, Debian trixie, glibc headers), at `-O0` and at `-O2`. The result
separates the two defects cleanly:

- The compile emits the same two warning lines verbatim, including the
  `((const Fd)*server).Fd::value_` shape. But the diagnostic paths contain zero
  "throws an exception" events (`grep -c` over the full output: 0). On glibc, the
  warnings come from the defect-1 purge logic alone.
- Thus defect 1 is target-independent. File it as such.
- Thus defect 2 does not reproduce on glibc, because glibc declares `close`/`fcntl`
  with `__THROW`. This confirms the prediction in the Analysis section. File defect 2
  as a defect that occurs on any libc without nothrow annotations, with Darwin as the
  concrete case, and name `-fanalyzer-assume-nothrow` as the flag-level control.

The full Linux diagnostic output is preserved at
`/Users/bjorn/finch-gcc16/fdleak-linux-full.txt` (not part of this repository).

## Local workaround

None is necessary. The build deliberately omits `-fanalyzer`. See the "Deliberately absent" list in the top-level `README.md`: the analyzer bails out on exactly the reactor code that it would need to analyze. That decision predates this entry. The blocker arose in an exploratory analyzer run over `unsafe/net.backend.cc`. There is no `PINS.md` entry.
