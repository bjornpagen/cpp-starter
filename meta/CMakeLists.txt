add_library(starter_meta)

target_sources(starter_meta
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            starter.meta.cppm
)

target_link_libraries(starter_meta
    PUBLIC
        starter_language_profile
)
