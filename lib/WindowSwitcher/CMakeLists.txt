add_wm_library(WindowSwitcher
    PUBLIC
        WindowSwitcher.ixx
    PRIVATE
        WindowSwitcher.cpp
)

target_link_libraries(WindowSwitcher
    PUBLIC
        llvm::SmallVector
        PkgConfig::HyprDeps
        Support
)
