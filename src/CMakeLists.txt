add_library(starter_core)

target_sources(starter_core
	PUBLIC
		FILE_SET CXX_MODULES
		FILES
			starter.core.cppm
	PRIVATE
		starter.core.cpp
)

target_link_libraries(starter_core
	PUBLIC
		starter_language_profile
)

# starter.integrate imports the reflection-derived starter.particles, so its
# parse graph is GCC-only and it stays out of the Clang lint graph
# (AGENTS.md §3.6).
if(NOT STARTER_LINT_GRAPH)
	add_library(starter_integrate)

	target_sources(starter_integrate
		PUBLIC
			FILE_SET CXX_MODULES
			FILES
				starter.integrate.cppm
		PRIVATE
			starter.integrate.cpp
	)

	target_link_libraries(starter_integrate
		PUBLIC
			starter_particles
			starter_language_profile
	)
endif()

add_executable(starter)

target_sources(starter
	PRIVATE
		main.cpp
)

target_link_libraries(starter
	PRIVATE
		starter_core
		starter_language_profile
)
