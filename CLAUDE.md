# CLAUDE.md

`AGENTS.md` is normative for all code in this repository. Read it before
writing or reviewing any C++. It defines a deliberately small C++26 dialect;
when it forbids a mechanism, do not use that mechanism even if it is idiomatic
C++ elsewhere.

## Commands

```sh
cmake --preset gcc-dev              # configure (debug)
cmake --build --preset gcc-dev      # build
ctest --preset gcc-dev              # test
cmake --preset clang-lint           # configure lint graph (clang-tidy runs during build)
cmake --build --preset clang-lint   # lint
python3 tools/check_policy.py       # repository policy checks (AGENTS.md §34)
```

All presets are defined in `CMakePresets.json`; each builds into
`build/<preset>/`. Never invoke ninja directly or hand-craft cmake command
lines.

## Source zones

- `src/`, `tests/` — dialect code; every AGENTS.md rule applies.
- `meta/` — GCC-only C++26 reflection code; excluded from the clang lint graph.
- `foreign/`, `unsafe/` — quarantine for headers/ABI/preprocessor; must export
  a safe module boundary upward.

## Toolchain notes

The pinned production compiler is GCC 16.1.0 with `-freflection`, built from
source into `~/.gcc/current` by `tools/install-gcc.sh` (MacPorts ships no
gcc16 yet). CMake/Ninja/clang-tidy come from MacPorts. Do not add
compatibility branches for other compilers or older standards; the configure
step rejects unacceptable toolchains.
