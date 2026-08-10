# Empty libstdc++ module interface files

File this report after Bugzilla creates the Darwin `rsize_t` report.

Replace every `DARWIN_PR_NUMBER` placeholder with the Darwin PR number.

Replace `PATCH_ARCHIVE_URL` with the public gcc-patches archive URL.

## Step 1: set the Bugzilla fields

Open [GCC Bugzilla](https://gcc.gnu.org/bugzilla/enter_bug.cgi?product=gcc).

Set these fields:

| Field | Value |
|---|---|
| Product | `gcc` |
| Component | `libstdc++` |
| Version | `17.0` |
| Known to fail | `16.1.0, 17.0` |
| Known to work | Leave empty |

Add these reports to See Also:

- `https://gcc.gnu.org/bugzilla/show_bug.cgi?id=124268`
- `https://gcc.gnu.org/bugzilla/show_bug.cgi?id=124554`
- `https://gcc.gnu.org/bugzilla/show_bug.cgi?id=DARWIN_PR_NUMBER`

Do not attach a patch.

## Step 2: paste the title

```text
libstdc++: module fallback installs empty interface units referenced by manifest
```

## Step 3: paste the report body

```text
PR libstdc++/124268 added a fallback for failed std module compilation. The fallback lets a supported GCC bootstrap continue.

The fallback also creates an invalid installation.

Current GCC trunk contains these unconditional install lists:

toolexeclib_DATA = libstdc++.modules.json
includebits_DATA = std.cc std.compat.cc

The failure rule replaces the generated module source with an empty file:

std.lo: std.cc
	if ! $(LTCXXCOMPILE) $(MODULES_FLAGS) -c $< ; then \
	  echo "Cannot compile std module" >&2; \
	  echo "Module initialization function will be missing" >&2; \
	  echo > $<.tmp && mv $<.tmp $< && \
	  $(LTCXXCOMPILE) $(MODULES_FLAGS) -c $< ; \
	fi

The std.o, std.compat.lo, and std.compat.o rules use the same fallback. The rules are in libstdc++-v3/src/c++23/Makefile.am.

When module compilation fails, the build performs these actions:

1. The build replaces std.cc or std.compat.cc with a one-byte file.
2. The build compiles that empty file as a fallback object.
3. The build exits successfully.
4. The install step installs the empty source file.
5. The install step installs a manifest that lists the empty file as a module interface.

The fallback therefore preserves the bootstrap. The installed manifest then claims that an unavailable interface exists.

This failure occurred in a GCC 16.1.0 Homebrew package for Apple Silicon. The package installed one-byte std.cc and std.compat.cc files. Its libstdc++.modules.json file listed both files.

Homebrew report:

https://github.com/Homebrew/homebrew-core/issues/289142

CMake rejected each file with this error:

is of type CXX_MODULES but does not provide a module interface unit or partition

PR target/DARWIN_PR_NUMBER describes the Darwin module failure that exposed this state. The install bug is target-independent. Any handled module compile failure produces the same files.

The related GCC patch is public here:

PATCH_ARCHIVE_URL

PR libstdc++/124554 also exercised the fallback on supported offload targets. Its comments confirm that a successful bootstrap was intentional.

The r16-8714-g9d02b118ee10506f90c7cf0e439108f48d370fc1 commit called this fallback temporary. The commit requested removal during GCC 17 stage 1. GCC fixed PR124554, but the fallback remains on GCC 17 trunk.

Expected result:

A successful fallback must not install an empty file as a module interface. The manifest must list only available interface units.

Possible fixes include these options:

1. Remove the temporary fallback.
2. Keep the fallback object, but omit unavailable interfaces from installation.
3. Compile a separate empty source file, and omit unavailable interfaces from the manifest.

Observed environment:

- GCC 16.1.0 release sources with the documented Darwin arm64 port series
- Build, host, and target: aarch64-apple-darwin24
- macOS 26.2 SDK from Xcode 26.3
- Installed bits/std.cc size: one byte
- Installed bits/std.compat.cc size: one byte
- Installed libstdc++.modules.json listed both files

Configure command:

../gcc-16.1.0/configure \
  --prefix=$HOME/.gcc/versions/16.1.0 \
  --enable-languages=c,c++ \
  --disable-nls \
  --enable-checking=release \
  --program-suffix=-16 \
  --with-system-zlib \
  --build=aarch64-apple-darwin24 \
  --with-sysroot=<Xcode MacOSX.sdk>

The upstream make rules and PR124554 provide the primary evidence. The Darwin package provides a real installed example.
```

## Verified source location

The fallback remains in fetched GCC master `cee53ed42c753a0c936e005b7dd15f029ca34da7`.

Relevant lines:

- `libstdc++-v3/src/c++23/Makefile.am:33-35`
- `libstdc++-v3/src/c++23/Makefile.am:103-130`

## Duplicate check

The Bugzilla search found no report for empty installed interfaces that remain in the manifest.

These reports describe different problems:

- PR124268 added the fallback and module initialization symbols.
- PR124554 describes the supported-target compilation failure that required the fallback.
- PR125460 describes `std.cc` with `-ffreestanding`.
- PR124714 describes missing long-double support in `std.cc`.
- PR119266 describes an incorrect manifest path.
- PR125956 describes a Darwin PowerPC build failure.
