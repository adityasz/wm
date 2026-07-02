module;

#include <linux/input-event-codes.h>

module wm.WindowManager;

import std;
import hyprland.config;
import hyprland.globals;
import hyprland.layout;
import hyprland.render;
import hyprutils.math;
import hyprutils.memory;
import llvm.Support;
import wm.Support;

using Config::Actions::ActionResult;
using Config::Actions::actionError;
using Config::Actions::eActionErrorCode;
using Config::Actions::eActionErrorLevel;
using Config::Actions::eTogglableAction;
using Config::Values::CFloatValue;
using Hyprutils::Math::CBox;
using Hyprutils::Memory::CSharedPointer, Hyprutils::Memory::makeUnique;

using namespace wm;

WindowManagerConfig::WindowManagerConfig(
    void *handle, const CSharedPointer<CFloatValue> &icon_size_config
) :
    app_switcher({handle, icon_size_config}),
    app_info_loader({handle, icon_size_config})
{}

WindowManager::WindowManager(const WindowManagerConfig &config) :
    app_switcher(config.app_switcher),
    app_info_loader(config.app_info_loader),
    config(config)
{
	window_info_map.reserve(10);
	app_id_to_stuff_map.reserve(20);
	for (const auto &window :
	     Desktop::History::windowTracker()->fullHistory() | std::views::reverse) {
		auto [app_id, inserted] = app_id_pool.get(window->m_initialClass);
		if (inserted) {
			app_id_focus_history.push_back(app_id);
			app_id_to_stuff_map[app_id].app_info = app_info_loader.get_app_info(app_id, app_id);
		}
		app_id_to_stuff_map[app_id].windows.push_back(window);
	}
}

void WindowManager::reset_config()
{
	if (app_switcher.is_active()) [[unlikely]]
		app_switcher.deactivate();
	if (window_switcher.is_active()) [[unlikely]]
		window_switcher.deactivate();

	app_switcher.reset_config(config.app_switcher);
	app_info_loader.reset_config(config.app_info_loader);
}

void WindowManager::on_open_window(const PHLWINDOW &window)
{
	if (!window)
		return;
	auto [app_id, inserted] = app_id_pool.get(window->m_initialClass);
	if (inserted) {
		auto [it_new, _] = app_id_to_stuff_map.try_emplace(app_id);
		it_new->second.windows.push_back(window);
		it_new->second.app_info = app_info_loader.get_app_info(app_id, app_id);
		app_id_focus_history.push_back(app_id);
		if (window_switcher.is_active()) [[unlikely]] {
			window_switcher.update_app_windows(
			    &app_id_to_stuff_map.find(window_switcher.current_app_id())->second.windows
			);
		}
	} else if (
	    auto &windows = app_id_to_stuff_map.find(app_id)->second.windows;
	    !std::ranges::contains(windows, window)
	) {
		// Hyprland calls this twice sometimes
		windows.push_back(window);
	}
}

void WindowManager::maybe_restore_fullscreen(const PHLWINDOW &window) const
{
	if (auto it = window_info_map.find(window.get());
	    it != window_info_map.end()
	    && window->m_fullscreenState.internal == eFullscreenMode::FSMODE_NONE) {
		// window was maximized/fullscreened before, but then some other window
		// got maximized/fullscreened, and Hyprland restored this window.
		auto _ = Config::Actions::fullscreenWindow(it->second.mode, window);
	}
}

void WindowManager::on_touch_window(const PHLWINDOW &window, Desktop::eFocusReason)
{
	// active event can be emitted after close event in 0.55.4:
	// https://github.com/hyprwm/Hyprland/blob/v0.55.4/src/desktop/view/Window.cpp#L2437
	// focus reason is "other", and I don't expect this to be the only place
	// where this reason is used; so, check if the window is mapped instead:
	if (!window || !window->m_isMapped)
		return;
	if (window_switcher.is_active()) {
		log<LogLevel::TRACE, "window switcher is active, ignoring touch">();
		return;
	}
	auto [app_id, new_app] = app_id_pool.get(window->m_initialClass);
	if (new_app) {
		auto [it_new, _] = app_id_to_stuff_map.try_emplace(app_id);
		it_new->second.windows.push_back(window);
		it_new->second.app_info = app_info_loader.get_app_info(app_id, app_id);
		app_id_focus_history.insert(app_id_focus_history.begin(), app_id);
	} else {
		auto &windows   = app_id_to_stuff_map.find(app_id)->second.windows;
		auto  app_id_it = std::ranges::find(app_id_focus_history, app_id);
		std::rotate(app_id_focus_history.begin(), app_id_it, app_id_it + 1);
		if (auto window_it = std::ranges::find(windows, window); window_it != windows.end()) {
			std::rotate(windows.begin(), window_it, window_it + 1);
		} else {
			// active event emitted before open event, which does happen in 0.55.4
			windows.insert(windows.begin(), window);
			return;
		}
	}
	maybe_restore_fullscreen(window);
}

void WindowManager::on_close_window(const PHLWINDOW &window)
{
	if (!window)
		return;
	const char *app_id = app_id_pool.find(window->m_initialClass);
	if (!app_id) [[unlikely]]
		return;
	auto &windows = app_id_to_stuff_map.find(app_id)->second.windows;
	if (window_switcher.is_active()) [[unlikely]]
		window_switcher.on_close_window(window);
	llvm::erase(windows, window);
	if (windows.empty()) {
		if (app_switcher.is_active()) [[unlikely]]
			app_switcher.on_close_app(app_id);
		app_id_to_stuff_map.erase(app_id);
		std::erase(app_id_focus_history, app_id);
		app_info_loader.prune(app_id_focus_history);
		app_id_pool.remove(app_id);
	}
	window_info_map.erase(window.get());
}

ActionResult WindowManager::focus_or_exec(const char *app_id, const char *command)
{
	const char *app_id_ptr = app_id_pool.find(app_id);
	if (!app_id_ptr) {
		if (Config::Supplementary::executor()->spawn(command)) [[likely]]
			return {};
		return actionError(
		    std::format("Failed to spawn {}", command),
		    eActionErrorLevel::ERROR,
		    eActionErrorCode::EXECUTION_FAILED
		);
	}
	focus_and_raise_window(app_id_to_stuff_map.find(app_id_ptr)->second.windows.front().lock());
	return {};
}

ActionResult WindowManager::move_or_exec(const char *app_id, const char *command)
{
	const char *app_id_ptr = app_id_pool.find(app_id);
	if (!app_id_ptr) {
		if (Config::Supplementary::executor()->spawn(command)) [[likely]]
			return {};
		return actionError(
		    std::format("Failed to spawn {}", command),
		    eActionErrorLevel::ERROR,
		    eActionErrorCode::EXECUTION_FAILED
		);
	}
	auto window  = app_id_to_stuff_map.find(app_id_ptr)->second.windows.front().lock();
	auto monitor = Desktop::focusState()->monitor();
	if (!monitor) [[unlikely]] {
		return actionError(
		    "Monitor not found", eActionErrorLevel::ERROR, eActionErrorCode::NOT_FOUND
		);
	}
	if (auto active_workspace = monitor->m_activeWorkspace; // some checks may be redundant
	    window->m_workspace != active_workspace) {
		if (!window->m_workspace || !active_workspace) [[unlikely]] {
			return actionError(
			    "Some workspace is null", eActionErrorLevel::ERROR, eActionErrorCode::INVALID_STATE
			);
		}
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

ActionResult
WindowManager::fullscreen(eFullscreenMode mode, bool toggle, const std::optional<PHLWINDOW> &w)
{
	auto window = w.value_or(Desktop::focusState()->window());
	if (!window) [[unlikely]] {
		return actionError(
		    "No window is focused", eActionErrorLevel::ERROR, eActionErrorCode::NOT_FOUND
		);
	}

	auto curr_mode = window->m_fullscreenState.internal;

	eFullscreenMode desired_mode;
	if (toggle && curr_mode == mode)
		desired_mode = eFullscreenMode::FSMODE_NONE;
	else
		desired_mode = mode;

	if (curr_mode == desired_mode) [[unlikely]]
		return {};

	if (desired_mode != eFullscreenMode::FSMODE_NONE) {
		if (auto it = window_info_map.find(window.get()); it != window_info_map.end()) {
			it->second.mode = desired_mode;
		} else {
			window_info_map.try_emplace(
			    window.get(),
			    WindowInfo{
			        .position = window->m_position,
			        .size     = window->m_size,
			        .floating = window->m_isFloating,
			        .mode     = desired_mode,
			    }
			);
			if (!window->m_isFloating) {
				auto _ =
				    Config::Actions::floatWindow(eTogglableAction::TOGGLE_ACTION_ENABLE, window);
			}
			if (auto monitor = window->m_monitor.lock()) [[likely]] {
				if (auto target = window->layoutTarget()) [[likely]] {
					auto box      = window->m_workspace->m_space->workArea(true);
					auto reserved = window->getFullWindowReservedArea();
					box.x        += reserved.topLeft.x;
					box.y        += reserved.topLeft.y;
					box.w        -= reserved.topLeft.x + reserved.bottomRight.x;
					box.h        -= reserved.topLeft.y + reserved.bottomRight.y;
					g_layoutManager->setTargetGeom(box, target);
				}
			}
		}
		return Config::Actions::fullscreenWindow(desired_mode, window);
	}

	if (auto it = window_info_map.find(window.get()); it != window_info_map.end()) [[likely]] {
		auto _ = Config::Actions::fullscreenWindow(eFullscreenMode::FSMODE_NONE, window);
		if (it->second.floating) {
			window->layoutTarget()->setPositionGlobal(CBox{it->second.position, it->second.size});
		} else {
			// no way to remember last floating position?
			if (auto target = window->layoutTarget()) [[likely]]
				target->rememberFloatingSize(it->second.size);
			auto _ = Config::Actions::floatWindow(eTogglableAction::TOGGLE_ACTION_DISABLE, window);
		}
		window_info_map.erase(it);
		return {};
	}

	return Config::Actions::fullscreenWindow(desired_mode, window);
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
			if (window_switcher.is_active()) {
				window_switcher.deactivate();
				if (auto window = Desktop::focusState()->window())
					maybe_restore_fullscreen(window);
			} else if (app_switcher.is_active()) {
				app_switcher.focus_selected();
			}
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
	if (window_switcher.is_active()) [[unlikely]]
		window_switcher.deactivate();
	if (!app_switcher.is_active())
		app_switcher.activate(&app_id_focus_history, &app_id_to_stuff_map);
	if (app_switcher.is_active()) [[likely]]
		app_switcher.highlight_next(backwards);
}

void WindowManager::handle_window_switching(bool backwards)
{
	if (app_switcher.is_active()) [[unlikely]]
		app_switcher.focus_selected();
	if (!window_switcher.is_active()) {
		auto last_window = Desktop::focusState()->window();
		if (!last_window) [[unlikely]]
			return;
		const char *app_id = app_id_pool.find(last_window->m_initialClass);
		if (!app_id) [[unlikely]]
			return;
		window_switcher.activate(app_id, &app_id_to_stuff_map.find(app_id)->second.windows);
	}
	if (window_switcher.is_active()) [[likely]]
		window_switcher.focus_next(backwards);
}

void WindowManager::render_app_switcher(eRenderStage stage)
{
	if (app_switcher.is_active() && stage == RENDER_POST_WINDOWS)
		g_pHyprRenderer->m_renderPass.add(makeUnique<AppSwitcherPassElement>(&app_switcher));
}

ActionResult WindowManager::dump_debug_info() { return {}; }

bool WindowManager::is_app_switcher_active() const { return app_switcher.is_active(); }
