module;

#include <linux/input-event-codes.h>

module wm.WindowManager;

import std;
import hyprland.config;
import hyprland.globals;
import hyprland.render;
import hyprutils.memory;
import llvm.Support;
import wm.Support;

using Config::Actions::ActionResult;
using Config::Actions::actionError;
using Config::Actions::eActionErrorCode;
using Config::Actions::eActionErrorLevel;
using Hyprutils::Memory::makeUnique;

using namespace wm;
using namespace std::chrono_literals;

WindowManager::WindowManager(const WindowManagerConfig &config) :
    app_switcher(config.app_switcher),
    app_info_loader(config.app_info_loader),
    config(config)
{
	app_id_to_stuff_map.reserve(20);
	for (auto &window : Desktop::History::windowTracker()->fullHistory() | std::views::reverse) {
		const char *app_id = app_id_pool.get(window->m_initialClass);
		if (!std::ranges::contains(app_id_focus_history, app_id)) {
			app_id_focus_history.push_back(app_id);
			app_id_to_stuff_map[app_id].app_info =
			    app_info_loader.get_app_info(app_id, window->m_initialClass);
		}
		app_id_to_stuff_map[app_id].windows.push_back(window);
	}
}

void WindowManager::reset_config()
{
	if (app_switcher.is_active())
		app_switcher.deactivate();
	if (window_switcher.is_active())
		window_switcher.deactivate();

	app_switcher.reset_config(config.app_switcher);
	app_info_loader.reset_config(config.app_info_loader);
}

void WindowManager::on_open_window(const PHLWINDOW &window)
{
	if (!window)
		return;
	const char *app_id = app_id_pool.get(window->m_initialClass);
	if (auto it = app_id_to_stuff_map.find(app_id); it == app_id_to_stuff_map.end()) {
		auto [it_new, _] = app_id_to_stuff_map.try_emplace(app_id);
		it_new->second.windows.push_back(window);
		it_new->second.app_info = app_info_loader.get_app_info(app_id, window->m_initialClass);
		app_id_focus_history.push_back(app_id);
	} else {
		// Hyprland calls this twice sometimes
		if (auto &windows = it->second.windows; !std::ranges::contains(windows, window))
			windows.push_back(window);
	}
}

void WindowManager::on_touch_window(const PHLWINDOW &window, Desktop::eFocusReason)
{
	if (!window)
		return;
	if (window_switcher.is_active()) {
		log<LogLevel::DEBUG, "window switcher is active, ignoring touch">();
		return;
	}
	const char *app_id = app_id_pool.get(window->m_initialClass);
	if (auto it = app_id_to_stuff_map.find(app_id); it == app_id_to_stuff_map.end()) {
		auto [it_new, _] = app_id_to_stuff_map.try_emplace(app_id);
		it_new->second.windows.push_back(window);
		it_new->second.app_info = app_info_loader.get_app_info(app_id, window->m_initialClass);
		app_id_focus_history.insert(app_id_focus_history.begin(), app_id);
	} else {
		auto &windows   = it->second.windows;
		auto  app_id_it = std::ranges::find(app_id_focus_history, app_id);
		std::rotate(app_id_focus_history.begin(), app_id_it, app_id_it + 1);
		if (auto window_it = std::ranges::find(windows, window); window_it != windows.end())
			std::rotate(windows.begin(), window_it, window_it + 1);
		else // app id changed at runtime
			windows.insert(windows.begin(), window);
	}
}

void WindowManager::on_close_window(const PHLWINDOW &window)
{
	if (!window)
		return;
	const char *app_id = app_id_pool.find(window->m_initialClass);
	if (!app_id)
		return;
	auto &windows = app_id_to_stuff_map.find(app_id)->second.windows;
	window_switcher.on_close_window(window);
	llvm::erase(windows, window);
	if (windows.empty()) {
		app_switcher.on_close_app(app_id);
		app_id_to_stuff_map.erase(app_id);
		std::erase(app_id_focus_history, app_id);
		app_info_loader.prune(app_id_focus_history);
		app_id_pool.remove(app_id);
	}
}

ActionResult WindowManager::focus_or_exec(const char *app_id, const char *command)
{
	const char *app_id_ptr = app_id_pool.find(app_id);
	if (!app_id_ptr) {
		if (!Config::Supplementary::executor()->spawn(command))
			return actionError(
			    std::format("Failed to spawn {}", command),
			    eActionErrorLevel::INFO,
			    eActionErrorCode::EXECUTION_FAILED
			);
		return {};
	}
	focus_and_raise_window(app_id_to_stuff_map.find(app_id_ptr)->second.windows.front().lock());
	return {};
}

ActionResult WindowManager::move_or_exec(const char *app_id, const char *command)
{
	const char *app_id_ptr = app_id_pool.find(app_id);
	if (!app_id_ptr) {
		if (!Config::Supplementary::executor()->spawn(command))
			return actionError(
			    std::format("Failed to spawn {}", command),
			    eActionErrorLevel::INFO,
			    eActionErrorCode::EXECUTION_FAILED
			);
		return {};
	}
	auto window  = app_id_to_stuff_map.find(app_id_ptr)->second.windows.front().lock();
	auto monitor = Desktop::focusState()->monitor();
	if (!monitor) {
		return actionError(
		    "Monitor not found", eActionErrorLevel::INFO, eActionErrorCode::NOT_FOUND
		);
	}
	if (auto active_workspace = monitor->m_activeWorkspace;
	    window->m_workspace != active_workspace) {
		log<LogLevel::TRACE, "moving window '{}' from workspace '{}' to the active workspace '{}'">(
		    window.get(), window->m_workspace->m_name, active_workspace->m_name
		);
		g_pCompositor->moveWindowToWorkspaceSafe(window, active_workspace);
	} else {
		g_pCompositor->warpCursorTo(window->middle());
	}
	log<LogLevel::TRACE, "focusing {}">(window.get());
	focus_and_raise_window(window);
	return {};
}

void WindowManager::on_key_press(IKeyboard::SKeyEvent e, Event::SCallbackInfo &info)
{
	static bool mod_held   = false;
	static bool shift_held = false;

	auto state = e.state;

	switch (e.keycode) {
	case SWITCHER_MOD:
		mod_held = state;
		if (!state) {
			// mod released
			// both of them cannot be active at the same time by design
			if (window_switcher.is_active())
				window_switcher.deactivate();
			else if (app_switcher.is_active())
				app_switcher.focus_selected();
		}
		info.cancelled = false;
		break;
	case KEY_LEFTSHIFT: [[fallthrough]];
	case KEY_RIGHTSHIFT:
		shift_held     = state;
		info.cancelled = false;
		break;
	case KEY_TAB:
		if (mod_held) {
			if (state)
				handle_app_switching(shift_held);
			info.cancelled = true;
			break;
		}
		info.cancelled = false;
		break;
	case KEY_GRAVE:
		if (mod_held) {
			if (state)
				handle_window_switching(shift_held);
			info.cancelled = true;
			break;
		}
		info.cancelled = false;
		break;
	default: info.cancelled = false;
	}
}

void WindowManager::handle_app_switching(bool backwards)
{
	if (window_switcher.is_active())
		window_switcher.deactivate();
	if (!app_switcher.is_active())
		app_switcher.activate(&app_id_focus_history, &app_id_to_stuff_map);
	app_switcher.highlight_next(backwards);
}

void WindowManager::handle_window_switching(bool backwards)
{
	if (app_switcher.is_active())
		app_switcher.focus_selected();
	if (!window_switcher.is_active()) {
		auto last_window = Desktop::focusState()->window();
		if (!last_window)
			return;
		const char *app_id = app_id_pool.find(last_window->m_initialClass);
		if (!app_id)
			return;
		window_switcher.activate(&app_id_to_stuff_map.find(app_id)->second.windows);
	}
	window_switcher.focus_next(backwards);
}

void WindowManager::render_app_switcher(eRenderStage stage)
{
	if (app_switcher.is_active() && stage == RENDER_POST_WINDOWS)
		g_pHyprRenderer->m_renderPass.add(makeUnique<AppSwitcherPassElement>(&app_switcher));
}

ActionResult WindowManager::dump_debug_info() { return {}; }
