export module wm.Support.Utils;

import std;
import hyprland.desktop;
import hyprland.globals;
import hyprland.helpers;
import hyprland.plugins;
import hyprland.protocols;
import hyprutils.memory;

using Hyprutils::Memory::CSharedPointer;
using Hyprutils::Memory::makeShared;

export namespace wm {

void focus_and_raise_window(
    const PHLWINDOW                          &window,
    const CSharedPointer<CWLSurfaceResource> &pSurface             = nullptr,
    bool                                      preserveFocusHistory = false
);

[[noreturn]] void init_die(void *handle, const auto &msg);

template <typename T, typename U>
CSharedPointer<T> add_config(void *handle, std::string_view key, const char *desc, U value)
{
	auto name       = std::format("plugin:wm:{}", key);
	auto config_val = makeShared<T>(name.c_str(), desc, value);
	if (HyprlandAPI::addConfigValueV2(handle, config_val))
		return config_val;
	init_die(handle, std::format("Failed to add config value for plugin:wm:{}", key));
}

template <typename T, typename U>
CSharedPointer<T> add_config(void *handle, std::string_view key, U value)
{ return add_config<T>(handle, key, "", value); }

} // namespace wm
