# The :net partition is quarantine code (sockets, kevent, errno live behind
# it), so it lives here — but it is still a partition of the single starter
# module target defined in src/CMakeLists.txt.
target_sources(starter_module
	PUBLIC
		FILE_SET CXX_MODULES
		FILES
			${CMAKE_CURRENT_SOURCE_DIR}/net.cc
)

# The networking swap-boundary TU: the kqueue io-context whose senders are
# spelled in stdexec (the I/O half of the boundary; the combinator half is
# foreign/exec.backend.cc). Same quarantine arrangement as that target and
# for the same pinned reasons: it must stay module-free (GCC 16.1 ICEs when
# stdexec headers are included in any module unit), the vendored headers are
# SYSTEM and visible to this target only, and the probe-verified UBSan
# carve-out plus the -Wnull-dereference drop apply (middle-end diagnostics
# and consteval machinery inside the stdexec headers; rationale pinned in
# foreign/CMakeLists.txt).
add_library(starter_net_backend STATIC
	net.backend.cc
)

# -Wno-maybe-uninitialized: additional middle-end false positive specific to
# this TU at -O3 — destroying the let_value operation state's value variant
# (stdexec __variant::__destroy over our Conn) trips the uninitialized
# analysis across the union; the asan-ubsan preset runs the same paths clean.
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
