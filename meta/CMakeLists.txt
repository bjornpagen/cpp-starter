add_library(starter_enums)

target_sources(starter_enums
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            starter.enums.cppm
)

target_link_libraries(starter_enums
    PUBLIC
        starter_language_profile
)
