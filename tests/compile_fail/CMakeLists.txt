# Expected-failure case: starting to compile, or losing its pinned diagnostic,
# fails the suite.
add_executable(cf_discarded_expected EXCLUDE_FROM_ALL
	discarded_expected.cc
)

target_link_libraries(cf_discarded_expected
	PRIVATE
		starter_module
		starter_language_profile
)

add_test(NAME compile_fail.discarded_expected
	COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target cf_discarded_expected
)

set_tests_properties(compile_fail.discarded_expected PROPERTIES
	PASS_REGULAR_EXPRESSION "ignoring return value of .*greeting.*nodiscard"
	RUN_SERIAL TRUE
)
