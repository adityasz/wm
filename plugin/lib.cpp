#include "Globals.h"

import std;
import wm;

using namespace wm;

#define WLR_USE_UNSTABLE

#define REGISTER_CALLBACK(eventName, lambdaBody)                                                  \
	static auto P##eventName = HyprlandAPI::registerCallbackDynamic(                              \
	    PHANDLE,                                                                                  \
	    #eventName,                                                                               \
	    [](void *, [[maybe_unused]] SCallbackInfo &info, [[maybe_unused]] const std::any &data)   \
	        lambdaBody                                                                            \
	)

#define ADD_DISPATCHER(dispatcher, lambdaBody)                                                    \
	HyprlandAPI::addDispatcherV2(                                                                 \
	    PHANDLE,                                                                                  \
	    "wm:" #dispatcher,                                                                        \
	    []([[maybe_unused]] const std::string &arg)->SDispatchResult lambdaBody                   \
	)

static std::optional<WindowManager> window_manager; // initialized after the handle is set

void register_callbacks()
{
	// Hyprland calls these with nullptr sometimes
	REGISTER_CALLBACK(openWindow, {
		if (auto window = std::any_cast<PHLWINDOW>(data))
			window_manager->on_open_window(window);
	});
	REGISTER_CALLBACK(activeWindow, {
		if (auto window = std::any_cast<PHLWINDOW>(data))
			window_manager->on_touch_window(window);
	});
	REGISTER_CALLBACK(closeWindow, {
		if (auto window = std::any_cast<PHLWINDOW>(data))
			window_manager->on_close_window(window);
	});
	REGISTER_CALLBACK(keyPress, {
		auto skeyevent = std::any_cast<IKeyboard::SKeyEvent>(
		    std::any_cast<std::unordered_map<std::string, std::any>>(data)["event"]
		);
		info.cancelled = window_manager->on_key_press(skeyevent.keycode, skeyevent.state);
	});
	REGISTER_CALLBACK(render, {
		if (std::any_cast<eRenderStage>(data) == eRenderStage::RENDER_POST_WINDOWS) {
			window_manager->render_app_switcher();
		}
	});
	REGISTER_CALLBACK(configReloaded, { window_manager->reload_config(); });
}

void register_dispatchers()
{
	bool success  = true;
	success      &= ADD_DISPATCHER(exec, { return window_manager->exec(std::stoi(arg)); });
	success &=
	    ADD_DISPATCHER(focusorexec, { return window_manager->focus_or_exec(std::stoi(arg)); });
	success &=
	    ADD_DISPATCHER(moveorexec, { return window_manager->move_or_exec(std::stoi(arg)); });
	success &= ADD_DISPATCHER(debuginfo, { return window_manager->dump_debug_info(); });
	if (!success) {
		auto error = "[wm] Failed to register dispatchers";
		HyprlandAPI::addNotification(PHANDLE, error, CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
		throw std::runtime_error(error);
	}
}

namespace hook {
inline CFunctionHook *close_active_window_hook = nullptr;

typedef SDispatchResult (*orig_close_active_window)(void *, std::string);

// ReSharper disable once CppPassValueParameterByConstReference
SDispatchResult close_active_window(void *thisptr, std::string data)
{
	if (auto windows = window_manager->get_app_switcher_current(); !windows.empty()) {
		// TODO: If I just close them all in a for loop, for some reason, close
		// events are not emitted. So let's not do anything stupid until I find
		// time to read the relevant parts of Hyprland's codebase.
		return {.success = false, .error = "not implemented"};
	}
	return (reinterpret_cast<orig_close_active_window>(close_active_window_hook->m_original))(
	    thisptr, data
	);
}
} // namespace hook

void register_hooks()
{
	static const auto METHODS      = HyprlandAPI::findFunctionsByName(PHANDLE, "closeActive");
	hook::close_active_window_hook = HyprlandAPI::createFunctionHook(
	    PHANDLE, METHODS[0].address, reinterpret_cast<void *>(&hook::close_active_window)
	);
	hook::close_active_window_hook->hook();
}

APICALL EXPORT std::string PLUGIN_API_VERSION() { return HYPRLAND_API_VERSION; }

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle)
{
	PHANDLE = handle;

	auto compositor_hash = __hyprland_api_get_hash();
	auto client_hash     = __hyprland_api_get_client_hash();
	if (std::strcmp(compositor_hash, client_hash)) {
		auto error = std::format(
		    "[wm] Failure in initialization: Version mismatch (headers ver = {} != {} = "
		    "running ver)",
		    compositor_hash,
		    client_hash
		);
		HyprlandAPI::addNotification(PHANDLE, error, CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
		throw std::runtime_error(error);
	}

	using Hyprlang::INT;
	using Hyprlang::STRING;
	using Hyprlang::FLOAT;

	for (int i = 0; i < NUM_QUICK_ACCESS_APPS; i++)
		add_config(std::format("app_{}", i), STRING{""});
	add_config("app_switcher:container:background_color", INT{0x11'ff'ff'ff});
	add_config("app_switcher:container:border_color", INT{0x11'80'80'80});
	add_config("app_switcher:container:padding", INT{20});
	add_config("app_switcher:container:radius", INT{35});
	add_config("app_switcher:container:border_width", INT{1});
	add_config("app_switcher:selection:background_color", INT{0x11'00'00'00});
	add_config("app_switcher:selection:padding", INT{10});
	add_config("app_switcher:selection:radius", INT{30});
	add_config("app_switcher:label:font_family", STRING{"Inter"});
	add_config("app_switcher:label:font_color", INT{0xff'ff'ff});
	add_config("app_switcher:label:font_size", INT{0});
	add_config("app_switcher:label:separation", INT{0});
	add_config("app_switcher:icons:size", INT{120});
	add_config("app_switcher:icons:separation", INT{40});
	add_config("app_switcher:icons:theme", STRING{""});

	HyprlandAPI::reloadConfig();

	window_manager.emplace();

	register_callbacks();

	register_dispatchers();

	register_hooks();

	return {
	    "wm",
	    "window manager, app switcher, window switcher, and a couple of dispatchers",
	    "Aditya Singh",
	    "0.1"
	};
}

APICALL EXPORT void PLUGIN_EXIT() {}
