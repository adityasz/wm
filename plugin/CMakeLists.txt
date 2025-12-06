add_library(wm SHARED)
target_sources(wm PRIVATE lib.cpp)
target_link_libraries(wm
    PRIVATE
        PkgConfig::HyprDeps
        Support
        AppSwitcher
        WindowSwitcher
        WindowManager
)
set_target_properties(wm PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
