# Expected-failure compiler harness: each case is an EXCLUDE_FROM_ALL target
# whose build MUST fail with the pinned diagnostic. The ctest entry invokes
# the build for just that target; PASS_REGULAR_EXPRESSION means the test
# passes only when the diagnostic appears — a case that starts compiling, or
# whose diagnostic degrades, fails the suite.
function(starter_compile_fail_case case diagnostic)
	add_executable(cf_${case} EXCLUDE_FROM_ALL ${case}.cc)
	target_link_libraries(cf_${case} PRIVATE starter_module starter_language_profile)
	add_test(NAME compile_fail.${case}
		COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target cf_${case}
	)
	set_tests_properties(compile_fail.${case} PROPERTIES
		PASS_REGULAR_EXPRESSION "${diagnostic}"
		RUN_SERIAL TRUE
	)
endfunction()

starter_compile_fail_case(discarded_expected
	"ignoring return value of .*greeting.*nodiscard"
)
