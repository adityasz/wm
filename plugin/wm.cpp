extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

import std;

import hyprland.config;
import hyprland.desktop;
import hyprland.devices;
import hyprland.event;
import hyprland.globals;
import hyprland.plugins;
import hyprland.render;
import hyprutils.signal;

import wm.Support;
import wm.WindowManager;

namespace Config::Lua {
namespace Bindings {
using namespace Internal;
}
using namespace Bindings;
}

using Config::Actions::ActionResult;
using Config::Values::CFloatValue;

using namespace wm;

static std::optional<WindowManager> window_manager;

static void register_listeners()
{
	static auto open_window   = Event::bus()->m_events.window.open.listen([](const PHLWINDOW &w) {
		window_manager->on_open_window(w);
	});
	static auto active_window = Event::bus()->m_events.window.active.listen(
	    [](const PHLWINDOW &w, Desktop::eFocusReason r) { window_manager->on_touch_window(w, r); }
	);
	static auto close_window = Event::bus()->m_events.window.close.listen([](const PHLWINDOW &w) {
		window_manager->on_close_window(w);
	});
	static auto key_press    = Event::bus()->m_events.input.keyboard.key.listen(
	    [](IKeyboard::SKeyEvent e, Event::SCallbackInfo &i) { window_manager->on_key_press(e, i); }
	);
	static auto render = Event::bus()->m_events.render.stage.listen([](eRenderStage s) {
		window_manager->render_app_switcher(s);
	});
	static auto config_reloaded =
	    Event::bus()->m_events.config.reloaded.listen([]() { window_manager->reset_config(); });
}

template <lua_CFunction F, ComptimeString Name>
static int lua_wm_dispatch_factory(lua_State *L)
{
	if (!lua_istable(L, 1)) {
		static constexpr auto err = Name + ": expected a table {{class=, cmd=}}";
		return Config::Lua::configError(L, err.str);
	}
	lua_getfield(L, 1, "class");
	lua_getfield(L, 1, "cmd");
	lua_pushcclosure(L, F, 2);
	return 1;
}

static bool register_dispatchers(void *handle)
{
	return HyprlandAPI::addLuaFunction(
	           handle,
	           "wm",
	           "focus_or_exec",
	           [](lua_State *L) {
		           return lua_wm_dispatch_factory<
		               [](lua_State *L) {
			               return Config::Lua::checkResult(
			                   L,
			                   window_manager->focus_or_exec(
			                       lua_tostring(L, lua_upvalueindex(1)),
			                       lua_tostring(L, lua_upvalueindex(2))
			                   )
			               );
		               },
		               "wm.focus_or_exec">(L);
	           }
	       )
	       && HyprlandAPI::addLuaFunction(handle, "wm", "move_or_exec", [](lua_State *L) {
		          return lua_wm_dispatch_factory<
		              [](lua_State *L) {
			              return Config::Lua::checkResult(
			                  L,
			                  window_manager->move_or_exec(
			                      lua_tostring(L, lua_upvalueindex(1)),
			                      lua_tostring(L, lua_upvalueindex(2))
			                  )
			              );
		              },
		              "wm.move_or_exec">(L);
	          });
}

namespace hooks {

static CFunctionHook *close_window_hook = nullptr;

using orig_close_window = ActionResult (*)(std::optional<PHLWINDOW>);

// ReSharper disable once CppPassValueParameterByConstReference
ActionResult close_window(std::optional<PHLWINDOW> w)
{
	// TODO: If I just close all windows of the currently highlighted app in the
	// app switcher in a for loop, close events are not emitted.
	if (window_manager->is_app_switcher_active()) {
		return Config::Actions::actionError(
		    "AppSwitcher active; ignoring closeWindow",
		    Config::Actions::eActionErrorLevel::INFO,
		    Config::Actions::eActionErrorCode::EXECUTION_FAILED
		);
	}

	return (reinterpret_cast<orig_close_window>(close_window_hook->m_original))(w);
}

} // namespace hooks

static bool register_hooks(void *handle)
{
	static const auto METHODS = HyprlandAPI::findFunctionsByName(handle, "closeWindow");
	hooks::close_window_hook  = HyprlandAPI::createFunctionHook(
	    handle, METHODS[0].address, reinterpret_cast<void *>(&hooks::close_window)
	);
	return hooks::close_window_hook->hook();
}

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
	auto compositor_hash = __hyprland_api_get_hash();
	auto client_hash     = __hyprland_api_get_client_hash();
	if (std::strcmp(compositor_hash, client_hash)) {
		init_die<"Version mismatch (headers ver = {} != {} = running ver)">(
		    handle, compositor_hash, client_hash
		);
	}

	if (Config::mgr()->type() != Config::CONFIG_LUA)
		init_die<"legacy config is not supported">(handle);

	// TODO: fix the AppInfoLoader and get rid of this mess
	auto icon_size_config = add_config<CFloatValue, "app_switcher:icons:size">(handle, 120);
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
