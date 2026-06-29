module;

#include <lua.hpp>

module dispatchers;

import std;

import hyprland.config;
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
