add_library(starter_simd)

# One explicitly supported implementation per kernel (AGENTS.md §3.1); no
# source file branches on architecture. The NEON kernel exists only for
# aarch64 targets; elsewhere the stub reports the kernel unavailable.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
	set(STARTER_NEON_IMPL starter.simd.kernels.neon.cpp)
else()
	set(STARTER_NEON_IMPL starter.simd.kernels.neon.stub.cpp)
endif()

# SVE compiles on any aarch64 toolchain but executes only on SVE hardware,
# which no supported development host has (Apple silicon lacks SVE; CI is
# x86_64). Opt in explicitly when the target machine executes SVE.
option(STARTER_SVE "Build the SVE kernel (target must execute SVE)" OFF)

if(STARTER_SVE)
	if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
		message(FATAL_ERROR "STARTER_SVE requires an aarch64 target (got ${CMAKE_SYSTEM_PROCESSOR})")
	endif()
	set(STARTER_SVE_IMPL starter.simd.kernels.sve.cpp)
	set_source_files_properties(starter.simd.kernels.sve.cpp PROPERTIES
		COMPILE_OPTIONS -march=armv8.2-a+sve
	)
else()
	set(STARTER_SVE_IMPL starter.simd.kernels.sve.stub.cpp)
endif()

target_sources(starter_simd
	PUBLIC
		FILE_SET CXX_MODULES
		FILES
			starter.simd.cppm
	PRIVATE
		starter.simd.cpp
		starter.simd.kernels.stdsimd.cpp
		${STARTER_NEON_IMPL}
		${STARTER_SVE_IMPL}
)

target_link_libraries(starter_simd
	PUBLIC
		starter_particles
		starter_language_profile
)
