cmake_minimum_required(VERSION 4.0)

if(NOT CMAKE_GENERATOR STREQUAL "Ninja")
	message(FATAL_ERROR "Ninja is the only supported generator (got ${CMAKE_GENERATOR})")
endif()

# Toolchain validation is the documented build-system reason for this probe;
# source configuration never depends on incidental host state.
execute_process(
	COMMAND "${CMAKE_MAKE_PROGRAM}" --version
	RESULT_VARIABLE STARTER_NINJA_STATUS
	OUTPUT_VARIABLE STARTER_NINJA_VERSION
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT STARTER_NINJA_STATUS EQUAL 0 OR NOT STARTER_NINJA_VERSION MATCHES "^1\\.13\\.")
	message(FATAL_ERROR "Ninja 1.13.x is required (got ${STARTER_NINJA_VERSION})")
endif()

if(CMAKE_VERSION VERSION_LESS 4.2 OR CMAKE_VERSION VERSION_GREATER_EQUAL 4.3)
	message(FATAL_ERROR "CMake 4.2.x is required: the import-std experimental UUID below is specific to the 4.2 series (got ${CMAKE_VERSION})")
endif()

# PIN(cmake-import-std-uuid): the 4.2-series value — see PINS.md
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")

project(starter LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	message(FATAL_ERROR "the current unsafe network boundary supports Darwin only (got ${CMAKE_SYSTEM_NAME})")
endif()

option(STARTER_LINT_GRAPH
	"Configure the reflection-free Clang lint graph instead of the GCC production graph"
	OFF
)

# The configure step decides toolchain acceptability; TUs never negotiate features.
if(STARTER_LINT_GRAPH)
	if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
		message(FATAL_ERROR "the lint graph requires the pinned Clang toolchain (got ${CMAKE_CXX_COMPILER_ID})")
	endif()
	if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 22.1
			OR CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 22.2)
		message(FATAL_ERROR "the lint graph requires the pinned Clang 22.1.x (got ${CMAKE_CXX_COMPILER_VERSION})")
	endif()
	if(NOT CMAKE_CXX_CLANG_TIDY)
		message(FATAL_ERROR "the lint graph requires the pinned clang-tidy")
	endif()
	list(GET CMAKE_CXX_CLANG_TIDY 0 STARTER_CLANG_TIDY)
	execute_process(
		COMMAND "${STARTER_CLANG_TIDY}" --version
		RESULT_VARIABLE STARTER_CLANG_TIDY_STATUS
		OUTPUT_VARIABLE STARTER_CLANG_TIDY_VERSION
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	if(NOT STARTER_CLANG_TIDY_STATUS EQUAL 0 OR NOT STARTER_CLANG_TIDY_VERSION MATCHES "LLVM version 22\\.1\\.")
		message(FATAL_ERROR "the lint graph requires clang-tidy 22.1.x (got ${STARTER_CLANG_TIDY_VERSION})")
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

# Target half of the centralized language profile. Module-ABI flags above are
# global because CMake's synthesized std BMI cannot link this interface target.
add_library(starter_language_profile INTERFACE)

if(NOT STARTER_LINT_GRAPH)
	# PIN(cmake-ld-link-order): the contracts runtime must land after every
	# consumer's object files — see PINS.md
	target_link_libraries(starter_language_profile INTERFACE stdc++exp)

	target_compile_options(starter_language_profile INTERFACE
		"$<$<CONFIG:Release>:-ffunction-sections;-fdata-sections>"
	)
	target_link_options(starter_language_profile INTERFACE
		"$<$<CONFIG:Release>:LINKER:-dead_strip>"
	)
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

# PIN(gcc-gmf-stdexec-ice): stdexec backs the one foreign swap boundary
# until libstdc++ ships __cpp_lib_senders — see PINS.md. The header-only
# upstream dependency is pinned immutably and is also present in the lint graph
# so the Clang-readable foreign boundary is genuinely checked.
include(FetchContent)
FetchContent_Declare(stdexec
	GIT_REPOSITORY https://github.com/NVIDIA/stdexec.git
	GIT_TAG 19c454dca710cae39f18d11021ca0be4a8035c4a
	SOURCE_SUBDIR include
)
FetchContent_MakeAvailable(stdexec)

if(NOT STARTER_LINT_GRAPH)
	add_subdirectory(src)
endif()
add_subdirectory(unsafe)
add_subdirectory(foreign)
if(NOT STARTER_LINT_GRAPH)
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
