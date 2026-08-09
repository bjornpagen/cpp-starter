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
	PRIVATE
		kernels.stdsimd.cc
		${STARTER_NEON_IMPL}
		${STARTER_SVE_IMPL}
)
