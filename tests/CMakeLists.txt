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

# PIN(gcc-template-for-wshadow): this TU instantiates expansion statements;
# source-level so it lands after the profile's -Wshadow — see PINS.md
set_source_files_properties(enums.test.cc PROPERTIES
	COMPILE_OPTIONS -Wno-shadow
)

add_test(NAME starter.enums COMMAND starter_enums_tests)

# The runtime spec of the execution boundary, driven through the exported
# module surface only.
add_executable(starter_exec_tests)

target_sources(starter_exec_tests
	PRIVATE
		exec.test.cc
)

target_link_libraries(starter_exec_tests
	PRIVATE
		starter_module
		starter_language_profile
)

add_test(NAME starter.exec COMMAND starter_exec_tests)

# HTTP parser/writer unit spec (:http partition, pure dialect).
add_executable(starter_http_tests)

target_sources(starter_http_tests
	PRIVATE
		http.test.cc
)

target_link_libraries(starter_http_tests
	PRIVATE
		starter_module
		starter_language_profile
)

add_test(NAME starter.http COMMAND starter_http_tests)

# Boots the example on an ephemeral port, hits it with concurrent curls, and
# requires a clean SIGINT shutdown; the generous timeout is a backstop only.
add_test(NAME starter.httpd.smoke
	COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/httpd.smoke.sh $<TARGET_FILE:starter_httpd>
)

set_tests_properties(starter.httpd.smoke PROPERTIES TIMEOUT 120)

# The executable spec of the production toolchain (feature-test macros,
# reflection expansion statements, contracts macro).
add_executable(starter_conformance_tests)

target_sources(starter_conformance_tests
	PRIVATE
		conformance.test.cc
)

target_link_libraries(starter_conformance_tests
	PRIVATE
		starter_language_profile
)

# PIN(gcc-template-for-wshadow): same expansion-statement quirk as
# enums.test.cc — see PINS.md
set_source_files_properties(conformance.test.cc PROPERTIES
	COMPILE_OPTIONS -Wno-shadow
)

add_test(NAME starter.conformance COMMAND starter_conformance_tests)

add_subdirectory(compile_fail)
