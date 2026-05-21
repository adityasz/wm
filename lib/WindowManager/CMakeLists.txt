wm_add_library(WindowManager
    WindowManager.cpp
    MODULES WindowManager.ixx
	LINK_LIBS PUBLIC Hyprland AppSwitcher WindowSwitcher
)
