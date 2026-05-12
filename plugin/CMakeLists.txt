add_library(wm SHARED)
target_sources(wm
    PRIVATE
        lib.cpp
    PRIVATE
        FILE_SET wm_module
        TYPE CXX_MODULES
        BASE_DIRS "${CMAKE_SOURCE_DIR}/modules"
        FILES "${CMAKE_SOURCE_DIR}/modules/wm.ixx"
)
target_link_libraries(wm
    PRIVATE
        Support
        AppSwitcher
        WindowSwitcher
        WindowManager
)
set_target_properties(wm PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
