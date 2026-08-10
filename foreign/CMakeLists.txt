if(NOT STARTER_LINT_GRAPH)
	target_sources(starter_module
		PUBLIC
			FILE_SET CXX_MODULES
			FILES
				${CMAKE_CURRENT_SOURCE_DIR}/exec.cc
	)
endif()

add_library(starter_stdexec_vendor INTERFACE)

target_include_directories(starter_stdexec_vendor SYSTEM INTERFACE
	${stdexec_SOURCE_DIR}/include
)

# PIN(gcc-gmf-stdexec-ice): the vendor boundary stays a plain module-free TU
# — see PINS.md. Scanning and the synthesized std module are off because this
# TU may never import or be imported.
add_library(starter_exec_backend STATIC
	exec.backend.cc
)

set_source_files_properties(exec.backend.cc PROPERTIES
	CXX_SCAN_FOR_MODULES OFF
)

set_target_properties(starter_exec_backend PROPERTIES CXX_MODULE_STD OFF)

if(STARTER_LINT_GRAPH)
	# PIN(stdexec-tooling-carveouts): stdexec operation states intentionally
	# contain self-references, and moving a generic sender is semantically required.
	set_property(TARGET starter_exec_backend APPEND PROPERTY CXX_CLANG_TIDY
		"--checks=-clang-analyzer-core.StackAddressEscape,-performance-move-const-arg"
	)
endif()

target_link_libraries(starter_exec_backend
	PRIVATE
		starter_language_profile
		starter_stdexec_vendor
)

if(NOT STARTER_LINT_GRAPH)
	target_link_libraries(starter_module
		PRIVATE
			starter_exec_backend
	)
endif()
