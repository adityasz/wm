add_wm_library(Support
    PUBLIC
        Support.ixx
        Logging.ixx
        Utils.ixx
    PRIVATE
        Utils.cpp
)

target_link_libraries(Support
    PUBLIC
        PkgConfig::HyprDeps
)
