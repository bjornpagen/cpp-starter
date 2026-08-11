# Darwin `-flto -g` objects carry invalid DWARF and send dsymutil into unbounded growth

Status: analysis complete, reproduction verified under guard. Check Linux before filing (a Linux run is being arranged separately).

DO NOT run this reproduction unguarded. The full-project form grew one `dsymutil` process to 67 GB of RSS in 13 seconds on a 96 GB machine. An earlier unguarded run consumed about 500 GB of swap in 13 minutes and kernel-panicked the host. Cap memory and wall time, and kill `dsymutil` yourself: the compiler driver runs it as a `collect2` grandchild, so killing the driver orphans it and it keeps growing.

The historical entry name says `lto1`. The measured runaway process is Apple `dsymutil`. GCC's invalid `-flto -g` object DWARF is what drives it.

## Symptom

The repository dev preset (Debug: `-O0 -g`) with `CMAKE_INTERPROCEDURAL_OPTIMIZATION=ON` compiles every translation unit. The `examples/starter_httpd` link then never completes. Memory grows without bound in `dsymutil`, which the GCC driver invokes automatically after every successful `-g` link on Darwin. The identical link without `-g` completes in about 5 seconds. The Release LTO build (`-O3`, no `-g`) also links in seconds.

Two separable defects produce the failure:

1. GCC 16.1.0 with `-flto -g` on `aarch64-apple-darwin24` emits Mach-O objects whose `__DWARF` sections are invalid. `dwarfdump --verify` reports out-of-bounds `DW_AT_stmt_list` offsets, dangling DIE references, and invalid `DW_FORM` attributes. The same source without `-flto` verifies clean. GCC prints no diagnostic.
2. Apple `dsymutil` consumes that invalid DWARF through the linked executable's `N_OSO` debug map and never gives up: it prints two warning lines per unresolved reference, re-walks endlessly (37 million warnings, 3.7 GB of stderr, in 600 seconds from a 4-file program), and at project scale allocates tens of GB per second.

Static archives are the necessary conduit. Darwin has no linker plugin, so `collect2`/`lto-wrapper` rewrites only command-line objects; archive members reach `ld64` as fat objects, the executable's debug map keeps pointing at the original members, and `dsymutil` reads their invalid `__DWARF`. Loose objects are rewritten by the LTO recompile, and the same graph links clean.

## Environment

- GCC 16.1.0, self-built, `/Users/bjorn/.gcc/current`, target `aarch64-apple-darwin24`
- Apple dsymutil: `Apple LLVM version 17.0.0` (Xcode toolchain), `ld` `PROJECT:ld-1230.1`
- macOS 15.7.7 (24G720), Apple Silicon (M2 Max), 96 GB RAM
- CMake 4.2.1, Ninja 1.13.2 (project form only; the minimal form needs neither)

## Files

- `p.cc`, `mprim.cc`, `pmain.cc` — minimal module graph: partition `m:p` imports `std`, primary module `m`, one importer with `main`
- `hello.cc`, `hmain.cc` — plain-TU control pair (no modules)
- `dsymutil-rss-growth.txt` — 1-second RSS/swap samples from the guarded full-project link, with the guard kill line
- `link-warning-sample.txt` — verbatim head of the dsymutil warning flood and the guarded-kill tail

## Reproduction (minimal, 4 files, verified under guard)

Every command was run with a watchdog that kills on 8 GB system swap growth, 12 GB process RSS, or 600 s, plus a companion that kills `dsymutil` above 6 GB RSS. Keep equivalent caps.

```sh
F="-std=c++26 -fmodules -O0 -g -flto=auto"
g++-16 $F -fsearch-include-path -c bits/std.cc -o std.o
g++-16 $F -c p.cc -o p.o
g++-16 $F -c mprim.cc -o m.o
g++-16 $F -c pmain.cc -o main.o
ar rcs libm.a p.o m.o
g++-16 -O0 -g -flto=auto main.o std.o libm.a -o app   # <- never completes
```

The compile steps succeed. The object DWARF is already wrong:

```text
$ dwarfdump --verify p.o
error: DW_AT_stmt_list offset out of bounds occurred 1 time(s).
error: File index in DW_AT_decl_file reference CU with no line table occurred 6450 time(s).
error: Invalid DIE reference occurred 1670 time(s).
```

The link step runs `ld64` successfully, then the driver-spawned `dsymutil` floods stderr and never terminates:

```text
warning: could not find referenced DIE
note: while processing libm.a(p.o)
warning: could not find referenced DIE
note: while processing libm.a(p.o)
...
```

Guarded observation after 600 s: 37,340,227 copies of the warning, 3.7 GB of stderr, `dsymutil` still running. When the guard kills the process tree, the driver reports:

```text
collect2: fatal error: /usr/bin/dsymutil terminated with signal 9 [Killed: 9]
compilation terminated.
```

Expected result: the link and the debug-map step complete, as they do without `-flto` (the plain dev build of the same graph links and `dwarfdump --verify` reports `No errors.` on every object), and as they do with `-flto` without `-g`.

## Full-project scale (memory explosion)

Configure the repository dev preset with IPO forced on, then link `examples/starter_httpd` (9 objects; the module set is archived into `libstarter_module.a`):

```sh
cmake -S . -B build-devlto -G Ninja -DCMAKE_CXX_COMPILER=g++-16 \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
ninja -C build-devlto -j1 examples/starter_httpd
```

All 25 compile/scan steps succeed. During the link, `dsymutil` RSS measured at 1-second intervals (`dsymutil-rss-growth.txt` has the full series):

```text
1s   1994MB     8s  23868MB
2s   9450MB    10s  44108MB
3s  17101MB    11s  54118MB
5s  26690MB    12s  61305MB
7s  37462MB    13s  66957MB   <- ~67 GB, then compressor thrash 23-67 GB
```

The guard killed the run when system swap crossed its cap:

```text
BUILDGUARD KILL: swap=8375MB max_rss=0MB elapsed=96s (limits: 8192/12288/600)
```

`max_rss=0` is the guard watching only `cc1plus`/`lto1`: the runaway is `dsymutil`. The orphaned `dsymutil` continued after the driver died; four minutes later it held ~24.4 GB RSS with 19.2 GB of swap used and was still growing when killed by hand. Its stderr held 13,679,041 `note: while processing` lines naming only `libstarter_module.a(starter.cc.o)` and `(core.cc.o)` — it never got past the second archive member. This run also printed `warning: Cann't load line table.` twice (verbatim, including the typo).

The identical link command without `-g` completes in 5 seconds with a working 16 MB executable.

## Trigger matrix (each cell verified under guard)

| Variation | Object DWARF (`dwarfdump --verify`) | Link result |
|---|---|---|
| `-O0 -g`, no `-flto`, modules | `No errors.` | completes (normal dev build) |
| `-O0 -g -flto=auto`, modules, loose objects | invalid (1670 dangling refs in `p.o`) | completes, 0 warnings — LTO recompile replaces the debug map |
| `-O0 -g -flto=auto`, modules, archive | invalid | never completes; 37.3 M warnings in 600 s; guard kill |
| `-O0 -g -flto=auto`, plain TU, archive | invalid (321 dangling refs) | completes with 3,974 warnings |
| `-O2 -g -flto=auto`, modules, archive | invalid (1650 dangling refs) | never completes; 39.7 M warnings in 600 s; guard kill |
| `-O0 -g -flto=1` (object side) | invalid, identical counts to `-flto=auto` | not separately linked; archives bypass the LTO plugin, so the partition count is irrelevant |
| `-O0 -flto=auto`, no `-g`, modules, archive | n/a (`dsymutil` not invoked) | completes in seconds (also full project) |

Necessary combination: `-g` AND `-flto` (any N) AND the module objects reaching `ld64` inside a static archive. `-O0` versus `-O2` is irrelevant. Modules and `import std` are not needed to make the DWARF invalid or to make `dsymutil` warn, but their debug volume (about 880 KB `__debug_info`, roughly 1,700 dangling references per importer TU) is what turns a bounded flood into an unbounded one; the plain-TU control finishes.

## Analysis

With `-flto -g` on Darwin, GCC emits each object twice over: LTO bytecode plus early debug in `__GNU_DWARF_LTO`, and fat machine code plus regular `__DWARF` sections (Darwin defaults to fat objects because it has no linker plugin). The `__DWARF` copy is the broken one: its `DW_AT_stmt_list` offset (for example `0x0008f7ac` in `p.o`) lies far beyond its own `__debug_line`, and DIE references dangle — the attribute values appear to be resolved against the early-debug section layout and then emitted into the fat-code sections. Consumers of the fat half (`ld64`'s debug notes, `dsymutil`, any debugger reading the `.o` through the executable's `N_OSO` map) therefore see structurally invalid DWARF. `cc1plus` prints no warning that `-flto -g` produces unusable debug info on this target.

The archive path makes it observable: `collect2`/`lto-wrapper` LTO-recompiles only objects named on the link line, so archive members contribute their fat code directly and stay referenced by the debug map. Darwin's driver then always runs `dsymutil` after a `-g` link. `dsymutil`'s handling of the invalid input is itself pathological — two stderr lines per bad reference, apparent re-walking (42 M warnings from 1,670 bad references in one member), unbounded allocation at scale — but it is fed garbage first.

## Suggested upstream destination

1. GCC Bugzilla, product `gcc`, component `debug` (triage may move it to `target` as Darwin-specific), version `16.1.0`, keywords `wrong-debug, lto`. Title suggestion: `[Darwin] -flto -g emits invalid __DWARF sections in Mach-O objects (out-of-bounds DW_AT_stmt_list, dangling DIE references); driver-run dsymutil then grows without bound`. Attach `p.cc`, `mprim.cc`, `pmain.cc`, `hello.cc`, `hmain.cc`, the `dwarfdump --verify` output, and `dsymutil-rss-growth.txt`. The `hello.cc` control shows the invalid DWARF without any modules involvement, so the report must not be framed as a modules bug.
2. Apple Feedback (second report, after the GCC report exists, following the `gcc-fixincludes-darwin-rsize-t` precedent): `dsymutil` must bound its work on invalid input — deduplicate the warning, and fail instead of allocating hundreds of GB. Reference the GCC PR for the producer side.

## Linux control (answered)

The corruption is in the Mach-O/darwin emission path only. On aarch64-unknown-linux-gnu
(the same GCC 16.1.0 sources, self-built): `llvm-dwarfdump --verify` reports **No errors**
on ELF fat LTO module objects built `-O0 -g -flto -ffat-lto-objects`, and the
archive-through-linker path that detonates on Darwin links and runs in seconds. Two
independent Linux environments (Debian trixie container; Guix System under QEMU/HVF)
reproduce nothing at any point of a matrix covering small/heavy module graphs,
`import std`, `-freflection`, partitions, an exact replica of this project's TU graph at
`-O0 -g -flto=auto`, a 4x-scale graph, and a mimicked plugin-less flow
(`-ffat-lto-objects -fno-use-linker-plugin`): all links complete in 0–2 s with peak
process RSS ≤ 689 MB, versus ~500 GB swap consumption on Darwin. Forced single-partition
WPA over the full 21-TU graph on Linux: 0.36 s, 104 MB. File against the Darwin target
side of the debug emission, with the ELF control attached.

## Local workaround

The repository pins whole-program optimization to Release only (`CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON`; Release carries no `-g`), so no configuration of this project links LTO objects while `-g` is active. See `PINS.md`.
