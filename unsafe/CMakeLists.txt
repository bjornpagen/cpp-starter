if(NOT STARTER_LINT_GRAPH)
	target_sources(starter_module
		PUBLIC
			FILE_SET CXX_MODULES
			FILES
				${CMAKE_CURRENT_SOURCE_DIR}/net.cc
	)
endif()

# The Darwin syscall boundary is a plain TU so Clang can lint it without
# parsing the reflection-bearing starter module.
add_library(starter_net_backend STATIC
	net.backend.cc
)

set_source_files_properties(net.backend.cc PROPERTIES
	CXX_SCAN_FOR_MODULES OFF
)

set_target_properties(starter_net_backend PROPERTIES CXX_MODULE_STD OFF)

target_link_libraries(starter_net_backend
	PRIVATE
		starter_language_profile
)

if(NOT STARTER_LINT_GRAPH)
	target_link_libraries(starter_module
		PRIVATE
			starter_net_backend
	)
endif()
