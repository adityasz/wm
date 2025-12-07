#include <gch/small_vector.hpp>

#include "AppSwitcher/AppSwitcherPassElement.h"
#include "Support/Logging.h"
#include "Support/Utils.h"
#include "WindowManager/WindowManager.h"

using namespace std::chrono_literals;


WindowManager::WindowManager()
{
	quick_access_apps = [] {
		std::array<QuickAccessApp, NUM_QUICK_ACCESS_APPS> arr;
		arr.fill({"", ""});
		return arr;
	}();

	reload_config();

	app_id_to_stuff_map.reserve(10);
	for (auto &window : g_pCompositor->m_windowFocusHistory) {
		if (!std::ranges::contains(app_id_focus_history, window->m_class)) {
			app_id_focus_history.push_back(window->m_class);
			app_id_to_stuff_map[window->m_class].app_info =
			    app_info_loader.get_app_info(window->m_class, window->m_initialClass);
		}
		app_id_to_stuff_map[window->m_class].windows.push_back(window);
	}
}

void WindowManager::reload_config()
{
	if (app_switcher.is_active())
		app_switcher.abort();
	if (window_switcher.is_active())
		window_switcher.abort();

	static const std::array<const Hyprlang::STRING *, NUM_QUICK_ACCESS_APPS> app_config_strings =
	    [] {
		    std::array<const Hyprlang::STRING *, NUM_QUICK_ACCESS_APPS> temp{};
		    for (int i = 0; i < NUM_QUICK_ACCESS_APPS; i++)
			    temp[i] = get_config<char>(std::format("app_{}", i));
		    return temp;
	    }();

	auto invalid_config = [] { log(ERR, "invalid config"); };

	for (int i = 0; i < NUM_QUICK_ACCESS_APPS; i++) {
		const char *config_str = *app_config_strings[i];
		if (*config_str == '\0')
			continue;

		if (*config_str == ',')
			return invalid_config();
		int comma_idx = 1;
		while (config_str[comma_idx] != ',') {
			if (config_str[comma_idx] == '\0')
				return invalid_config();
			comma_idx++;
		}

		int app_id_start = 0;
		int app_id_end   = comma_idx - 1;
		while (std::isspace(static_cast<unsigned char>(config_str[app_id_start]))) {
			app_id_start++;
			if (app_id_start == comma_idx)
				return invalid_config();
		}
		while (std::isspace(static_cast<unsigned char>(config_str[app_id_end])))
			app_id_end--;
		quick_access_apps[i].app_id =
		    std::string(config_str + app_id_start, app_id_end - app_id_start + 1);

		const char *command = *app_config_strings[i] + comma_idx + 1;
		if (*command == '\0')
			return invalid_config();
		while (std::isspace(static_cast<unsigned char>(*command))) {
			command++;
			if (*command == '\0')
				return invalid_config();
		}
		quick_access_apps[i].command = command;

		log(INFO, "app {}: class={}, command={}", i, quick_access_apps[i].app_id, command);
	}
	app_switcher.reload_config();
	app_info_loader.reload_config();
}


void WindowManager::on_open_window(const PHLWINDOW &window)
{
	LOG_TRACE("{}", as_str(window));
	if (auto it = app_id_to_stuff_map.find(window->m_class); it == app_id_to_stuff_map.end()) {
		auto [it_new, _] = app_id_to_stuff_map.try_emplace(window->m_class);
		it_new->second.windows.push_back(window);
		it_new->second.app_info =
		    app_info_loader.get_app_info(window->m_class, window->m_initialClass);
		app_id_focus_history.push_back(window->m_class);
	} else {
		// Hyprland calls this twice sometimes
		if (auto &windows = it->second.windows; !std::ranges::contains(windows, window))
			windows.push_back(window);
	}
#ifdef JETBRAINS_SEARCH_EVERYWHERE_WORKAROUND

#endif
}

void WindowManager::on_touch_window(const PHLWINDOW &window)
{
	LOG_TRACE("{}", as_str(window));
	if (window_switcher.is_active()) {
		log(INFO, "window switcher is active, ignoring touch");
		return;
	}
	if (auto it = app_id_to_stuff_map.find(window->m_class); it == app_id_to_stuff_map.end()) {
		auto [it_new, _] = app_id_to_stuff_map.try_emplace(window->m_class);
		it_new->second.windows.push_back(window);
		it_new->second.app_info =
		    app_info_loader.get_app_info(window->m_class, window->m_initialClass);
		app_id_focus_history.insert(app_id_focus_history.begin(), window->m_class);
	} else {
		// app id can change at runtime
		auto &windows   = it->second.windows;
		auto  app_id_it = std::ranges::find(app_id_focus_history, window->m_class);
		std::rotate(app_id_focus_history.begin(), app_id_it, app_id_it + 1);
		if (auto window_it = std::ranges::find(windows, window); window_it != windows.end()) {
			std::rotate(windows.begin(), window_it, window_it + 1);
		} else {
			if (!std::ranges::contains(windows, window))
				windows.insert(windows.begin(), window);
		}
	}
}

void WindowManager::on_close_window(const PHLWINDOW &window)
{
	LOG_TRACE("{}", as_str(window));
	auto it = app_id_to_stuff_map.find(window->m_class);
	if (it == app_id_to_stuff_map.end())
		return;
	auto &windows = it->second.windows;
	window_switcher.on_close_window(window);
	gch::erase(windows, window);
	if (windows.empty()) {
		app_switcher.on_close_app(window->m_class);
		app_id_to_stuff_map.erase(window->m_class);
		std::erase(app_id_focus_history, window->m_class);
		app_info_loader.prune(app_id_focus_history);
	}
}

static SDispatchResult oob(int n)
{
	auto error = std::format("app_{} has an empty application ID or command", n);
	log(ERR, "{}", error);
	return {.success = false, .error = error};
}

SDispatchResult WindowManager::exec(int n)
{
	LOG_TRACE("{}", n);

	if (!strlen(quick_access_apps[n].command))
		return oob(n);

	log(INFO, "executing {}", quick_access_apps[n].command);
	CKeybindManager::spawn(quick_access_apps[n].command);
	return {};
}

SDispatchResult WindowManager::focus_or_exec(int n)
{
	LOG_TRACE("{}", n);

	if (quick_access_apps[n].app_id.empty())
		return oob(n);

	auto it = app_id_to_stuff_map.find(quick_access_apps[n].app_id);
	if (it == app_id_to_stuff_map.end())
		return exec(n);

	auto window_ref = it->second.windows.front();
	auto window     = window_ref.lock();
	if (!window) {
		gch::erase(it->second.windows, window_ref);
		auto error = std::format("{} became null", as_str(window_ref));
		log(INFO, "{}", error);
		return {.success = false, .error = error};
	}
	log(INFO, "focusing {}", as_str(window));
	if (auto last_monitor = g_pCompositor->m_lastMonitor.lock();
	    last_monitor
	    && last_monitor->m_activeSpecialWorkspace
	    && window->m_workspace != last_monitor->m_activeSpecialWorkspace) {
		last_monitor->setSpecialWorkspace(nullptr);
	}
	focus_and_raise_window(window);
	return {};
}

SDispatchResult WindowManager::move_or_exec(int n)
{
	LOG_TRACE("{}", n);

	if (quick_access_apps[n].app_id.empty())
		return oob(n);

	auto it = app_id_to_stuff_map.find(quick_access_apps[n].app_id);
	if (it == app_id_to_stuff_map.end())
		return exec(n);

	auto window_ref = it->second.windows.front();
	auto window     = window_ref.lock();
	if (!window) {
		gch::erase(it->second.windows, window_ref);
		auto error = std::format("{} became null", as_str(window_ref));
		log(INFO, "{}", error);
		return {.success = false, .error = error};
	}
	auto monitor = g_pCompositor->m_lastMonitor.lock();
	if (!monitor) {
		auto error = std::format("monitor {} became null", as_str(monitor));
		log(INFO, "{}", error);
		return {.success = false, .error = error};
	}
	if (auto active_workspace = monitor->m_activeWorkspace;
	    window->m_workspace != active_workspace) {
		log(INFO,
		    "moving {} from workspace '{}' to the active workspace '{}'",
		    window,
		    window->m_workspace->m_name,
		    active_workspace->m_name);
		g_pCompositor->moveWindowToWorkspaceSafe(window, active_workspace);
	} else {
		g_pCompositor->warpCursorTo(window->middle());
	}
	log(INFO, "focusing {}", as_str(window));
	focus_and_raise_window(window);
	return {};
}

bool WindowManager::on_key_press(uint32_t key, wl_keyboard_key_state state)
{
	static bool mod_held   = false;
	static bool shift_held = false;

	switch (key) {
	case SWITCHER_MOD:
		mod_held = state;
		if (!state) {
			// mod released
			// both of them cannot be active at the same time by design
			if (window_switcher.is_active())
				window_switcher.focus_selected();
			else if (app_switcher.is_active())
				app_switcher.focus_selected();
		}
		return false;
	case KEY_LEFTSHIFT:
	case KEY_RIGHTSHIFT: shift_held = state; return false;
	case KEY_TAB:
		if (mod_held) {
			if (state)
				handle_app_switching(shift_held);
			return true;
		}
		return false;
	case KEY_GRAVE:
		if (mod_held) {
			if (state)
				handle_window_switching(shift_held);
			return true;
		}
		return false;
	default: return false;
	}
}

void WindowManager::handle_app_switching(bool backwards)
{
	LOG_TRACE("backwards = {}", backwards);
	if (window_switcher.is_active())
		window_switcher.abort();
	if (!app_switcher.is_active())
		app_switcher.show(&app_id_focus_history, &app_id_to_stuff_map);
	app_switcher.move(backwards);
}

void WindowManager::handle_window_switching(bool backwards)
{
	LOG_TRACE("backwards = {}", backwards);
	if (app_switcher.is_active())
		app_switcher.focus_selected();
	if (!window_switcher.is_active()) {
		auto last_window = g_pCompositor->m_lastWindow;
		if (!last_window)
			return;
		auto it = app_id_to_stuff_map.find(last_window->m_class);
		if (it == app_id_to_stuff_map.end())
			return;
		window_switcher.seed(&it->second.windows);
	}
	window_switcher.move(backwards);
}

void WindowManager::render_app_switcher()
{
	if (app_switcher.is_active())
		g_pHyprRenderer->m_renderPass.add(makeUnique<AppSwitcherPassElement>(&app_switcher));
}

SDispatchResult WindowManager::dump_debug_info()
{
	log(LOG,
	    "Compositor windows: {}",
	    g_pCompositor->m_windows
	        | std::views::transform([](auto &window) { return as_str(window); }));
	log(LOG,
	    "Compositor focus history: {}",
	    g_pCompositor->m_windowFocusHistory
	        | std::views::transform([](auto &window) { return as_str(window); }));
	log(LOG, "WM:");
	app_switcher.show(&app_id_focus_history, &app_id_to_stuff_map); // to load icon textures
	// this is terrible since it causes the focused app to be raised but
	// ::hide() is a private method (and rightfully so) and this is a debugging
	// method so this should not be a big deal
	app_switcher.focus_selected();
	for (const auto &app_id : app_id_focus_history) {
		auto &[windows, app_info] = app_id_to_stuff_map.at(app_id);
		std::string app_name      = "<future>";
		if (auto app_render_data = std::get_if<AppRenderData>(&app_info))
			app_name = app_render_data->app_name;
		log(LOG, "{:<4}{}: {}", "", app_id, app_name);
		for (const auto &window : windows)
			log(LOG, "{:<8}{}", "", as_str(window));
	}
	return {};
}

std::span<PHLWINDOWREF> WindowManager::get_app_switcher_current()
{
	return app_switcher.is_active()
	           ? app_id_to_stuff_map.at(app_switcher.get_current_selection()).windows
	           : std::span<PHLWINDOWREF>{};
}
