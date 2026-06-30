wm_add_library(WindowManager
    WindowManager.cpp
    MODULES WindowManager.ixx
	LINK_LIBS PUBLIC Hyprland AppSwitcher WindowSwitcher
)
set_source_files_properties(WindowManager.cpp PROPERTIES COMPILE_DEFINITIONS SWITCHER_MOD=${SWITCHER_MOD})
