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

# Reflection tests import starter.meta, so their parse graph is GCC-only and
# they are excluded from the Clang lint graph (AGENTS.md §3.6).
if(TARGET starter_meta)
    add_executable(starter_meta_tests)

    target_sources(starter_meta_tests
        PRIVATE
            starter.meta.test.cpp
    )

    target_link_libraries(starter_meta_tests
        PRIVATE
            starter_core
            starter_meta
            starter_language_profile
    )

    add_test(NAME starter.meta COMMAND starter_meta_tests)
endif()
