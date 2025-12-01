add_wm_library(WindowSwitcher
    PUBLIC
        WindowSwitcher.ixx
    PRIVATE
        WindowSwitcher.cpp
)

target_link_libraries(WindowSwitcher
    PUBLIC
        PkgConfig::HyprDeps
        Support
)

add_comptime_value(WindowManager SWITCHER_MOD "The modifier key for switching apps and windows" KEY_LEFTMETA)
add_comptime_value(WindowManager NUM_QUICK_ACCESS_APPS "The number of quick access apps" 10)
