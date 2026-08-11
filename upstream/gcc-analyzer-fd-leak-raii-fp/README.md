# Analyzer false-positive fd leaks on canonical RAII fd owners

Status: analysis complete, reproduction verified under guard, root causes traced to source. Check Linux before filing (a Linux run is being arranged separately).

Every compile below is instantaneous and allocates nothing unusual; each was still run under the standard watchdog (`/tmp/buildguard.sh 8192 12288 600 ...`, one compile at a time). The guard never fired.

The original repro material was lost with `/tmp`; this directory re-minimizes it from `unsafe/net.backend.cc` (the `Fd` owner at lines 84–123 and the kqueue accept loop at lines 437–475). The reduction corrects two claims in the historical note: the ~76-line file's 6 warnings collapse into 3 distinct reports here, produced by exactly two independent defects; and the headline defect is not C++-specific at all — a 17-line plain-C translation unit triggers it.

## Symptom

`g++-16 -O2 -fanalyzer` (no extra analyzer flags) emits spurious CWE-775 `-Wanalyzer-fd-leak` warnings on a minimal RAII fd wrapper: an int-owning `Fd` class with close-in-destructor and move semantics, a `Server` struct holding a listener `Fd`, and an accept helper taking `Server&`. Every descriptor in the file is provably owned: the listener stays owned by the caller through the `Server&` parameter and is closed by `Fd::~Fd`, and the accepted fd is either closed by the `Fd` destructor on the `continue`/return paths or moved into slot ownership. The code is valid; every warning is a false positive.

Two independent defects produce the reports:

1. **Caller-owned fd reported as leaking (the headline shape).** Calling `accept(server.listener.get(), ...)` makes the analyzer attach fd state ("listening stream socket") to the value loaded from `server->listener.value_`. When the loading temporary dies — at the very next statement — the state is purged and reported as a leak, anchored at the `accept()` callsite: `leak of file descriptor '((const Fd*)server)[1].Fd::value_'`. The fd is still reachable by the caller through the reference parameter and is never leaked. Plain C with a `struct server *` parameter triggers the identical report at every `-O` level (`repro.c`).
2. **Exception-edge leak through a noexcept destructor.** With `-fexceptions` (the C++ default), GCC 16's new exception modeling assumes every external call not marked `nothrow` can throw. Darwin's SDK headers annotate nothing, so `close`, `fcntl`, and `accept` all bifurcate into a throwing edge, and the fd "leaks" on it — the report is anchored at the very `::close(value_)` call that closes the fd, with the path event `if 'int close(int)' throws an exception...`. The enclosing destructor is implicitly `noexcept`, so even under the false premise the exception could only reach `std::terminate`, never a running caller.

## Environment

- GCC 16.1.0, self-built, `/Users/bjorn/.gcc/current`, target `aarch64-apple-darwin24`
- macOS arm64 (Darwin 24.6.0), Apple Silicon, 96 GB RAM
- CMake 4.2.1, Ninja 1.13.2 (not used by this reproduction; single-TU compiles only)
- Current GCC master retains all three implicated code sites unchanged (read from a local master checkout at commit `14d1f0c9858`, 2026-08-10; trunk was not run locally)

## Files

- `repro.cc` — 91-line faithful minimization of the accept loop: `Fd` owner, `Server` with `queue`/`listener`/slots, accept helper taking `Server&`; 3 warnings, including the historical diagnostic shape `((const Fd*)server)[1].Fd::value_` at the accept callsite
- `repro-minimal.cc` — 29-line reduction showing both defects in one TU (2 warnings)
- `repro.c` — 17-line plain-C reduction of defect 1 only (1 warning; no C++ anywhere)
- `analyzer-output.txt` — complete verbatim `g++-16 -O2 -fanalyzer -c repro.cc` output with all three event paths

## Reproduction (verified under guard)

```sh
g++-16 -O2 -fanalyzer -c repro.cc
```

Verbatim warning lines (full paths in `analyzer-output.txt`; the compile still exits 0 and produces an object):

```text
repro.cc:65:35: warning: leak of file descriptor '*(const Fd*)((char*)&*accept_ready::server + offsetof(Server, Server::listener)).Fd::value_' [CWE-775] [-Wanalyzer-fd-leak]
repro.cc:65:35: warning: leak of file descriptor 'descriptor.Fd::value_' [CWE-775] [-Wanalyzer-fd-leak]
repro.cc:77:87: warning: leak of file descriptor '((const Fd*)server)[1].Fd::value_' [CWE-775] [-Wanalyzer-fd-leak]
```

The two `repro.cc:65` reports are in `set_nonblocking` (the `fcntl` call): the first claims the caller-owned listener leaks there (defect 1), the second claims the accepted fd leaks "if `fcntl` throws" (defect 2). The `repro.cc:77` report claims the listener leaks at its own `accept()` callsite (defect 1); its entire path is three events ending in `(3) '((const Fd*)server)[1].Fd::value_' leaks here` at the call.

Reduced forms:

```sh
g++-16 -O2 -fanalyzer -c repro-minimal.cc
```

```text
repro-minimal.cc:14:32: warning: leak of file descriptor 'accept(*(const Fd*)server.Fd::value_, 0, 0)' [CWE-775] [-Wanalyzer-fd-leak]
repro-minimal.cc:27:79: warning: leak of file descriptor '((const Fd)*server).Fd::value_' [CWE-775] [-Wanalyzer-fd-leak]
```

Line 14 is `::close(value_)` inside `Fd::~Fd` — the analyzer reports the accepted fd as leaking at the exact statement that closes it (the path walks through the close and appends `leaks here`).

```sh
gcc-16 -O2 -fanalyzer -c repro.c
```

```text
repro.c:12:18: warning: leak of file descriptor '*s.listener' [CWE-775] [-Wanalyzer-fd-leak]
```

Expected result for all three: the analyzer finishes with no diagnostic, as it does for the same logic with the fd passed as a plain `int` parameter (defect 1) or with `-fno-exceptions` (defect 2).

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

Necessary conditions, per defect: (1) an fd stored in an object the analyzed function reaches only through a pointer/reference parameter, and any fd-state-machine-relevant call on it (`accept` here); (2) `-fexceptions` plus a libc whose headers do not declare `close`/`fcntl` as non-throwing (Darwin's SDK does not: `int close(int) __DARWIN_ALIAS_C(close);` in `unistd.h`, no nothrow attribute), plus an fd owned by an object with a destructor — every RAII fd wrapper in every default-flags C++ program on this target.

## Analysis

Both defects are visible in the 16.1.0 analyzer sources, and all cited sites are unchanged on current master (commit `14d1f0c9858`).

**Defect 1 — liveness purge of caller-owned fd state.** `fd_state_machine::on_accept` (`gcc/analyzer/sm-fd.cc:2061`) transitions the listener *svalue* — here the value loaded from `server->listener.value_` — to `fd-listening-stream-socket`. sm-state is keyed by svalue, and the only thing keeping that svalue alive is the SSA temporary holding the loaded member; once it dies (immediately after the call), `sm_state_map::on_liveness_change` (`gcc/analyzer/program-state.cc:639`) finds the svalue non-live, `fd_state_machine::can_purge_p` (`sm-fd.cc:2292`) returns false for socket states, and `on_state_leak` fires — which is why the report anchors at the callsite itself with a near-empty path. The rescue hook, `initial_svalue::implicitly_live_p` (`gcc/analyzer/svalue.cc:1427`), keeps an initial value alive only if (a) the store still binds that identical svalue to its region, or (b) it is the initial value of a *parameter of the top-level frame* (`initial_value_of_param_p`, which requires the SSA default-def of a `PARM_DECL`). The initial value of a field reached *through* a parameter satisfies neither, so any fd owned by a caller-visible object and merely used by the analyzed function is reported as leaking. A plausible fix is to extend the liveness test (or the leak check) to treat sm-state on values reachable from live pointer/reference parameters as caller-owned rather than leaked — the same reasoning the analyzer already applies to the parameters themselves.

**Defect 2 — every external call presumed throwing, terminate not modeled.** GCC 16 added exception modeling; `can_throw_p` (`gcc/analyzer/region-model.cc:2220`) returns true for any call under `-fexceptions` unless the decl is `nothrow` or the callee is on the assumed-not-to-throw list — which contains exactly one entry, `"fclose"`, above a `// TODO: populate this list more fully` (`region-model.cc:2203`). Darwin's SDK annotates no POSIX function, so `close`, `fcntl`, and `accept` all get a throwing edge (on glibc, `close` is declared `__THROW`, so this half may not reproduce on Linux — to be checked before filing). On that edge the close's effect is never applied, the frame unwinds (`exploded_graph::unwind_from_exception`, `gcc/analyzer/engine.cc`), the `Fd` dies with it, and the fd is reported leaked. The unwinder walks CFG EH edges and caller frames; nothing in `gcc/analyzer/` models that the destructor is implicitly `noexcept` — a throw from `close` could only reach `std::terminate`, at which point the whole process (and every fd in it) is torn down by the OS, so a leak report is unactionable even under the false throwing premise. `-fanalyzer-assume-nothrow` (added in GCC 16 precisely as a workaround for this class of noise) silences it wholesale, at the cost of assuming *user* C++ callees do not throw either.

The GCC 16 release notes say the analyzer is "now usable on simple C++ examples" while `invoke.texi` still says it "is only suitable for use on C code in this release"; `repro.c` keeps defect 1 squarely inside the supported-C scope, so the primary report cannot be triaged away as a C++ limitation.

## Suggested upstream destination

Two Bugzilla reports, since the defects are independent and one is C-scoped:

1. GCC Bugzilla, product `gcc`, component `analyzer`, version `16.1.0`, keywords `diagnostic`. Title suggestion: `[analyzer] -Wanalyzer-fd-leak false positive on fd owned by caller through pointer/reference parameter (state purged when loading temporary dies)`. Attach `repro.c` (primary, plain C) and `repro-minimal.cc`; both are small enough to inline. Point at the `sm_state_map::on_liveness_change` / `initial_svalue::implicitly_live_p` interaction and note the sites are unchanged on master.
2. GCC Bugzilla, product `gcc`, component `analyzer`, version `16.1.0`, keywords `diagnostic`. Title suggestion: `[analyzer] fd/resource leak false positives from assumed-throwing libc calls (close, fcntl) on targets without nothrow header annotations; noexcept destructors not modeled`. Attach `repro-minimal.cc`; reference the one-entry `get_fns_assumed_not_to_throw` whitelist and ask whether sm-fd's own known functions should be assumed non-throwing, and whether unwinding should stop at noexcept frames. Note `-fanalyzer-assume-nothrow` as the existing blunt workaround.

Duplicate check: web searches for the diagnostic shapes (`-Wanalyzer-fd-leak` false positive with struct members / pointer parameters / RAII destructors, and for the anchored-at-close shape) on 2026-08-11 found no existing report; GCC Bugzilla's own search UI was unreachable from this network (bot-check interstitial), so run a Bugzilla quicksearch for `-Wanalyzer-fd-leak` at filing time.

Linux check (done): `repro-minimal.cc` under `g++ -std=c++26 -fanalyzer -O0` on
aarch64-unknown-linux-gnu (same self-built GCC 16.1.0, Debian trixie, glibc headers)
emits both false positives verbatim — `leak of file descriptor 'descriptor.Fd::value_'`
and the `((const Fd)*server.Server::listener).Fd::value_` shape. Not Darwin-specific
and not dependent on nothrow annotations; file as target-independent.

## Local workaround

None needed. The build deliberately omits `-fanalyzer` (see the top-level `README.md`, "Deliberately absent" list: it bails out on exactly the reactor code it would need to analyze), a decision that predates this entry; the blocker arose in an exploratory analyzer run over `unsafe/net.backend.cc`. No `PINS.md` entry.
