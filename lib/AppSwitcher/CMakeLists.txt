wm_add_library(AppSwitcher
	AppSwitcher.cpp
	AppSwitcherPassElement.cpp

	MODULES
        AppSwitcher.ixx

	LINK_LIBS
		PRIVATE turbojpeg
		PUBLIC Hyprland Hyprutils AppInfoLoader Support llvm_modules
)
