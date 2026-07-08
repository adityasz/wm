if(NOT HASH_CHECK)
    set_source_files_properties(wm.cpp PROPERTIES COMPILE_DEFINITIONS UNSAFE_SKIP_HASH_CHECK)
endif()

set(hooks_defs)
if(BETTER_FLOATING_BEHAVIOR)
    list(APPEND hooks_defs BETTER_FLOATING_BEHAVIOR)
endif()
if(BETTER_DRAG_BEHAVIOR)
    list(APPEND hooks_defs BETTER_DRAG_BEHAVIOR)
endif()
set_source_files_properties(Hooks.cpp PROPERTIES COMPILE_DEFINITIONS "${hooks_defs}")

add_library(wm SHARED wm.cpp Dispatchers.cpp Hooks.cpp Listeners.cpp)
target_sources(wm
    PUBLIC
        FILE_SET wm_modules
        TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES Dispatchers.ixx Globals.ixx Hooks.ixx Listeners.ixx
)
set_target_properties(wm PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_link_libraries(wm
    PRIVATE
        WindowManager
        AppInfoLoader
        AppSwitcher
        WindowSwitcher
        Support
        llvm_modules
        Hyprland
        nkutils-icons
)

install(TARGETS wm RUNTIME DESTINATION "${CMAKE_INSTALL_LIBDIR}")
