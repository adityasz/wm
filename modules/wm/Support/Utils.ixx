export module wm.Support.Utils;

import std;
import hyprland.desktop;
import hyprland.helpers;
import hyprland.protocols;
import hyprland.plugins;
import hyprutils.memory;

using Hyprutils::Memory::CSharedPointer;
using Hyprutils::Memory::makeShared;

export namespace wm {

void focus_and_raise_window(
    const PHLWINDOW              &window,
    const CSharedPointer<CWLSurfaceResource> &pSurface             = nullptr,
    bool                          preserveFocusHistory = false
);

template <typename T, typename U>
CSharedPointer<T> add_config(void *handle, std::string_view key, const char *desc, U value)
{
	auto config_val = makeShared<T>(std::format("plugin:wm:{}", key).c_str(), desc, value);
	HyprlandAPI::addConfigValueV2(handle, config_val);
	return config_val;
}

template <typename T, typename U>
CSharedPointer<T> add_config(void *handle, std::string_view key, U value)
{ return add_config<T>(handle, key, "", value); }

} // namespace wm
