# Identical dialect-test executables. Not a general CMake framework.
function(starter_dialect_test stem)
	add_executable(starter_${stem}_tests)
	target_sources(starter_${stem}_tests PRIVATE ${stem}.test.cc)
	target_link_libraries(starter_${stem}_tests PRIVATE starter_module starter_language_profile)
	add_test(NAME starter.${stem} COMMAND starter_${stem}_tests)
endfunction()

starter_dialect_test(core)
starter_dialect_test(enums)
starter_dialect_test(exec)
starter_dialect_test(http)
starter_dialect_test(net)
starter_dialect_test(handle)

# PIN(gcc-template-for-wshadow): expansion statements re-declare the loop
# variable; source-scoped so it lands after the profile's -Wshadow — see PINS.md
set_source_files_properties(enums.test.cc PROPERTIES COMPILE_OPTIONS -Wno-shadow)

add_test(NAME starter.httpd.smoke
	COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/httpd.smoke.sh $<TARGET_FILE:starter_httpd>
)
set_tests_properties(starter.httpd.smoke PROPERTIES TIMEOUT 120)

add_executable(starter_conformance_tests)
target_sources(starter_conformance_tests PRIVATE conformance.test.cc)
target_link_libraries(starter_conformance_tests PRIVATE starter_language_profile)
set_source_files_properties(conformance.test.cc PROPERTIES COMPILE_OPTIONS -Wno-shadow)
add_test(NAME starter.conformance COMMAND starter_conformance_tests)

add_subdirectory(compile_fail)

if(STARTER_DARWIN)
	set_tests_properties(
		starter.core
		starter.enums
		starter.exec
		starter.http
		starter.net
		starter.handle
		starter.conformance
		starter.httpd.smoke
		PROPERTIES ENVIRONMENT "MallocProbGuard=1"
	)
endif()
