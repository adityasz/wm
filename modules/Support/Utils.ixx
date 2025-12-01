module;

#include "Globals.h"
#include "Hyprland.h"

export module wm.Support.Utils;

export namespace wm {
template <typename T>
void add_config(std::string_view key, T value)
{
	HyprlandAPI::addConfigValue(PHANDLE, std::format("plugin:wm:{}", key), value);
}

template <typename T>
const T *const *get_config(std::string_view key)
{
	return reinterpret_cast<const T *const *>(
	    HyprlandAPI::getConfigValue(PHANDLE, std::format("plugin:wm:{}", key))->getDataStaticPtr()
	);
}

void focus_and_raise_window(
    PHLWINDOW window, SP<CWLSurfaceResource> pSurface = nullptr, bool preserveFocusHistory = false
);
} // namespace wm
