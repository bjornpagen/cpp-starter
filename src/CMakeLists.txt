# One importable named module: `starter` is the only export surface, its
# partitions are unimportable internals. Quarantine partitions join this
# target from unsafe/ and foreign/.
add_library(starter_module)

target_sources(starter_module
	PUBLIC
		FILE_SET CXX_MODULES
		BASE_DIRS ${PROJECT_SOURCE_DIR}
		FILES
			starter.cc
			core.cc
			enums.cc
			http.cc
)

# PIN(gcc-template-for-wshadow): the :enums partition instantiates expansion
# statements — see PINS.md
target_compile_options(starter_module PRIVATE -Wno-shadow)

target_link_libraries(starter_module
	PUBLIC
		starter_language_profile
)

add_executable(starter)

target_sources(starter
	PRIVATE
		main.cc
)

target_link_libraries(starter
	PRIVATE
		starter_module
		starter_language_profile
)
