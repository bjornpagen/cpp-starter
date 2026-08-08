add_library(starter_enums)

target_sources(starter_enums
	PUBLIC
		FILE_SET CXX_MODULES
		FILES
			starter.enums.cppm
)

target_link_libraries(starter_enums
	PUBLIC
		starter_language_profile
)

add_library(starter_particles)

target_sources(starter_particles
	PUBLIC
		FILE_SET CXX_MODULES
		FILES
			starter.particles.cppm
)

# Same GCC 16.1 `template for` -Wshadow quirk as the reflection tests: the
# expansion statement in field_spans re-declares its loop variable per
# iteration, tripping -Wshadow on compiler-generated scoping.
set_source_files_properties(starter.particles.cppm PROPERTIES
	COMPILE_OPTIONS -Wno-shadow
)

target_link_libraries(starter_particles
	PUBLIC
		starter_language_profile
)
