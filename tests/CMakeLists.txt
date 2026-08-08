add_executable(starter_core_tests)

target_sources(starter_core_tests
	PRIVATE
		starter.core.test.cpp
)

target_link_libraries(starter_core_tests
	PRIVATE
		starter_core
		starter_language_profile
)

add_test(NAME starter.core COMMAND starter_core_tests)

# Reflection tests import starter.enums, so their parse graph is GCC-only and
# they are excluded from the Clang lint graph (AGENTS.md §3.6).
if(TARGET starter_enums)
	add_executable(starter_enums_tests)

	target_sources(starter_enums_tests
		PRIVATE
			starter.enums.test.cpp
	)

	target_link_libraries(starter_enums_tests
		PRIVATE
			starter_core
			starter_enums
			starter_language_profile
	)

	# GCC 16.1 quirk: expansion statements (`template for`) re-declare the
	# loop variable in a nested scope per iteration, so any expansion over a
	# range with more than one element trips -Wshadow on compiler-generated
	# scoping. Disabled only for the TUs that instantiate expansion
	# statements; -Wshadow stays on everywhere else. (Source-level property so
	# it lands after the language profile's -Wshadow on the command line.)
	set_source_files_properties(starter.enums.test.cpp PROPERTIES
		COMPILE_OPTIONS -Wno-shadow
	)

	add_test(NAME starter.enums COMMAND starter_enums_tests)
endif()

# The kernel-agreement test imports meta/ reflection modules and unsafe/
# intrinsic kernels, so it is likewise GCC-only (AGENTS.md §3.6).
if(TARGET starter_simd)
	add_executable(starter_particles_tests)

	target_sources(starter_particles_tests
		PRIVATE
			starter.particles.test.cpp
	)

	target_link_libraries(starter_particles_tests
		PRIVATE
			starter_particles
			starter_integrate
			starter_simd
			starter_language_profile
	)

	add_test(NAME starter.particles COMMAND starter_particles_tests)
endif()

# The conformance test is the executable spec of the production toolchain
# (GCC feature-test macros, reflection expansion statements, contracts macro).
# The Clang lint frontend is not the object under test, so it stays out of
# the lint graph entirely.
if(NOT STARTER_LINT_GRAPH)
	add_executable(starter_conformance_tests)

	target_sources(starter_conformance_tests
		PRIVATE
			starter.conformance.test.cpp
	)

	target_link_libraries(starter_conformance_tests
		PRIVATE
			starter_language_profile
	)

	# Same GCC 16.1 `template for` -Wshadow quirk as starter.enums.test.cpp.
	set_source_files_properties(starter.conformance.test.cpp PROPERTIES
		COMPILE_OPTIONS -Wno-shadow
	)

	add_test(NAME starter.conformance COMMAND starter_conformance_tests)
endif()
