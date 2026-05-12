add_wm_library(WindowManager
    WindowManager.cpp

    LINK_LIBS
        llvm::SmallVector
        PkgConfig::HyprDeps
        AppSwitcher
        Support
        WindowSwitcher
)
