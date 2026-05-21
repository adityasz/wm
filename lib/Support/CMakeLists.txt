wm_add_library(Support
	Utils.cpp
	MODULES
        Support.ixx
        Logging.ixx
        Utils.ixx
	LINK_LIBS PUBLIC Hyprland Hyprutils
)
