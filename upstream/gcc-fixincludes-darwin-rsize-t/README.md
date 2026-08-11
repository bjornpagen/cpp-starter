# gcc-fixincludes-darwin-rsize-t — filed; live artifact retained

The Bugzilla report, the fixincludes patch, and the Apple Feedback material
were all submitted on 2026-08-10 and removed from the tree (recover any of it
from git history before commit `e0a9d27^` if a maintainer asks).

`fixed-header.h` stays: it is the production copy of the local fixinclude that
every toolchain rebuild drops into GCC's `include-fixed/sys/_types/` until the
upstream patch lands — see the `macos-rsize-t-fixinclude` entry in `PINS.md`.
Delete this directory when that pin retires.
