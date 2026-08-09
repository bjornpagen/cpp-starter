# The :simd partition is quarantine code (its adapter traffics in the raw
# pointers behind the spans), so it lives here — but it is still a partition
# of the single starter module target defined in src/CMakeLists.txt.
#
# One explicitly supported implementation per kernel (AGENTS.md §3.1); no
# source file branches on architecture. The NEON kernel exists only for
# aarch64 targets; elsewhere the stub reports the kernel unavailable.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
	set(STARTER_NEON_IMPL kernels.neon.cc)
else()
	set(STARTER_NEON_IMPL kernels.neon.stub.cc)
endif()

# SVE compiles on any aarch64 toolchain but executes only on SVE hardware,
# which no supported development host has (Apple silicon lacks SVE; CI is
# x86_64). Opt in explicitly when the target machine executes SVE.
option(STARTER_SVE "Build the SVE kernel (target must execute SVE)" OFF)

if(STARTER_SVE)
	if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
		message(FATAL_ERROR "STARTER_SVE requires an aarch64 target (got ${CMAKE_SYSTEM_PROCESSOR})")
	endif()
	set(STARTER_SVE_IMPL kernels.sve.cc)
	set_source_files_properties(kernels.sve.cc PROPERTIES
		COMPILE_OPTIONS -march=armv8.2-a+sve
	)
else()
	set(STARTER_SVE_IMPL kernels.sve.stub.cc)
endif()

target_sources(starter_module
	PUBLIC
		FILE_SET CXX_MODULES
		FILES
			${CMAKE_CURRENT_SOURCE_DIR}/simd.cc
			${CMAKE_CURRENT_SOURCE_DIR}/net.cc
	PRIVATE
		kernels.stdsimd.cc
		${STARTER_NEON_IMPL}
		${STARTER_SVE_IMPL}
)

# The networking swap-boundary TU: the kqueue io-context whose senders are
# spelled in stdexec (the I/O half of the boundary; the combinator half is
# foreign/exec.backend.cc). Same quarantine arrangement as that target and
# for the same pinned reasons: it must stay module-free (GCC 16.1 ICEs when
# stdexec headers are included in any module unit), the vendored headers are
# SYSTEM and visible to this target only, and the probe-verified UBSan
# carve-out plus the -Wnull-dereference drop apply (middle-end diagnostics
# and consteval machinery inside the stdexec headers; rationale pinned in
# foreign/CMakeLists.txt).
add_library(starter_net_backend STATIC
	net.backend.cc
)

# -Wno-maybe-uninitialized: additional middle-end false positive specific to
# this TU at -O3 — destroying the let_value operation state's value variant
# (stdexec __variant::__destroy over our Conn) trips the uninitialized
# analysis across the union; the asan-ubsan preset runs the same paths clean.
set_source_files_properties(net.backend.cc PROPERTIES
	CXX_SCAN_FOR_MODULES OFF
	COMPILE_OPTIONS "-Wno-null-dereference;-Wno-maybe-uninitialized"
)

set_target_properties(starter_net_backend PROPERTIES CXX_MODULE_STD OFF)

target_include_directories(starter_net_backend SYSTEM PRIVATE
	${stdexec_SOURCE_DIR}/include
)

target_compile_options(starter_net_backend PRIVATE
	-fno-sanitize=null,nonnull-attribute,returns-nonnull-attribute
)

target_link_libraries(starter_net_backend
	PRIVATE
		starter_language_profile
)

target_link_libraries(starter_module
	PRIVATE
		starter_net_backend
)
