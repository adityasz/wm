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
using Config::Values::CColorValue;
using Config::Values::CFloatValue;
using Config::Values::CIntValue;
using Config::Values::CStringValue;
using Hyprutils::Signal::CHyprSignalListener;

using namespace wm;

static std::optional<WindowManager> window_manager;

struct Listeners {
	CHyprSignalListener open_window;
	CHyprSignalListener active_window;
	CHyprSignalListener close_window;
	CHyprSignalListener key_press;
	CHyprSignalListener render;
	CHyprSignalListener config_reloaded;
};

Listeners register_listeners(WindowManager &wm)
{
	return Listeners{
	    .open_window   = Event::bus()->m_events.window.open.listen([&wm](const PHLWINDOW &w) {
		    wm.on_open_window(w);
	    }),
	    .active_window = Event::bus()->m_events.window.active.listen(
	        [&wm](const PHLWINDOW &w, Desktop::eFocusReason r) { wm.on_touch_window(w, r); }
	    ),
	    .close_window = Event::bus()->m_events.window.close.listen([&wm](const PHLWINDOW &w) {
		    wm.on_close_window(w);
	    }),
	    .key_press    = Event::bus()->m_events.input.keyboard.key.listen(
	        [&wm](IKeyboard::SKeyEvent e, Event::SCallbackInfo &i) { wm.on_key_press(e, i); }
	    ),
	    .render = Event::bus()->m_events.render.stage.listen([&wm](eRenderStage s) {
		    wm.render_app_switcher(s);
	    }),
	    .config_reloaded =
	        Event::bus()->m_events.config.reloaded.listen([&wm]() { wm.reset_config(); })
	};
}

static int dsp_focus_or_exec(lua_State *L)
{
	return Config::Lua::checkResult(
	    L,
	    window_manager->focus_or_exec(
	        lua_tostring(L, lua_upvalueindex(1)), lua_tostring(L, lua_upvalueindex(2))
	    )
	);
}

static int dsp_move_or_exec(lua_State *L)
{
	return Config::Lua::checkResult(
	    L,
	    window_manager->move_or_exec(
	        lua_tostring(L, lua_upvalueindex(1)), lua_tostring(L, lua_upvalueindex(2))
	    )
	);
}

template <lua_CFunction F>
static int luaDispatchFactory(lua_State *L, const char *name)
{
	// Ideally, the string should be allocated at compile time. However,
	// comptime string construction will take more lines of code than the code
	// duplication it is supposed to prevent.
	if (!lua_istable(L, 1))
		return Config::Lua::configError(L, "{}: expected a table {{class=, cmd=}}", name);
	lua_getfield(L, 1, "class");
	lua_getfield(L, 1, "cmd");
	lua_pushcclosure(L, F, 2);
	return 1;
}

bool register_functions(void *handle)
{
	return HyprlandAPI::addLuaFunction(
	           handle,
	           "wm",
	           "focus_or_exec",
	           [](lua_State *L) {
		           return luaDispatchFactory<dsp_focus_or_exec>(L, "wm.focus_or_exec");
	           }
	       )
	       && HyprlandAPI::addLuaFunction(handle, "wm", "move_or_exec", [](lua_State *L) {
		          return luaDispatchFactory<dsp_move_or_exec>(L, "wm.move_or_exec");
	          });
}

namespace hooks {

static CFunctionHook *close_window_hook = nullptr;

using orig_close_window = ActionResult (*)(std::optional<PHLWINDOW>);

// ReSharper disable once CppPassValueParameterByConstReference
ActionResult close_window(std::optional<PHLWINDOW> w, WindowManager &window_manager)
{
	if (auto windows = window_manager.get_app_switcher_current(); !windows.empty()) {
		// TODO: If I just close them all in a for loop, close events are not emitted.
		return {};
	}
	return (reinterpret_cast<orig_close_window>(close_window_hook->m_original))(w);
}

} // namespace hooks

void register_hooks(void *handle)
{
	static const auto METHODS = HyprlandAPI::findFunctionsByName(handle, "closeWindow");
	hooks::close_window_hook  = HyprlandAPI::createFunctionHook(
	    handle, METHODS[0].address, reinterpret_cast<void *>(&hooks::close_window)
	);
	hooks::close_window_hook->hook();
}

// Return type is equivalent to the return type mentioned in Hyprland
// documentation (which is of the form `using identifier = (anonymous struct)`
// instead of `struct identifier {}` for some reason).
extern "C" __attribute__((visibility("default")))
std::tuple<std::string, std::string, std::string, std::string>
pluginInit(void *handle)
{
	auto compositor_hash = __hyprland_api_get_hash();
	auto client_hash     = __hyprland_api_get_client_hash();
	if (std::strcmp(compositor_hash, client_hash)) {
		init_die(
		    handle,
		    std::format(
		        "Version mismatch (headers ver = {} != {} = running ver)",
		        compositor_hash,
		        client_hash
		    )
		);
	}

	if (Config::mgr()->type() != Config::CONFIG_LUA)
		init_die(handle, "legacy config is not supported");

	auto icon_size_config = add_config<CFloatValue>(handle, "app_switcher:icons:size", 120);
	WindowManagerConfig config{
	    .app_switcher =
	        AppSwitcherConfig{
	            .container_background_color = add_config<CColorValue>(
	                handle, "app_switcher:container:background_color", 0x11'ff'ff'ff
	            ),
	            .container_border_color = add_config<CColorValue>(
	                handle, "app_switcher:container:border_color", 0x11'80'80'80
	            ),
	            .container_border_width =
	                add_config<CFloatValue>(handle, "app_switcher:container:border_width", 1),
	            .container_padding =
	                add_config<CFloatValue>(handle, "app_switcher:container:padding", 20),
	            .container_radius =
	                add_config<CIntValue>(handle, "app_switcher:container:radius", 35),
	            .selection_background_color = add_config<CColorValue>(
	                handle, "app_switcher:selection:background_color", 0x11'00'00'00
	            ),
	            .selection_padding =
	                add_config<CFloatValue>(handle, "app_switcher:selection:padding", 10),
	            .selection_radius =
	                add_config<CIntValue>(handle, "app_switcher:selection:radius", 30),
	            .font_family =
	                add_config<CStringValue>(handle, "app_switcher:label:font_family", "Inter"),
	            .font_color =
	                add_config<CColorValue>(handle, "app_switcher:label:font_color", 0xff'ff'ff),
	            .font_size = add_config<CIntValue>(handle, "app_switcher:label:font_size", 0),
	            .label_sep = add_config<CFloatValue>(handle, "app_switcher:label:separation", 0),
	            .icon_size = icon_size_config,
	            .icon_sep  = add_config<CFloatValue>(handle, "app_switcher:icons:separation", 40)
	        },
	    .app_info_loader = AppInfoLoaderConfig{
	        .icon_size = icon_size_config,
	        .theme     = add_config<CStringValue>(handle, "app_switcher:icons:theme", "")
	    }
	};

	HyprlandAPI::reloadConfig();

	window_manager.emplace(config);

	if (!register_functions(handle))
		init_die(handle, "failed to register functions");

	static auto listeners = register_listeners(*window_manager);
	register_hooks(handle);

	return {"wm", "a plugin that does a whole bunch of stuff", "Aditya", "0.1"};
}

extern "C" __attribute__((visibility("default"))) void pluginExit()
{ g_pHyprRenderer->m_renderPass.removeAllOfType(AppSwitcherPassElement::pass_name); }

// Return type is as per Hyprland documentation.
extern "C" __attribute__((visibility("default"))) std::string pluginAPIVersion() { return "0.1"; }
