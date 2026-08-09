add_executable(starter_core_tests)

target_sources(starter_core_tests
	PRIVATE
		core.test.cc
)

target_link_libraries(starter_core_tests
	PRIVATE
		starter_module
		starter_language_profile
)

add_test(NAME starter.core COMMAND starter_core_tests)

add_executable(starter_enums_tests)

target_sources(starter_enums_tests
	PRIVATE
		enums.test.cc
)

target_link_libraries(starter_enums_tests
	PRIVATE
		starter_module
		starter_language_profile
)

# GCC 16.1 quirk: expansion statements (`template for`) re-declare the
# loop variable in a nested scope per iteration, so any expansion over a
# range with more than one element trips -Wshadow on compiler-generated
# scoping. Disabled only for the TUs that instantiate expansion
# statements; -Wshadow stays on everywhere else. (Source-level property so
# it lands after the language profile's -Wshadow on the command line.)
set_source_files_properties(enums.test.cc PROPERTIES
	COMPILE_OPTIONS -Wno-shadow
)

add_test(NAME starter.enums COMMAND starter_enums_tests)

add_executable(starter_particles_tests)

target_sources(starter_particles_tests
	PRIVATE
		particles.test.cc
)

target_link_libraries(starter_particles_tests
	PRIVATE
		starter_module
		starter_language_profile
)

add_test(NAME starter.particles COMMAND starter_particles_tests)

# The conformance test is the executable spec of the production toolchain
# (GCC feature-test macros, reflection expansion statements, contracts macro).
add_executable(starter_conformance_tests)

target_sources(starter_conformance_tests
	PRIVATE
		conformance.test.cc
)

target_link_libraries(starter_conformance_tests
	PRIVATE
		starter_language_profile
)

# Same GCC 16.1 `template for` -Wshadow quirk as enums.test.cc.
set_source_files_properties(conformance.test.cc PROPERTIES
	COMPILE_OPTIONS -Wno-shadow
)

add_test(NAME starter.conformance COMMAND starter_conformance_tests)
