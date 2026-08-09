cmake_minimum_required(VERSION 4.0)

if(CMAKE_VERSION VERSION_LESS 4.2 OR CMAKE_VERSION VERSION_GREATER_EQUAL 4.3)
	message(FATAL_ERROR "CMake 4.2.x is required: the import-std experimental UUID below is specific to the 4.2 series (got ${CMAKE_VERSION})")
endif()

# `import std` is experimental in CMake. This UUID gates the feature and is
# specific to the CMake 4.2 series. On any CMake bump, re-read
# Help/dev/experimental.rst and update the value.
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")

project(starter LANGUAGES CXX)

option(STARTER_LINT_GRAPH
	"Configure the reflection-free Clang lint graph instead of the GCC production graph"
	OFF
)

# The configure step decides whether the toolchain is acceptable; translation
# units never negotiate features. Production: GCC 16.1.x. Lint: Clang 22.x.
# Acquiring the toolchain is the developer's business (see README).
if(STARTER_LINT_GRAPH)
	if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
		message(FATAL_ERROR "the lint graph requires the pinned Clang toolchain (got ${CMAKE_CXX_COMPILER_ID})")
	endif()
	if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 22
			OR CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 23)
		message(FATAL_ERROR "the lint graph requires the pinned Clang 22.x (got ${CMAKE_CXX_COMPILER_VERSION})")
	endif()
else()
	if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		message(FATAL_ERROR "production builds require the pinned GCC toolchain (got ${CMAKE_CXX_COMPILER_ID})")
	endif()
	if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16.1
			OR CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 16.2)
		message(FATAL_ERROR "production builds require the pinned GCC 16.1.x (got ${CMAKE_CXX_COMPILER_VERSION})")
	endif()
endif()

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Centralized `import std` policy: every dialect target consumes the
# standard-library named module. Do not set this per target.
set(CMAKE_CXX_MODULE_STD ON)

# Exceptions/RTTI/reflection are part of the module ABI: the std module BMI
# must be built with the same model as every importer (and `-freflection` is
# required for the std module to export std::meta at all). CMake's
# synthesized import-std targets only see global flags — so these are
# global, not part of the language-profile target.
string(APPEND CMAKE_CXX_FLAGS " -fno-exceptions -fno-rtti")

if(NOT STARTER_LINT_GRAPH)
	# Reflection is not negotiable: the version gate above guarantees GCC
	# 16.1.x, which implements it.
	string(APPEND CMAKE_CXX_FLAGS " -freflection")
	# Contracts (P2900) linkage moved to the language-profile target below:
	# GNU ld resolves left-to-right and CMAKE_EXE_LINKER_FLAGS precede the
	# object files, so the -l flag spelling satisfies nothing on Linux.
endif()

# ----------------------------------------------------------------------------
# Language profile: the single build-system source of truth for the dialect.
# Every dialect target links this and nothing redefines these requirements.
# ----------------------------------------------------------------------------
add_library(starter_language_profile INTERFACE)

target_compile_features(starter_language_profile INTERFACE cxx_std_26)

if(NOT STARTER_LINT_GRAPH)
	# Contracts (P2900): handle_contract_violation lives in libstdc++exp.
	# Linked as an interface library so it lands AFTER the object files on
	# the link line — GNU ld resolves left-to-right; macOS ld64 masked this.
	target_link_libraries(starter_language_profile INTERFACE stdc++exp)
endif()

# -fno-exceptions / -fno-rtti are global (see CMAKE_CXX_FLAGS above): they
# must also apply to the CMake-synthesized std module compilation.
target_compile_options(starter_language_profile INTERFACE
	-Werror
	-Wall
	-Wextra
	-Wpedantic
	-Wconversion
	-Wsign-conversion
	-Wshadow
	-Wold-style-cast
	-Wcast-align
	-Wnull-dereference
	-Wdouble-promotion
	"$<$<CXX_COMPILER_ID:GNU>:-Wuseless-cast;-Wduplicated-branches;-Wduplicated-cond;-Wlogical-op>"
)

# The starter module's partitions use C++26 reflection, which makes the
# whole module GCC-only as a unit, and every executable here imports it. The
# Clang lint graph covers only the Clang-parseable remainder — and today
# that remainder is empty: every TU either imports the starter module or is
# one of the two swap-boundary backend TUs, which consume the vendored
# stdexec headers that only this graph's targets may see. The lint preset
# still enforces its toolchain gate and configure-time sanity.
if(NOT STARTER_LINT_GRAPH)
	# stdexec: the pinned std::execution reference implementation backing the
	# sender/receiver dialect surface until libstdc++ ships __cpp_lib_senders
	# (tombstone in tests/conformance.test.cc). Pinned to the immutable
	# revision below per AGENTS.md §3.7 — never a moving branch/tag, so the
	# acquired source is reproducible byte-for-byte.
	#
	# Consumed from OUR MAINTAINED FORK, not upstream: every fork commit is
	# an individually-upstreamable fix with a pending PR (policy and
	# per-branch PR briefs: FORK.md in the fork), and the fork's success
	# condition is its own emptiness as upstream absorbs the delta.
	#
	#   fork      https://github.com/bjornpagen/stdexec  (branch: integration)
	#   upstream  https://github.com/NVIDIA/stdexec
	#   revision  0d8cfba62800bbe6604c3325ec29291e256614d8  (2026-08-09)
	#             = upstream fd60b20a
	#             + fix(__spin_loop_pause): ill-formed 'inline inline' on the
	#               ARM branch — hard GCC error on every arm build since
	#               upstream f0e8ae6f; fix also gives the ALWAYS_INLINE macro
	#               fallback branch a plain 'inline' so nvcc/NVHPC/EDG keep
	#               inline linkage (their expansion was empty)
	#               [submitted: github.com/NVIDIA/stdexec/pull/2167]
	#             + fix(sync_wait): terminate instead of misreporting errors
	#               as stopped when exceptions are unavailable
	#               [submitted: github.com/NVIDIA/stdexec/pull/2168]
	#
	# Header-only consumption: SOURCE_SUBDIR points at include/, which has no
	# CMakeLists.txt, so FetchContent_MakeAvailable populates the source tree
	# without ever running stdexec's own build (which would fetch
	# rapids-cmake). The include path is applied SYSTEM PRIVATE to the one
	# swap-boundary target in foreign/CMakeLists.txt; no other target may see
	# these headers.
	#
	include(FetchContent)
	FetchContent_Declare(stdexec
		GIT_REPOSITORY https://github.com/bjornpagen/stdexec.git
		GIT_TAG 0d8cfba62800bbe6604c3325ec29291e256614d8
		SOURCE_SUBDIR include
	)
	FetchContent_MakeAvailable(stdexec)

	add_subdirectory(src)
	add_subdirectory(unsafe)
	add_subdirectory(foreign)
	add_subdirectory(examples)
endif()

enable_testing()
if(NOT STARTER_LINT_GRAPH)
	add_subdirectory(tests)
endif()

# The CMake-internal target that builds the standard-library module is not
# project code: clang-tidy must not run over it, and it must be compiled as
# strict c++26 (not gnu++26) or its BMI is rejected by our translation units.
if(TARGET __cmake_cxx26)
	set_property(TARGET __cmake_cxx26 PROPERTY CXX_EXTENSIONS OFF)
	if(STARTER_LINT_GRAPH)
		set_property(TARGET __cmake_cxx26 PROPERTY CXX_CLANG_TIDY "")
	endif()
endif()
