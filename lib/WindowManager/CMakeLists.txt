add_wm_library(WindowManager
    PUBLIC
        WindowManager.ixx
    PRIVATE
        WindowManager.cpp
)

target_link_libraries(WindowManager
    PUBLIC
        llvm::SmallVector
        PkgConfig::HyprDeps
        AppSwitcher
        Support
        WindowSwitcher
)
