if(NOT STARTER_LINT_GRAPH)
	target_sources(starter_module
		PUBLIC
			FILE_SET CXX_MODULES
			FILES
				${CMAKE_CURRENT_SOURCE_DIR}/net.cc
	)
endif()

if(STARTER_DARWIN)
	set(STARTER_WAIT_BACKEND net.darwin.cc)
elseif(STARTER_LINUX)
	set(STARTER_WAIT_BACKEND net.linux.cc)
endif()

add_library(starter_net_backend STATIC
	net.loop.cc
	${STARTER_WAIT_BACKEND}
)

set_source_files_properties(net.loop.cc ${STARTER_WAIT_BACKEND} PROPERTIES
	CXX_SCAN_FOR_MODULES OFF
)
set_target_properties(starter_net_backend PROPERTIES CXX_MODULE_STD OFF)

target_include_directories(starter_net_backend PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(STARTER_LINUX)
	set_source_files_properties(net.linux.cc PROPERTIES COMPILE_DEFINITIONS _GNU_SOURCE)
endif()

target_link_libraries(starter_net_backend PRIVATE starter_language_profile)

if(NOT STARTER_LINT_GRAPH)
	target_link_libraries(starter_module PRIVATE starter_net_backend)
endif()
