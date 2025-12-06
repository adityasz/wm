add_library(WM SHARED)
target_sources(WM PRIVATE lib.cpp)
target_link_libraries(WM
    PRIVATE
        PkgConfig::HyprDeps
        Support
        AppSwitcher
        WindowSwitcher
        WindowManager
)
