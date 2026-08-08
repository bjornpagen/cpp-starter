add_library(starter_core)

target_sources(starter_core
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            starter.core.cppm
    PRIVATE
        starter.core.cpp
)

target_link_libraries(starter_core
    PUBLIC
        starter_language_profile
)

add_executable(starter)

target_sources(starter
    PRIVATE
        main.cpp
)

target_link_libraries(starter
    PRIVATE
        starter_core
        starter_language_profile
)
