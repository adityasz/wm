add_wm_library(WindowManager
    WindowManager.cpp

    LINK_LIBS
        gch::small_vector
        PkgConfig::HyprDeps
        AppSwitcher
        Support
        WindowSwitcher
)
