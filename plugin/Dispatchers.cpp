module;

#include <lua.hpp>

module dispatchers;

import std;

import hyprland.config;
import hyprland.desktop;
import hyprland.plugins;

import globals;

import wm.Support;

using namespace wm;

namespace Config::Lua {
namespace Bindings {
using namespace Internal;
}
using namespace Bindings;
}

static int lua_wm_fullscreen_factory(lua_State *L)
{
	if (!lua_isstring(L, 1)) [[unlikely]] {
		return Config::Lua::configError(
		    L, "wm.fullscreen: expected a string \"maximized\" or \"fullscreen\""
		);
	}

	const std::string_view mode_str = lua_tostring(L, 1);
	eFullscreenMode        mode;
	if (mode_str == "maximized") {
		mode = eFullscreenMode::FSMODE_MAXIMIZED;
	} else if (mode_str == "fullscreen") {
		mode = eFullscreenMode::FSMODE_FULLSCREEN;
	} else [[unlikely]] {
		return Config::Lua::configError(
		    L, "wm.fullscreen: mode must be \"maximized\" or \"fullscreen\""
		);
	}

	lua_pushinteger(L, static_cast<lua_Integer>(mode));
	lua_pushcclosure(
	    L,
	    [](lua_State *L) {
		    return Config::Lua::checkResult(
		        L,
		        window_manager->fullscreen(
		            static_cast<eFullscreenMode>(lua_tointeger(L, lua_upvalueindex(1)))
		        )
		    );
	    },
	    1
	);
	return 1;
}

template <lua_CFunction F, ComptimeString Name>
static int lua_wm_dispatch_factory(lua_State *L)
{
	if (!lua_istable(L, 1)) [[unlikely]] {
		static constexpr auto err = Name + ": expected a table {{class=, cmd=}}";
		return Config::Lua::configError(L, err.str);
	}
	lua_getfield(L, 1, "class");
	lua_getfield(L, 1, "cmd");
	lua_pushcclosure(L, F, 2);
	return 1;
}

bool register_dispatchers(void *handle)
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
	       && HyprlandAPI::addLuaFunction(
	           handle,
	           "wm",
	           "move_or_exec",
	           [](lua_State *L) {
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
	           }
	       )
	       && HyprlandAPI::addLuaFunction(handle, "wm", "fullscreen", lua_wm_fullscreen_factory);
}
