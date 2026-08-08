cmake_minimum_required(VERSION 4.0)

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
# units never negotiate features. Production: GCC 16.x. Lint: Clang 22.x.
# Acquiring the toolchain is the developer's business (see README).
if(STARTER_LINT_GRAPH)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "the lint graph requires the pinned Clang toolchain (got ${CMAKE_CXX_COMPILER_ID})")
    endif()
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 22)
        message(FATAL_ERROR "the lint graph requires Clang 22.x (got ${CMAKE_CXX_COMPILER_VERSION})")
    endif()
else()
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR "production builds require the pinned GCC toolchain (got ${CMAKE_CXX_COMPILER_ID})")
    endif()
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16)
        message(FATAL_ERROR "production builds require the pinned GCC 16.x (got ${CMAKE_CXX_COMPILER_VERSION})")
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

include(CheckCompilerFlag)
check_compiler_flag(CXX -freflection STARTER_COMPILER_HAS_REFLECTION)
if(STARTER_COMPILER_HAS_REFLECTION AND NOT STARTER_LINT_GRAPH)
    string(APPEND CMAKE_CXX_FLAGS " -freflection")
endif()

# ----------------------------------------------------------------------------
# Language profile: the single build-system source of truth for the dialect.
# Every dialect target links this and nothing redefines these requirements.
# ----------------------------------------------------------------------------
add_library(starter_language_profile INTERFACE)

target_compile_features(starter_language_profile INTERFACE cxx_std_26)

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

add_subdirectory(src)

# meta/ holds GCC-only reflection modules; it is never part of the lint graph.
if(STARTER_COMPILER_HAS_REFLECTION AND NOT STARTER_LINT_GRAPH)
    add_subdirectory(meta)
endif()

enable_testing()
add_subdirectory(tests)

# The CMake-internal target that builds the standard-library module is not
# project code: clang-tidy must not run over it, and it must be compiled as
# strict c++26 (not gnu++26) or its BMI is rejected by our translation units.
if(TARGET __cmake_cxx26)
    set_property(TARGET __cmake_cxx26 PROPERTY CXX_EXTENSIONS OFF)
    if(STARTER_LINT_GRAPH)
        set_property(TARGET __cmake_cxx26 PROPERTY CXX_CLANG_TIDY "")
    endif()
endif()
