add_wm_library(WindowSwitcher
    WindowSwitcher.cpp

    LINK_LIBS
        gch::small_vector
        PkgConfig::HyprDeps
        Support
)
