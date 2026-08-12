# Darwin `-flto -g` objects contain invalid DWARF and cause `dsymutil` to grow without limit

Status: the analysis is complete. We verified the reproduction under the guard. The Linux check is also complete: the defect is Mach-O-only (see the section "Linux control (answered)"). The entry is ready to comment (see "Suggested upstream destination"). The duplicate search is complete (2026-08-11): see "Related reports".

DO NOT run this reproduction without the guard. The full-project form grew one `dsymutil` process to 67 GB of RSS in 13 seconds on a 96 GB machine. An earlier run without the guard used approximately 500 GB of swap in 13 minutes and caused a kernel panic on the host. Set limits on memory and on wall time. Kill `dsymutil` yourself: the compiler driver runs it as a grandchild of `collect2`. If you kill only the driver, `dsymutil` becomes an orphan and continues to grow.

The historical entry name says `lto1`. The measured runaway process is Apple `dsymutil`. The invalid object DWARF that GCC writes with `-flto -g` causes the growth.

## Symptom

The repository dev preset (Debug: `-O0 -g`) with `CMAKE_INTERPROCEDURAL_OPTIMIZATION=ON` compiles every translation unit. The link of `examples/starter_httpd` then never completes. Memory grows without limit in `dsymutil`. The GCC driver starts `dsymutil` automatically after each successful `-g` link on Darwin. The identical link without `-g` completes in approximately 5 seconds. The Release LTO build (`-O3`, no `-g`) also links in seconds.

Two separate defects cause the failure:

1. GCC 16.1.0 with `-flto -g` on `aarch64-apple-darwin24` writes Mach-O objects that contain invalid `__DWARF` sections. `dwarfdump --verify` reports out-of-bounds `DW_AT_stmt_list` offsets, dangling DIE references, and invalid `DW_FORM` attributes. The same source without `-flto` shows no verification errors. GCC shows no diagnostic message.
2. Apple `dsymutil` reads that invalid DWARF through the `N_OSO` debug map of the linked executable, and it does not stop. It writes two warning lines for each unresolved reference. It walks the references again without end (37 million warnings and 3.7 GB of stderr in 600 seconds from a 4-file program). At project scale, it allocates tens of GB each second.

Static archives are the necessary path. Darwin has no linker plugin, thus `collect2`/`lto-wrapper` rewrites only the objects named on the command line. Archive members reach `ld64` as fat objects. The debug map of the executable continues to point at the original members, and `dsymutil` reads their invalid `__DWARF`. The LTO recompile rewrites loose objects, and the same graph then links with no warnings.

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

We ran each command with a watchdog. The watchdog kills the run at 8 GB of system swap growth, at 12 GB of process RSS, or at 600 s. A companion process kills `dsymutil` above 6 GB of RSS. Keep equivalent limits.

```sh
F="-std=c++26 -fmodules -O0 -g -flto=auto"
g++-16 $F -fsearch-include-path -c bits/std.cc -o std.o
g++-16 $F -c p.cc -o p.o
g++-16 $F -c mprim.cc -o m.o
g++-16 $F -c pmain.cc -o main.o
ar rcs libm.a p.o m.o
g++-16 -O0 -g -flto=auto main.o std.o libm.a -o app   # <- never completes
```

The compile steps complete correctly. The object DWARF is already invalid:

```text
$ dwarfdump --verify p.o
error: DW_AT_stmt_list offset out of bounds occurred 1 time(s).
error: File index in DW_AT_decl_file reference CU with no line table occurred 6450 time(s).
error: Invalid DIE reference occurred 1670 time(s).
```

The link step runs `ld64` correctly. Then the `dsymutil` process, which the driver starts, floods stderr and does not stop:

```text
warning: could not find referenced DIE
note: while processing libm.a(p.o)
warning: could not find referenced DIE
note: while processing libm.a(p.o)
...
```

Observation under the guard after 600 s: 37,340,227 copies of the warning, 3.7 GB of stderr, and `dsymutil` continues to run. When the guard kills the process tree, the driver reports:

```text
collect2: fatal error: /usr/bin/dsymutil terminated with signal 9 [Killed: 9]
compilation terminated.
```

Expected result: the link and the debug-map step complete. They complete without `-flto`: the plain dev build of the same graph links, and `dwarfdump --verify` reports `No errors.` on every object. They also complete with `-flto` and without `-g`.

## Full-project scale (memory explosion)

Configure the repository dev preset with IPO set to ON. Then link `examples/starter_httpd` (9 objects; the build puts the module set into the archive `libstarter_module.a`):

```sh
cmake -S . -B build-devlto -G Ninja -DCMAKE_CXX_COMPILER=g++-16 \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
ninja -C build-devlto -j1 examples/starter_httpd
```

All 25 compile/scan steps complete correctly. During the link, we measured the `dsymutil` RSS at 1-second intervals (`dsymutil-rss-growth.txt` contains the full series):

```text
1s   1994MB     8s  23868MB
2s   9450MB    10s  44108MB
3s  17101MB    11s  54118MB
5s  26690MB    12s  61305MB
7s  37462MB    13s  66957MB   <- ~67 GB, then compressor thrash 23-67 GB
```

The guard killed the run when the system swap went above its limit:

```text
BUILDGUARD KILL: swap=8375MB max_rss=0MB elapsed=96s (limits: 8192/12288/600)
```

The value `max_rss=0` occurs because the guard monitors only `cc1plus`/`lto1`. The runaway process is `dsymutil`. The orphaned `dsymutil` continued after the driver died. Four minutes later, it held approximately 24.4 GB of RSS with 19.2 GB of swap in use. It continued to grow until we killed it by hand. Its stderr held 13,679,041 `note: while processing` lines that named only `libstarter_module.a(starter.cc.o)` and `(core.cc.o)`. The process never got past the second archive member. This run also printed `warning: Cann't load line table.` two times (verbatim, with the typo).

The identical link command without `-g` completes in 5 seconds and makes a functional 16 MB executable.

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

The failure needs this combination: `-g` AND `-flto` (any N) AND module objects that reach `ld64` inside a static archive. The choice of `-O0` or `-O2` has no effect. Modules and `import std` are not necessary to make the DWARF invalid or to make `dsymutil` warn. But their debug volume (about 880 KB of `__debug_info`, roughly 1,700 dangling references per importer TU) changes a bounded flood into an unbounded one. The plain-TU control completes.

## Analysis

With `-flto -g` on Darwin, GCC writes each object two times: LTO bytecode plus early debug data in `__GNU_DWARF_LTO`, and fat machine code plus regular `__DWARF` sections. (Darwin uses fat objects by default because it has no linker plugin.) The `__DWARF` copy is the invalid one. Its `DW_AT_stmt_list` offset (for example `0x0008f7ac` in `p.o`) points far beyond its own `__debug_line`, and the DIE references dangle. It appears that GCC resolves the attribute values against the early-debug section layout and then writes them into the fat-code sections. Thus each consumer of the fat half sees structurally invalid DWARF. The consumers are: the debug notes of `ld64`, `dsymutil`, and each debugger that reads the `.o` through the `N_OSO` map of the executable. `cc1plus` shows no warning that `-flto -g` makes unusable debug data on this target.

The archive path makes the defect visible. `collect2`/`lto-wrapper` recompiles with LTO only the objects named on the link line. Thus archive members supply their fat code directly, and the debug map continues to refer to them. The Darwin driver then always runs `dsymutil` after a `-g` link. The behavior of `dsymutil` on the invalid input is itself pathological: two stderr lines for each bad reference, apparent repeated walks (42 M warnings from 1,670 bad references in one member), and unbounded allocation at scale. But `dsymutil` receives the invalid input first.

## Code path (GCC 16.1.0 sources)

The defect has a documented history and a visible code path. We verified each location in the local 16.1.0 tree.

- `gcc/dwarf2out.cc:33734-33740` (`dwarf2out_early_finish`): GCC emits early LTO debug for every `-flto -g` compile, except on PE-COFF and COFF targets. A FIXME there names the reason for that exclusion: libiberty cannot copy the early-debug sections on those formats. Mach-O has the same gap, but no exclusion. The Mach-O `simple-object` backend sets `copy_lto_debug_sections` to NULL (`libiberty/simple-object-mach-o.c:1362-1373`); the generic wrapper then reports "`simple_object_copy_lto_debug_sections` not implemented" (`libiberty/simple-object.c:348-351`). Thus GCC writes early debug that no Darwin tool can extract.
- The GCC 8 workaround for PR 82005 (r258123) is still present: `darwin_asm_lto_start` sets `debug_info_level = DINFO_LEVEL_NONE` (`gcc/config/darwin.cc:2128-2142`). That hook brackets only the LTO IL streaming (`gcc/cgraphunit.cc:2273-2297`). `dwarf2out_early_finish` runs outside the bracket (`gcc/cgraphunit.cc:2619`). The workaround therefore suppresses neither the early `__GNU_DWARF_LTO` output nor the late `__DWARF` output.
- At link time, `lto-wrapper` searches each object for the ELF section name `.gnu.debuglto_.debug_info` (`debug_objcopy`, `gcc/lto-wrapper.cc:1237-1246`). On Mach-O the search fails, `debug_objcopy` returns NULL without a message, and the driver continues without any early debug (`gcc/lto-wrapper.cc:1942-1947`). This explains the silence: no component reports the loss.
- `darwin_asm_output_dwarf_offset` (`gcc/config/darwin.cc:3103-3132`) emits every DWARF section offset as an assembly-time label difference (`label - Lsection__debug_*`), not as a relocation. The function comment states the design assumption: "either the section never leaves the .o file, or the linker or other tool is responsible for parsing the DWARF and updating the offsets." With `-flto -g`, one object holds two parallel DWARF section sets (`__GNU_DWARF_LTO` and `__DWARF`) with two label generations (`init_sections_and_labels`, `gcc/dwarf2out.cc:29961-30167`). A reference that resolves against a label in the other set bakes a fixed cross-section constant into the object. No relocation remains for any later tool to check or repair. This mechanism agrees with the observed out-of-bounds `DW_AT_stmt_list` and the dangling DIE references, and with the clean ELF control, where the same references stay relocations against separate sections. The cross-unit DIE references use a weak hidden symbol on the early CU (`output_comp_unit`, `gcc/dwarf2out.cc:11326-11345`); symbol-minus-label expressions of this kind produced the original PR 82005 assembler errors.

## Documented expectation

The GCC 16 manual limits the promise in one sentence (`gcc/doc/invoke.texi:16003-16005`, under `-flto`): "Link-time optimization does not work well with generation of debugging information on systems other than those using a combination of ELF and DWARF." Darwin uses DWARF without ELF, so the caveat covers this target. But the caveat promises degraded quality, not structurally invalid `__DWARF` sections, and GCC prints no diagnostic when it writes them. The report should quote this sentence and ask for correct output or a hard diagnostic (or the same exclusion that PE-COFF/COFF already has in `dwarf2out_early_finish`).

## Related reports (verified 2026-08-11)

No existing report shows this defect as filed here: invalid `__DWARF` in fat Mach-O objects proven with `dwarfdump --verify`, plus quantified unbounded `dsymutil` growth. The closest reports:

- [GCC PR 82005](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=82005) "Early lto debug not implemented on Darwin" (target, NEW since 2017-08-28, last change 2023-11-27) — the umbrella report; comment 7 (2017) shows the identical `could not find referenced DIE` flood, and comment 21 (2018) reports an endless `dsymutil` loop with ~4 GB of stderr from one asan test. It differs: it centers on assembler errors and testsuite noise, carries no `dwarfdump --verify` proof on the objects, and has no memory data. File the new report with PR 82005 in "See Also"; triage may fold it in, and the new evidence belongs upstream either way.
- GCC PRs 82031, 82124, 83960 — marked duplicates of PR 82005; they add no separate analysis.
- GCC r258123 (2018-03-02; gcc-patches "[PATCH][darwin] Work around missing LTO debug support for Mach-O, PR82005") — the committed workaround; it disables debug info only while GCC streams the LTO IL, and 16.1.0 still contains it. It differs: it does not cover the DWARF output paths that produce this defect (see "Code path").
- [GCC PR 84645](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=84645) (RESOLVED FIXED) — ICE fallout of that workaround (`-flto -g0` objects with a `-g` link). It differs: a compile-time ICE, fixed in 2018.
- [GCC PR 112718](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=112718) (RESOLVED FIXED) — ICE with `-g -fdebug-types-section -flto -ffat-lto-objects`; a "See Also" of PR 82005. It differs: the compiler crashes instead of writing invalid output.
- [GCC PR 105877](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105877) (NEW) — GNU `strip` corrupts `-flto -g` objects on ELF. It differs: there a consumer damages valid sections; here the producer writes invalid sections.
- [GCC PR 88389](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=88389) (SUSPENDED) — `-flto -g -gsplit-dwarf` is broken. It differs: the split-DWARF axis on ELF.
- gcc-patches: ["Heads-Up: early LTO debug to land, breaking Mach-O / [X]COFF"](https://inbox.sourceware.org/gcc-patches/alpine.LSU.2.20.1705121115430.20726@zhemvz.fhfr.qr/) (Richard Biener, 2017-05-12) — announced this exact breakage class before the early-LTO-debug merge.
- gcc-patches: ["Allow target to emit LTO early debug to a separate LTO file"](https://inbox.sourceware.org/gcc-patches/E720EF9A-F876-4998-8C40-27B736A9E269@sandoe.co.uk/) (Iain Sandoe, 2018-08-22) and ["[PATCH, lto-wrapper] Don't pre-link the early-debug objects"](https://inbox.sourceware.org/gcc-patches/83B774F4-E946-4F6F-9268-7783525D152B@sandoe.co.uk/) (Iain Sandoe, 2018-12-23) — proposed Darwin enablement work; the Mach-O copy support is absent from 16.1.0.
- [llvm-project issue #102965](https://github.com/llvm/llvm-project/issues/102965) "Non-terminating dsymutil invocation w/LTO" (open, 2024-08-12, label LTO) — GCC 14.1 on macOS, a static archive member (`libgtest_main.a(gtest_main.cc.o)`), the identical endless warning; disabling IPO suppresses it. The nearest consumer-side report. It differs: no memory data, no producer diagnosis, and no resolution.
- [llvm-project PR #195363](https://github.com/llvm/llvm-project/pull/195363) (merged 2026-05) — the parallel DWARFLinker now treats NULL DIE references as resolution failures; the patch also fixes the `Cann't` warning typo that our full-project log shows verbatim. It differs: it covers `--linker parallel` only; the installed Apple dsymutil 17 defaults to the classic linker (its `--help` states `Defaults to 'classic'`).

Searches that found nothing further (clean-duplicate evidence): GCC Bugzilla REST quicksearch, including resolved bugs, for `lto debug darwin`, `flto -g darwin`, `dsymutil`, `early debug lto`, `simple-object mach-o`, `debuglto`, `lto wrong-debug`, `mach-o dwarf lto`, `darwin lto`, `invalid DWARF`, `stmt_list`, `dwarfdump`, `could not find referenced DIE`; llvm-project issues for `dsymutil` with `hang`, `infinite`, `OOM`, `unbounded`, `memory`. No GCC report describes the fat-object `__DWARF` corruption in any released GCC, and no LLVM report quantifies the dsymutil memory growth.

One mitigation note for the Apple report: the installed Apple dsymutil 17 accepts `--verify-dwarf=input` (supported modes: `none`, `input`, `output`, `all`, `auto`; verified against the binary). The GCC driver passes no verify flag. We did not run input verification against the explosive link.

## Suggested upstream destination

1. GCC side: we do NOT file a new GCC PR. PR 82005 ("Early lto debug not implemented on Darwin", open since 2017) already covers this defect class, and its thread shows the identical `dsymutil` warning flood (comment 7) and an endless `dsymutil` loop (comment 21). The action is ONE comment on PR 82005. The comment adds what the thread lacks:
   - the `dwarfdump --verify` proof that the emitted `__DWARF` is invalid (out-of-bounds `DW_AT_stmt_list`, dangling DIE references)
   - the memory growth data (`dsymutil-rss-growth.txt`: 67 GB of RSS in 13 seconds; the guard-kill record)
   - the archive-path mechanism (archive members bypass the LTO recompile, so the debug map keeps pointing at the invalid `__DWARF`)
   - the PE-COFF exclusion precedent in `dwarf2out_early_finish` (`gcc/dwarf2out.cc`, the FIXME that names `copy_lto_debug_sections`) — a one-line-fix lever for a matching Mach-O exclusion
   - the minimal 4-file reproduction
   The comment must acknowledge the documented limitation in `invoke.texi` (see "Documented expectation"). It must aim at the undocumented part: GCC gives no diagnostic, and the driver still runs `dsymutil` on the known-bad output. Do not present the comment as a modules bug; the `hello.cc` control shows the invalid DWARF without modules.
2. Consumer side: the action is one comment on llvm-project issue #102965 (the open identical endless-dsymutil report; GCC 14, static archive member, same warning). That comment adds our memory quantification (`dsymutil-rss-growth.txt`, the 37.3 M warning count) and refers to PR #195363 (NULL-DIE-reference fix, parallel linker only) and to `--verify-dwarf=input` as a possible guard that the driver does not use. The separate Apple Feedback follows, per `upstream/TODO.md` (after the PR 82005 comment exists, in agreement with the `gcc-fixincludes-darwin-rsize-t` precedent): `dsymutil` must limit its work on invalid input, deduplicate the warning, and fail instead of allocate hundreds of GB.

## Linux control (answered)

The corruption is only in the Mach-O/darwin emission path. The Linux control ran on aarch64-unknown-linux-gnu with the same GCC 16.1.0 sources, self-built. `llvm-dwarfdump --verify` reports **No errors** on ELF fat LTO module objects built with `-O0 -g -flto -ffat-lto-objects`. The archive-through-linker path, which causes the memory explosion on Darwin, links and runs in seconds on Linux. Two independent Linux environments (a Debian trixie container; Guix System under QEMU/HVF) reproduce nothing at any point of the matrix. The matrix covers:

- small and heavy module graphs
- `import std`
- `-freflection`
- partitions
- an exact replica of the TU graph of this project at `-O0 -g -flto=auto`
- a 4x-scale graph
- a mimicked plugin-less flow (`-ffat-lto-objects -fno-use-linker-plugin`)

All links complete in 0–2 s with peak process RSS ≤ 689 MB, versus approximately 500 GB of swap consumption on Darwin. Forced single-partition WPA over the full 21-TU graph on Linux completed in 0.36 s with 104 MB. File the report against the Darwin target side of the debug emission, and attach the ELF control.

## Local workaround

The repository limits whole-program optimization to Release only (`CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON`; the Release build has no `-g`). Thus no configuration of this project links LTO objects while `-g` is active. See `PINS.md`.
