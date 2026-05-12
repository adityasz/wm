add_wm_library(WindowSwitcher
    WindowSwitcher.cpp

    LINK_LIBS
        llvm::SmallVector
        PkgConfig::HyprDeps
        Support
)
