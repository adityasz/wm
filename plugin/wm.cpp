import std;

import hyprland.config;
import hyprland.plugins;
import hyprland.render;

import wm.Support;
import wm.WindowManager;

import dispatchers;
import globals;
import hooks;
import listeners;

using namespace wm;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type-c-linkage"
// Return type is as per Hyprland documentation.
extern "C" [[gnu::visibility("default")]] std::string pluginAPIVersion() { return "0.1"; }

// Return type is equivalent to the return type mentioned in Hyprland
// documentation (which is of the form `using identifier = (anonymous struct)`
// instead of `struct identifier {}` for some reason).
extern "C" [[gnu::visibility("default")]]
std::tuple<std::string, std::string, std::string, std::string> pluginInit(void *handle)
#pragma GCC diagnostic pop
{
#ifndef UNSAFE_SKIP_HASH_CHECK
	auto compositor_hash = __hyprland_api_get_hash();
	auto client_hash     = __hyprland_api_get_client_hash();
	if (std::strcmp(compositor_hash, client_hash)) {
		init_die<"Version mismatch (headers ver = {} != {} = running ver)">(
		    handle, compositor_hash, client_hash
		);
	}
#endif

	if (Config::mgr()->type() != Config::CONFIG_LUA)
		init_die<"legacy config is not supported">(handle);

	// TODO: fix the AppInfoLoader and get rid of this mess
	auto icon_size_config =
	    add_config<Config::Values::CFloatValue, "app_switcher:icons:size">(handle, 120);
	WindowManagerConfig config(handle, icon_size_config);

	HyprlandAPI::reloadConfig();

	window_manager.emplace(config);

	register_listeners();
	if (!register_dispatchers(handle))
		init_die<"failed to register dispatchers">(handle);
	if (!register_hooks(handle))
		init_die<"failed to register hooks">(handle);

	return {"wm", "a plugin that does a whole bunch of stuff", "Aditya", "0.1"};
}

extern "C" [[gnu::visibility("default")]] void pluginExit()
{ g_pHyprRenderer->m_renderPass.removeAllOfType(AppSwitcherPassElement::pass_name); }
