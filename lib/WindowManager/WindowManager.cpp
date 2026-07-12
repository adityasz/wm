module;

#include <cassert>
#include <linux/input-event-codes.h>

module wm.WindowManager;

import std;
import llvm.Support;

import hyprland.config;
import hyprland.globals;
import hyprland.layout;
import hyprland.render;
import hyprutils.math;
import hyprutils.memory;

import wm.Support.Logging;
import wm.Support.Utils;

using Config::Actions::ActionResult;
using Config::Actions::actionError;
using Config::Actions::eActionErrorCode;
using Config::Actions::eActionErrorLevel;
using Config::Actions::eTogglableAction;
using Hyprutils::Math::CBox;
using Hyprutils::Memory::CSharedPointer, Hyprutils::Memory::makeUnique;

using namespace wm;

WindowManagerConfig::WindowManagerConfig(void *handle) : app_switcher(handle) {}

std::tuple<const char *, std::string_view, DesktopFileStatus>
WindowManager::resolve_app_id(std::string_view hl_class)
{
	if (!app_switcher.app_info_loader.is_available()) [[unlikely]] {
		app_switcher.dirty = true;
		return {app_id_pool.get(hl_class).first, std::string_view{}, DesktopFileStatus::Scanning};
	}
	if (app_switcher.dirty) [[unlikely]] {
		// this code path is unlikely to ever run since scanning desktop files is very fast,
		// unless the plugin was loaded with windows already open
		absl::flat_hash_map<const char *, AppStuff> new_stuff_map;
		new_stuff_map.reserve(app_id_to_stuff_map.capacity());
		for (auto &app_id : app_id_focus_history) {
			auto [app_id_new, name] = app_switcher.app_info_loader.get_app_info(app_id);
			if (app_id_new) [[likely]] {
				auto [it, _] = new_stuff_map.emplace(
				    app_id_new, std::move(app_id_to_stuff_map.find(app_id)->second)
				);
				it->second.icon_texture = app_switcher.load_app_icon(app_id_new);
				app_id_pool.remove(app_id);
				app_id = app_id_new;
			} else {
				auto [it, _] = new_stuff_map.emplace(
				    app_id, std::move(app_id_to_stuff_map.find(app_id)->second)
				);
				it->second.icon_texture = std::monostate{};
			}
		}
		app_id_to_stuff_map = std::move(new_stuff_map);
		app_switcher.dirty  = false;
	}
	auto [app_id, name]                   = app_switcher.app_info_loader.get_app_info(hl_class);
	DesktopFileStatus desktop_file_status = DesktopFileStatus::HasDesktopFile;
	if (!app_id) [[unlikely]] {
		desktop_file_status = DesktopFileStatus::NoDesktopFile;
		app_id              = app_id_pool.get(hl_class).first;
	}
	return {app_id, name, desktop_file_status};
}

AppEntryResult WindowManager::get_or_create_app_entry(std::string_view hl_class)
{
	auto [app_id, name, desktop_file_status] = resolve_app_id(hl_class);

	auto res = app_id_to_stuff_map.try_emplace(
	    app_id, llvm::SmallVector<PHLWINDOWREF>{}, name, std::monostate{}
	);

	auto &[it, inserted] = res;
	if (inserted) {
		app_id_focus_history.push_back(app_id);
		switch (desktop_file_status) {
		case DesktopFileStatus::HasDesktopFile:
			it->second.icon_texture = app_switcher.load_app_icon(app_id);
			break;
		case DesktopFileStatus::NoDesktopFile: it->second.icon_texture = {}; break;
		case DesktopFileStatus::Scanning:      break;
		}
	}

	return res;
}

WindowManager::WindowManager(const WindowManagerConfig &config) : app_switcher(config.app_switcher)
{
	window_info_map.reserve(10);
	app_id_to_stuff_map.reserve(20);
	for (const auto &window :
	     Desktop::History::windowTracker()->fullHistory() | std::views::reverse) {
		auto [it, _] = get_or_create_app_entry(window->m_initialClass);
		it->second.windows.push_back(window);
	}
}

void WindowManager::reset_config()
{
	if (app_switcher.is_active()) [[unlikely]]
		app_switcher.deactivate();
	if (window_switcher.is_active()) [[unlikely]]
		window_switcher.deactivate();

	app_switcher.reset_config();
}

void WindowManager::on_open_window(const PHLWINDOW &window)
{
	if (!window)
		return;

	auto [it, inserted] = get_or_create_app_entry(window->m_initialClass);
	auto &app_windows   = it->second.windows;
	if (inserted) {
		app_windows.push_back(window);
		if (window_switcher.is_active()) [[unlikely]]
			window_switcher.update_app_windows(&app_windows);
	} else {
		assert(!std::ranges::contains(app_windows, window));
		app_windows.push_back(window);
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
	if (!window)
		return;
	if (window_switcher.is_active()) {
		log<LogLevel::TRACE, "window switcher is active, ignoring touch">();
		return;
	}
	auto [app_id, _, _] = resolve_app_id(window->m_initialClass);
	auto &windows       = app_id_to_stuff_map.find(app_id)->second.windows;
	auto  app_id_it     = std::ranges::find(app_id_focus_history, app_id);
	assert(app_id_it != app_id_focus_history.end());
	std::rotate(app_id_focus_history.begin(), app_id_it, app_id_it + 1);
	auto window_it = std::ranges::find(windows, window);
	std::rotate(windows.begin(), window_it, window_it + 1);
	maybe_restore_fullscreen(window);
}

void WindowManager::on_close_window(const PHLWINDOW &window)
{
	if (!window)
		return;
	auto [app_id, _, foo] = resolve_app_id(window->m_initialClass);
	auto it               = app_id_to_stuff_map.find(app_id);
	// Hyprland can emit window.destroy before window.openEarly
	if (it == app_id_to_stuff_map.end())
		return;
	auto &windows = it->second.windows;
	if (window_switcher.is_active()) [[unlikely]]
		window_switcher.on_close_window(window);
	assert(std::ranges::contains(windows, window));
	llvm::erase(windows, window);
	if (windows.empty()) {
		if (app_switcher.is_active()) [[unlikely]]
			app_switcher.on_close_app(app_id);
		app_id_to_stuff_map.erase(app_id);
		std::erase(app_id_focus_history, app_id);
		app_switcher.prune_cache(app_id_focus_history);
		if (foo == DesktopFileStatus::NoDesktopFile || foo == DesktopFileStatus::Scanning)
		    [[unlikely]] {
			app_id_pool.remove(app_id);
		}
	}
	window_info_map.erase(window.get());
}

std::variant<PHLWINDOW, ActionResult>
WindowManager::find_window_or_spawn(const char *app_id, const char *command)
{
	auto [app_id_ptr, _, _] = resolve_app_id(app_id);
	auto it                 = app_id_to_stuff_map.find(app_id_ptr);
	if (it == app_id_to_stuff_map.end()) {
		if (Config::Supplementary::executor()->spawn(command)) [[likely]]
			return ActionResult{};
		return actionError(
		    std::format("Failed to spawn {}", command),
		    eActionErrorLevel::ERROR,
		    eActionErrorCode::EXECUTION_FAILED
		);
	}
	return it->second.windows.front().lock();
}

ActionResult WindowManager::focus_or_exec(const char *app_id, const char *command)
{
	auto ret = find_window_or_spawn(app_id, command);
	if (auto window = std::get_if<PHLWINDOW>(&ret)) {
		focus_and_raise_window(*window);
		return {};
	}
	return std::get<ActionResult>(ret);
}

ActionResult WindowManager::move_or_exec(const char *app_id, const char *command)
{
	auto ret = find_window_or_spawn(app_id, command);
	if (auto res = std::get_if<ActionResult>(&ret))
		return *res;

	auto window  = std::get<PHLWINDOW>(ret);
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
			auto size     = window->m_size;
			auto floating = window->m_isFloating;
			if (auto target = window->layoutTarget(); !floating && target) [[likely]]
				size = target->lastFloatingSize();
			window_info_map.try_emplace(
			    window.get(),
			    WindowInfo{
			        .position = window->m_position,
			        .size     = size,
			        .floating = floating,
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
			if (auto target = window->layoutTarget()) [[likely]]
				g_layoutManager->setTargetGeom(CBox{it->second.position, it->second.size}, target);
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
	[[unlikely]] case KEY_LEFTSHIFT:
		[[fallthrough]];
	[[unlikely]] case KEY_RIGHTSHIFT:
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
	[[likely]] default:
		info.cancelled = false;
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
		auto [app_id, _, _] = resolve_app_id(last_window->m_initialClass);
		window_switcher.activate(app_id, &app_id_to_stuff_map.find(app_id)->second.windows);
	}
	if (window_switcher.is_active()) [[likely]]
		window_switcher.focus_next(backwards);
}

void WindowManager::render_app_switcher(eRenderStage stage)
{
	if (app_switcher.is_active() && stage == RENDER_LAST_MOMENT)
		g_pHyprRenderer->m_renderPass.add(makeUnique<AppSwitcherPassElement>(&app_switcher));
}

ActionResult WindowManager::dump_debug_info() { return {}; }

bool WindowManager::is_app_switcher_active() const { return app_switcher.is_active(); }
