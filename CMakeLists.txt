cmake_minimum_required(VERSION 4.0)

if(NOT CMAKE_GENERATOR STREQUAL "Ninja")
	message(FATAL_ERROR "Ninja is the only supported generator (got ${CMAKE_GENERATOR})")
endif()

if(NOT CMAKE_MAKE_PROGRAM)
	find_program(CMAKE_MAKE_PROGRAM ninja REQUIRED)
endif()

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

if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64" AND NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
	message(FATAL_ERROR "the current machine boundary is arm64 only (got ${CMAKE_SYSTEM_PROCESSOR})")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	set(STARTER_DARWIN TRUE)
	set(STARTER_LINUX FALSE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(STARTER_DARWIN FALSE)
	set(STARTER_LINUX TRUE)
else()
	message(FATAL_ERROR "the wait backend supports Darwin and Linux only (got ${CMAKE_SYSTEM_NAME})")
endif()

option(STARTER_LINT_GRAPH
	"Configure the reflection-free Clang lint graph instead of the GCC production graph"
	OFF
)
option(STARTER_RUNTIME_SANITIZERS
	"Use runtime ASan+UBSan instead of trap-mode UBSan (the asan-ubsan preset)"
	OFF
)

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
	if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16.1)
		message(FATAL_ERROR "production builds require GCC 16.1 or later (got ${CMAKE_CXX_COMPILER_VERSION})")
	endif()
endif()

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_MODULE_STD ON)

# Module-ABI flags must be global: CMake's synthesized import-std targets see
# only CMAKE_CXX_FLAGS, and the std module BMI must match every importer.
string(APPEND CMAKE_CXX_FLAGS " -fno-exceptions -fno-rtti -D_GLIBCXX_ASSERTIONS")

if(NOT STARTER_LINT_GRAPH)
	# IPO probe must precede -freflection: the probe TU is not compiled as C++26.
	include(CheckIPOSupported)
	check_ipo_supported(LANGUAGES CXX)
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
	string(APPEND CMAKE_CXX_FLAGS " -freflection")
endif()

add_library(starter_language_profile INTERFACE)

if(NOT STARTER_LINT_GRAPH)
	# PIN(cmake-ld-link-order): contracts runtime after every consumer — see PINS.md
	target_link_libraries(starter_language_profile INTERFACE stdc++exp)
	if(NOT STARTER_RUNTIME_SANITIZERS)
		target_compile_options(starter_language_profile INTERFACE
			-fsanitize=undefined
			-fsanitize-trap=all
			"$<$<CONFIG:Release>:-ffunction-sections;-fdata-sections>"
		)
		target_link_options(starter_language_profile INTERFACE
			-fsanitize=undefined
			-fsanitize-trap=all
			"$<$<AND:$<CONFIG:Release>,$<BOOL:${STARTER_DARWIN}>>:LINKER:-dead_strip>"
			"$<$<AND:$<CONFIG:Release>,$<BOOL:${STARTER_LINUX}>>:LINKER:--gc-sections>"
		)
	endif()
endif()

target_compile_options(starter_language_profile INTERFACE
	-ftrivial-auto-var-init=zero
	-fstack-protector-strong
	-fzero-call-used-regs=used-gpr
	"$<$<CXX_COMPILER_ID:GNU>:-fstack-clash-protection;-fno-delete-null-pointer-checks;-fno-strict-aliasing;-fstrict-flex-arrays=3;-fzero-init-padding-bits=all>"
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
	-Wformat
	-Wformat=2
	-Werror=format-security
	-Wimplicit-fallthrough
	"$<$<CXX_COMPILER_ID:GNU>:-Wuseless-cast;-Wduplicated-branches;-Wduplicated-cond;-Wlogical-op;-Wtrampolines;-Wbidi-chars=any>"
)

# PIN(darwin-inert-mitigations): FORTIFY and PAC/BTI are Linux-only — see PINS.md
if(STARTER_LINUX)
	target_compile_options(starter_language_profile INTERFACE
		-fPIE
		"$<$<CXX_COMPILER_ID:GNU>:-mbranch-protection=standard>"
	)
	target_link_options(starter_language_profile INTERFACE
		-pie
		LINKER:-z,relro
		LINKER:-z,now
		LINKER:-z,noexecstack
		LINKER:-z,nodlopen
		LINKER:--as-needed
		LINKER:--no-copy-dt-needed-entries
	)
	if(NOT STARTER_RUNTIME_SANITIZERS)
		target_compile_options(starter_language_profile INTERFACE
			-U_FORTIFY_SOURCE
			-D_FORTIFY_SOURCE=3
		)
	endif()
endif()

# PIN(gcc-gmf-stdexec-ice): stdexec backs the one foreign swap boundary
# until libstdc++ ships __cpp_lib_senders — see PINS.md
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

if(TARGET __cmake_cxx26)
	set_property(TARGET __cmake_cxx26 PROPERTY CXX_EXTENSIONS OFF)
	if(STARTER_LINT_GRAPH)
		set_property(TARGET __cmake_cxx26 PROPERTY CXX_CLANG_TIDY "")
	endif()
endif()
