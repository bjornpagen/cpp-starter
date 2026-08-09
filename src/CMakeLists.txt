# One importable named module per component: `starter` is the only export
# surface and its internals are partitions, so outsiders physically cannot
# import them. Every partition file plus the primary interface lives in this
# one FILE_SET; CMake scans the partition dependencies itself. The :simd
# partition and its intrinsic kernel TUs are quarantine code and are added to
# this target from unsafe/CMakeLists.txt.
add_library(starter_module)

target_sources(starter_module
	PUBLIC
		FILE_SET CXX_MODULES
		BASE_DIRS ${PROJECT_SOURCE_DIR}
		FILES
			starter.cc
			core.cc
			enums.cc
			particles.cc
			integrate.cc
			http.cc
)

# GCC 16.1 quirk: expansion statements (`template for`) re-declare the loop
# variable in a nested scope per iteration, so any expansion over a range
# with more than one element trips -Wshadow on compiler-generated scoping.
# The :particles partition instantiates expansion statements, so the module
# target drops -Wshadow; it stays on everywhere else (test TUs that
# instantiate expansion statements carry their own source-level -Wno-shadow).
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
