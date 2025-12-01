add_wm_library(AppSwitcher
    PUBLIC
        AppSwitcher.ixx
    PRIVATE
        AppSwitcherPassElement.cpp
        AppSwitcher.cpp
)

target_link_libraries(AppSwitcher
    PUBLIC
        PkgConfig::HyprDeps
        Support
)
