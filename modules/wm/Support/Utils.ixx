export module wm.Support.Utils;

import std;
import hyprland.desktop;
import hyprland.globals;
import hyprland.helpers;
import hyprland.plugins;
import hyprland.protocols;
import hyprutils.memory;

import wm.Support.ComptimeString;

using Hyprutils::Memory::CSharedPointer;
using Hyprutils::Memory::makeShared;

export namespace wm {

void focus_and_raise_window(
    const PHLWINDOW                          &window,
    const CSharedPointer<CWLSurfaceResource> &pSurface             = nullptr,
    bool                                      preserveFocusHistory = false
);

template <ComptimeString FmtStr, typename... Args>
[[noreturn]] void init_die(void *handle, Args &&...fmt_args)
{
	static constexpr auto fmt_str = ComptimeString{"[wm] Error: Initialization failed: "} + FmtStr;
	auto                  err_msg = std::format(fmt_str.str, std::forward<Args>(fmt_args)...);
	HyprlandAPI::addNotification(handle, err_msg, CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
	throw std::runtime_error(err_msg);
}

template <typename T, ComptimeString Key, typename U>
CSharedPointer<T> add_config(void *handle, const char *desc, U value)
{
	static constexpr auto name       = ComptimeString{"plugin:wm:"} + Key;
	auto                  config_val = makeShared<T>(name.str, desc, value);
	if (HyprlandAPI::addConfigValueV2(handle, config_val))
		return config_val;
	static constexpr auto err = ComptimeString{"Failed to add config value for "} + name;
	init_die<err>(handle);
}

template <typename T, ComptimeString Key, typename U>
CSharedPointer<T> add_config(void *handle, U value)
{ return add_config<T, Key>(handle, "", value); }

} // namespace wm
