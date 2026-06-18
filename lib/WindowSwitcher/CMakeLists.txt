wm_add_library(WindowSwitcher
    WindowSwitcher.cpp
    MODULES WindowSwitcher.ixx
	LINK_LIBS
		PUBLIC llvm_modules Hyprland Support
)
