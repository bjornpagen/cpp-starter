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

# Senders conformance: the runtime spec of the vendored execution boundary
# (foreign/exec.cc + foreign/exec.backend.cc), driven through the exported
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

# Httpd smoke: boots the example server on an ephemeral port, hits it with
# concurrent curl requests (200s + body shape), and requires a clean SIGINT
# shutdown. The generous timeout is a backstop only; the run takes seconds.
add_test(NAME starter.httpd.smoke
	COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/httpd.smoke.sh $<TARGET_FILE:starter_httpd>
)

set_tests_properties(starter.httpd.smoke PROPERTIES TIMEOUT 120)

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
