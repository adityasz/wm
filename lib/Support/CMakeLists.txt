wm_add_library(Support
	Utils.cpp
	StringPool.cpp
	MODULES
        ComptimeString.ixx
        Logging.ixx
        Utils.ixx
        StringPool.ixx
	LINK_LIBS PUBLIC Hyprland Hyprutils absl_modules
)
