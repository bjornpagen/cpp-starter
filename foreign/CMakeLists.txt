# The :exec partition is still a partition of the single starter module
# target defined in src/CMakeLists.txt (same arrangement as unsafe/'s :net).
target_sources(starter_module
	PUBLIC
		FILE_SET CXX_MODULES
		FILES
			${CMAKE_CURRENT_SOURCE_DIR}/exec.cc
)

# PIN(gcc-gmf-stdexec-ice): the combinator half of the swap boundary stays a
# plain module-free TU — see PINS.md. Scanning and the synthesized std module
# are off because this TU may never import or be imported.
add_library(starter_exec_backend STATIC
	exec.backend.cc
)

# -Wno-null-dereference: middle-end warning ignores the SYSTEM suppression
# once stdexec's intrusive-queue splice inlines at -O2; source-level property
# so it lands after the profile's flag, which stays on everywhere else.
set_source_files_properties(exec.backend.cc PROPERTIES
	CXX_SCAN_FOR_MODULES OFF
	COMPILE_OPTIONS -Wno-null-dereference
)

set_target_properties(starter_exec_backend PROPERTIES CXX_MODULE_STD OFF)

# Vendored headers visible to this target only; SYSTEM keeps -Werror clean
# (stdexec emits -Wmissing-field-initializers under -Wextra).
target_include_directories(starter_exec_backend SYSTEM PRIVATE
	${stdexec_SOURCE_DIR}/include
)

# UBSan carve-out (probe-verified): these sub-checks break COMPILATION of
# stdexec's consteval signature machinery; a no-op without sanitizers.
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
