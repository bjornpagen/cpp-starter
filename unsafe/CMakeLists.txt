# The :net partition is quarantine code but still a partition of the single
# starter module target defined in src/CMakeLists.txt.
target_sources(starter_module
	PUBLIC
		FILE_SET CXX_MODULES
		FILES
			${CMAKE_CURRENT_SOURCE_DIR}/net.cc
)

# PIN(gcc-gmf-stdexec-ice): the I/O half of the swap boundary stays a plain
# module-free TU, same arrangement and suppressions as
# foreign/CMakeLists.txt — see PINS.md
add_library(starter_net_backend STATIC
	net.backend.cc
)

# -Wno-maybe-uninitialized: middle-end false positive in this TU at -O3
# (stdexec __variant::__destroy over Conn); asan-ubsan runs the same paths clean.
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
