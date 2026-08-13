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

if(STARTER_DARWIN)
	set(STARTER_BOMBARDIER_ASSET bombardier-darwin-arm64)
	set(STARTER_BOMBARDIER_SHA256 5d893be6e3e90d660c798b13f7616bdcee6954c04be4f34eb5b45a667e50e18b)
elseif(STARTER_LINUX)
	set(STARTER_BOMBARDIER_ASSET bombardier-linux-arm64)
	set(STARTER_BOMBARDIER_SHA256 258025ea4dc183bbe3f4ccc7772de1faf2c73700e9f81aa5b84e0c1799d7323c)
endif()
set(STARTER_BOMBARDIER "${CMAKE_BINARY_DIR}/tools/bombardier")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/tools")
if(NOT EXISTS "${STARTER_BOMBARDIER}")
	file(DOWNLOAD
		"https://github.com/codesenberg/bombardier/releases/download/v2.0.2/${STARTER_BOMBARDIER_ASSET}"
		"${STARTER_BOMBARDIER}"
		EXPECTED_HASH SHA256=${STARTER_BOMBARDIER_SHA256}
		TLS_VERIFY ON
	)
endif()
file(SHA256 "${STARTER_BOMBARDIER}" STARTER_BOMBARDIER_GOT)
if(NOT STARTER_BOMBARDIER_GOT STREQUAL STARTER_BOMBARDIER_SHA256)
	message(FATAL_ERROR "bombardier hash mismatch")
endif()
file(CHMOD "${STARTER_BOMBARDIER}" PERMISSIONS
	OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

add_library(starter_testkit)
target_sources(starter_testkit
	PUBLIC
		FILE_SET CXX_MODULES
		BASE_DIRS ${PROJECT_SOURCE_DIR}
		FILES
			${PROJECT_SOURCE_DIR}/unsafe/testkit.cc
)
target_link_libraries(starter_testkit PRIVATE starter_child starter_language_profile)

add_executable(starter_httpd_tests)
target_sources(starter_httpd_tests PRIVATE httpd.test.cc)
target_link_libraries(starter_httpd_tests PRIVATE starter_testkit starter_module starter_language_profile)

file(GENERATE
	OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/httpd.paths.txt"
	CONTENT "$<TARGET_FILE:starter_httpd>\n${STARTER_BOMBARDIER}\n"
)

add_test(NAME starter.httpd COMMAND starter_httpd_tests)
set_tests_properties(starter.httpd PROPERTIES
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	TIMEOUT 180
	RUN_SERIAL TRUE
)

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
		starter.httpd
		PROPERTIES ENVIRONMENT "MallocProbGuard=1"
	)
endif()
