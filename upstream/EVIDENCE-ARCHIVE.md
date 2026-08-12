# Evidence archive

This file is the zero-loss catalog of the off-repository evidence. It lets a
fresh agent find every artifact with no conversation context. Each entry
`README.md` in this directory holds the per-bug analysis. This file holds the
raw evidence locations.

## Evidence root

- The root is `/Users/bjorn/finch-gcc16/` on the host. That directory survives
  reboots. `/tmp` does not.
- The memory watchdog lived at `/tmp/buildguard.sh` during the campaign. A copy
  now also lives in this repository, at `upstream/buildguard.sh`.

## Toolchains in the volume

- `gcc16/` — self-built GCC 16.1.0, aarch64-linux.
- `gcc17/` — self-built GCC master, commit
  `475e9efffaf8de781d7e17b687faf1807e104b01`, built 2026-08-12.
- Relaunch pattern:
  `finch run --rm --memory 20g -v /Users/bjorn/finch-gcc16:/work <image>`,
  then inside the container set `PATH=/work/gcc16/bin:$PATH` (or `gcc17`) and
  `LD_LIBRARY_PATH=/work/gcc16/lib64` (or `/work/gcc17/lib64`).
- Official image: `gcc:16.1.0` (already pulled). Its compiler is on the default
  `PATH`; it needs no volume toolchain.

## Per-directory catalog

Each line names the artifact and states what it proves.

- `logs/`, `logs2/`, `logs3/`, `logs4/` — the LTO/modules/reflection OOM matrix
  rounds. They prove that Linux does NOT reproduce the Darwin memory explosion.
- `logs5/` — the ELF fat-LTO DWARF check. `dwarfdump --verify` reports the ELF
  fat objects clean. This is the control that decides the Bugzilla component
  for the PR 82005 comment.
- `logs6.log` and `fdleak-linux-full.txt` — the fd-leak Linux runs. They show
  zero throwing-edge events on glibc.
- `trunkcheck/` — the master build check: `master-commit.txt`,
  `trunk-matrix.txt`, `source-checks.txt`. It holds the trunk runs for every
  reproduction, including the PR 126783 recheck.
- `official/` — the official-image verification: `matrix.txt`,
  `official-run.log`, the per-cell logs, and the official-build preprocessed
  sources (`ice-repro.official.ii`, `fd-repro.official.i`,
  `fd-minimal.official.ii`; copies now also sit in the two analyzer entry
  directories of this repository).
- `FINDINGS.md` — the Linux campaign summary.
- `linux-vm-lab/` — the QEMU/Guix VM lab: `results-final.tgz` holds the
  exact-project-replica matrix, `scratch.qcow2` holds a complete Linux
  GCC 16.1.0 install, and `run-vm.sh` plus `vmkey` relaunch the VM.
- `analyzer-check/` and `cells*/` — reproduction staging directories.

## Kernel-panic history

One unguarded run of the lto-oom archive-link reproduction caused a kernel
panic on the development machine. The root cause: the static archives fed
dsymutil invalid `__DWARF` sections from `-flto -g` fat objects, and dsymutil
grew without bound. Therefore every reproduction must run under `buildguard.sh`
or under an explicit `ulimit -v` cap.

## Bugzilla access

The Bugzilla HTML UI blocks automated fetches with an Anubis bot-check
interstitial. Plain curl and the REST API work:
`https://gcc.gnu.org/bugzilla/rest/bug/<id>/comment` and
`https://gcc.gnu.org/bugzilla/rest/bug?quicksearch=...`. GCC policy still
requires a duplicate search in a real browser before filing.
