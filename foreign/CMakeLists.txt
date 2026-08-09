# The :exec partition is the dialect-clean execution surface; it is still a
# partition of the single starter module target defined in src/CMakeLists.txt
# (same arrangement as unsafe/'s :simd).
target_sources(starter_module
	PUBLIC
		FILE_SET CXX_MODULES
		FILES
			${CMAKE_CURRENT_SOURCE_DIR}/exec.cc
)

# The swap-boundary TU. It must stay module-free: GCC 16.1 ICEs when stdexec
# headers are included in any module unit (pinned in exec.backend.cc), so
# the boundary is an ordinary TU behind a narrow ABI. Scanning is disabled
# because this TU may never import (or be imported); the synthesized std
# module is off for the same reason.
add_library(starter_exec_backend STATIC
	exec.backend.cc
)

# -Wnull-dereference is a middle-end warning and does not honor the SYSTEM
# include suppression once stdexec's intrusive-queue splice is inlined under
# the sanitizer preset's -O2 (false positive inside
# stdexec/__detail/__intrusive_queue.hpp). Dropped for this quarantine TU
# only — source-level property so it lands after the language profile's
# -Wnull-dereference on the command line; the profile keeps it everywhere
# else.
set_source_files_properties(exec.backend.cc PROPERTIES
	CXX_SCAN_FOR_MODULES OFF
	COMPILE_OPTIONS -Wno-null-dereference
)

set_target_properties(starter_exec_backend PROPERTIES CXX_MODULE_STD OFF)

# The vendored headers are visible to this target only: nothing else in the
# build may see them (the whole point of the swap boundary). SYSTEM keeps
# the -Werror build clean — stdexec emits -Wmissing-field-initializers under
# -Wextra (probe-verified, from __continues_on.hpp).
target_include_directories(starter_exec_backend SYSTEM PRIVATE
	${stdexec_SOURCE_DIR}/include
)

# Probe-verified UBSan carve-out: the null, nonnull-attribute, and
# returns-nonnull-attribute sub-checks of -fsanitize=undefined break
# COMPILATION of stdexec's consteval completion-signature machinery
# (address comparison of __mtypeid_v objects is "not a constant
# expression"). A no-op when no sanitizer is enabled.
target_compile_options(starter_exec_backend PRIVATE
	-fno-sanitize=null,nonnull-attribute,returns-nonnull-attribute
)

target_link_libraries(starter_exec_backend
	PRIVATE
		starter_language_profile
)

target_link_libraries(starter_module
	PRIVATE
		starter_exec_backend
)
