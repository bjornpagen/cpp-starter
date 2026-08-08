#!/bin/sh
# Reproducible build of the pinned production compiler: GCC 16.1.0 with the
# Apple Silicon (aarch64-apple-darwin) patch maintained by GCC's Darwin
# maintainer, as validated by Homebrew CI. Installs bun-style into
# ~/.gcc/versions/<version> with a stable ~/.gcc/current symlink; no
# system-wide state, no sudo, removable with `rm -rf ~/.gcc`.
#
# To bump the pin: update the three variables below (a matching darwin patch
# must exist for the new version), run this script, then re-verify the
# repository presets and README pin table.
set -eu

GCC_VERSION=16.1.0
GCC_SHA256=50efb4d94c3397aff3b0d61a5abd748b4dd31d9d3f2ab7be05b171d36a510f79
PATCH_URL="https://raw.githubusercontent.com/Homebrew/homebrew-core/HEAD/Patches/gcc/gcc-${GCC_VERSION}.diff"

PREFIX="${HOME}/.gcc/versions/${GCC_VERSION}"
BUILD_ROOT="${HOME}/.gcc/build"
SOURCE_DIR="${BUILD_ROOT}/gcc-${GCC_VERSION}"
BUILD_DIR="${BUILD_ROOT}/build-${GCC_VERSION}"

mkdir -p "${BUILD_ROOT}"
cd "${BUILD_ROOT}"

curl -sL -o "gcc-${GCC_VERSION}.tar.xz" \
    "https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/gcc-${GCC_VERSION}.tar.xz"
echo "${GCC_SHA256}  gcc-${GCC_VERSION}.tar.xz" | shasum -a 256 -c -
curl -sL -o "gcc-${GCC_VERSION}.diff" "${PATCH_URL}"

tar xf "gcc-${GCC_VERSION}.tar.xz"
cd "${SOURCE_DIR}"
patch -p1 --quiet < "../gcc-${GCC_VERSION}.diff"
./contrib/download_prerequisites

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

"${SOURCE_DIR}/configure" \
    --prefix="${PREFIX}" \
    --enable-languages=c,c++ \
    --disable-nls \
    --enable-checking=release \
    --program-suffix=-16 \
    --with-system-zlib \
    --build="aarch64-apple-darwin$(uname -r | cut -d. -f1)" \
    --with-sysroot="$(xcrun --show-sdk-path)"

make "-j$(sysctl -n hw.ncpu)" \
    BOOT_LDFLAGS=-Wl,-headerpad_max_install_names \
    LDFLAGS_FOR_TARGET=-Wl,-headerpad_max_install_names
make install-strip

# --- macOS SDK fixinclude ---------------------------------------------------
# The SDK's sys/_types/_rsize_t.h tests __has_feature(modules) and assumes
# clang's stddef.h __need_rsize_t protocol. GCC also reports the modules
# feature but ignores that protocol, so rsize_t stays undefined and the
# libstdc++ `std` module fails to compile — and libstdc++ silently installs
# EMPTY std.cc/std.compat.cc as a fallback, breaking `import std`.
# Install a plain-typedef fixinclude, then rebuild and reinstall the module.
TRIPLE="aarch64-apple-darwin$(uname -r | cut -d. -f1)"
FIX_BODY='#ifndef _RSIZE_T
#define _RSIZE_T
#include <machine/types.h> /* __darwin_size_t */
typedef __darwin_size_t rsize_t;
#endif /* _RSIZE_T */'
for fixdir in \
    "${PREFIX}/lib/gcc/${TRIPLE}/${GCC_VERSION}/include-fixed" \
    "${BUILD_DIR}/gcc/include-fixed"
do
    mkdir -p "${fixdir}/sys/_types"
    printf '%s\n' "${FIX_BODY}" > "${fixdir}/sys/_types/_rsize_t.h"
done

LIBSTDCXX="${BUILD_DIR}/${TRIPLE}/libstdc++-v3"
rm -f "${LIBSTDCXX}/src/c++23/std.cc" "${LIBSTDCXX}/src/c++23/std.compat.cc" \
      "${LIBSTDCXX}/src/c++23/std.lo" "${LIBSTDCXX}/src/c++23/std.compat.lo"
make -C "${LIBSTDCXX}/src/c++23" std.cc std.compat.cc
make -C "${LIBSTDCXX}/src"
make -C "${LIBSTDCXX}" install

# `import std` is broken if the installed module sources are the empty
# fallback; fail loudly instead of producing a subtly broken toolchain.
for module_source in std.cc std.compat.cc; do
    installed="${PREFIX}/include/c++/${GCC_VERSION}/bits/${module_source}"
    if [ "$(wc -c < "${installed}")" -lt 1000 ]; then
        echo "error: ${installed} is the empty fallback; std module build failed" >&2
        exit 1
    fi
done
# -----------------------------------------------------------------------------

ln -sfn "${PREFIX}" "${HOME}/.gcc/current"

"${HOME}/.gcc/current/bin/g++-16" --version
