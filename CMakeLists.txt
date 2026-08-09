cmake_minimum_required(VERSION 4.0)

if(CMAKE_VERSION VERSION_LESS 4.2 OR CMAKE_VERSION VERSION_GREATER_EQUAL 4.3)
	message(FATAL_ERROR "CMake 4.2.x is required: the import-std experimental UUID below is specific to the 4.2 series (got ${CMAKE_VERSION})")
endif()

# PIN(cmake-import-std-uuid): the 4.2-series value — see PINS.md
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")

project(starter LANGUAGES CXX)

option(STARTER_LINT_GRAPH
	"Configure the reflection-free Clang lint graph instead of the GCC production graph"
	OFF
)

# The configure step decides toolchain acceptability; TUs never negotiate features.
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

# Centralized `import std` policy: never set per target.
set(CMAKE_CXX_MODULE_STD ON)

# Module-ABI flags must be global: CMake's synthesized import-std targets see
# only CMAKE_CXX_FLAGS, and the std module BMI must match every importer.
string(APPEND CMAKE_CXX_FLAGS " -fno-exceptions -fno-rtti")

if(NOT STARTER_LINT_GRAPH)
	# -freflection is required for the std module to export std::meta at all.
	string(APPEND CMAKE_CXX_FLAGS " -freflection")
endif()

# Language profile: the single source of truth for the dialect; every dialect
# target links it and nothing redefines these requirements.
add_library(starter_language_profile INTERFACE)

target_compile_features(starter_language_profile INTERFACE cxx_std_26)

if(NOT STARTER_LINT_GRAPH)
	# PIN(cmake-ld-link-order): the contracts runtime must land after every
	# consumer's object files — see PINS.md
	target_link_libraries(starter_language_profile INTERFACE stdc++exp)

	target_compile_options(starter_language_profile INTERFACE
		"$<$<CONFIG:Release>:-ffunction-sections;-fdata-sections>"
	)
	if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
		target_link_options(starter_language_profile INTERFACE
			"$<$<CONFIG:Release>:LINKER:-dead_strip>"
		)
	else()
		target_link_options(starter_language_profile INTERFACE
			"$<$<CONFIG:Release>:LINKER:--gc-sections>"
		)
	endif()
endif()

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

# Reflection makes the starter module GCC-only as a unit; the Clang lint
# graph covers only the Clang-parseable remainder, which today is empty.
if(NOT STARTER_LINT_GRAPH)
	# PIN(gcc-gmf-stdexec-ice): stdexec backs the swap boundary until
	# libstdc++ ships __cpp_lib_senders — see PINS.md. Consumed from our
	# maintained fork (policy: FORK.md in the fork), pinned to an immutable
	# revision:
	#   fork      https://github.com/bjornpagen/stdexec  (branch: integration)
	#   upstream  https://github.com/NVIDIA/stdexec
	#   revision  0d8cfba62800bbe6604c3325ec29291e256614d8  (2026-08-09)
	#             = upstream fd60b20a
	#             + fix(__spin_loop_pause)  [NVIDIA/stdexec PR 2167]
	#             + fix(sync_wait)          [NVIDIA/stdexec PR 2168]
	# Header-only: SOURCE_SUBDIR include/ has no CMakeLists.txt, so stdexec's
	# own build never runs; the path is SYSTEM PRIVATE to the two
	# swap-boundary targets only.
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

# The CMake-internal std-module target is not project code: no clang-tidy,
# and strict c++26 (a gnu++26 BMI is rejected by our translation units).
if(TARGET __cmake_cxx26)
	set_property(TARGET __cmake_cxx26 PROPERTY CXX_EXTENSIONS OFF)
	if(STARTER_LINT_GRAPH)
		set_property(TARGET __cmake_cxx26 PROPERTY CXX_CLANG_TIDY "")
	endif()
endif()
