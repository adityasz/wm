#include "WindowManager.h"
#include "AppInfoLoader.h"
#include "AppSwitcherPassElement.h"
#include "Globals.h"
#include "Logging.h"
#include <future>
#include <hyprlang.hpp>
#include <src/desktop/DesktopTypes.hpp>

using namespace std::chrono_literals;

std::array<QuickAccessApp, NUM_QUICK_ACCESS_APPS> WindowManager::quick_access_apps = [] {
	std::array<QuickAccessApp, NUM_QUICK_ACCESS_APPS> arr;
	arr.fill({"", nullptr});
	return arr;
}();

WindowManager::WindowManager()
{
	for (int i = 0; i < NUM_QUICK_ACCESS_APPS; i++)
		quick_access_apps[i].command = get_config<char>(std::format("app_{}:command", i));

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
	for (int i = 0; i < NUM_QUICK_ACCESS_APPS; i++) {
		quick_access_apps[i].app_id = *get_config<char>(std::format("app_{}:class", i));
		if (auto &[app_id, command] = quick_access_apps[i]; !(app_id.empty() && strlen(*command)))
			log(INFO, "app {}: class={}, command={}", i, app_id, *command);
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
}

void WindowManager::on_touch_window(const PHLWINDOW &window)
{
	LOG_TRACE("{}", as_str(window));
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
	auto &windows = app_id_to_stuff_map[window->m_class].windows;
	gch::erase(windows, window);
	if (windows.empty()) {
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

	if (!strlen(*quick_access_apps[n].command))
		return oob(n);

	log(INFO, "executing {}", *quick_access_apps[n].command);
	CKeybindManager::spawn(*quick_access_apps[n].command);
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
	g_pCompositor->focusWindow(window);
	g_pCompositor->changeWindowZOrder(window, true);

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
		    as_str(window),
		    window->m_workspace->m_name,
		    active_workspace->m_name);
		g_pCompositor->moveWindowToWorkspaceSafe(window, active_workspace);
	} else {
		g_pCompositor->warpCursorTo(window->middle());
	}
	log(INFO, "focusing {}", as_str(window));
	g_pCompositor->focusWindow(window);
	g_pCompositor->changeWindowZOrder(window, true);
	return {};
}

bool WindowManager::on_key_press(uint32_t key, wl_keyboard_key_state state)
{
	static bool super_held = false;
	static bool shift_held = false;

	switch (key) {
	case SWITCHER_MOD:
		super_held = state;
		if (!state) {
			window_switcher.active = false;
			window_switcher.seed({});
			if (app_switcher.is_active())
				app_switcher.focus_selected();
		}
		return false;
	case KEY_LEFTSHIFT:
	case KEY_RIGHTSHIFT: shift_held = state; return false;
	case KEY_TAB:
		if (super_held) {
			if (state)
				handle_app_switching(shift_held);
			return true;
		}
		return false;
	case KEY_GRAVE:
		if (super_held) {
			if (state)
				handle_window_switching(shift_held);
			return true;
		}
		return false;
	default: return false;
	}
}

void WindowManager::load_icon_textures()
{
	for (auto &app_stuff : app_id_to_stuff_map | std::views::values) {
		auto &[_, app_info] = app_stuff;
		if (auto future = std::get_if<std::future<AppInfo *>>(&app_info)) {
			if (future->wait_for(0s) != std::future_status::ready)
				continue;
			if (auto app_info_ptr = future->get(); !app_info_ptr->icon.buffer) {
				app_info = AppRenderData{app_info_ptr->name, {}};
			} else {
				auto &icon      = app_info_ptr->icon;
				auto  texture   = makeShared<CTexture>();
				texture->m_size = {
				    static_cast<double>(icon.width), static_cast<double>(icon.height)
				};

				glGenTextures(1, &texture->m_texID);
				glBindTexture(GL_TEXTURE_2D, texture->m_texID);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

				auto format = icon.format != ImageFormat::RGB ? GL_RGBA : GL_RGB;
				switch (icon.format) {
				case ImageFormat::RGB:  format = GL_RGB; break;
				case ImageFormat::BGRA: // swizzling done later
				case ImageFormat::RGBA: format = GL_RGBA; break;
				}
				glTexImage2D(
				    GL_TEXTURE_2D,
				    0,
				    format,
				    icon.width,
				    icon.height,
				    0,
				    format,
				    GL_UNSIGNED_BYTE,
				    icon.buffer.get()
				);
				if (icon.format == ImageFormat::BGRA) {
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
				}
				glBindTexture(GL_TEXTURE_2D, 0);

				app_info = AppRenderData{app_info_ptr->name, texture};
			}
		}
	}
}

void WindowManager::handle_app_switching(bool backwards)
{
	LOG_TRACE("backwards = {}", backwards);
	if (window_switcher.active) {
		window_switcher.active = false;
		window_switcher.seed({});
	}
	if (!app_switcher.is_active()) {
		load_icon_textures();
		app_switcher.show(app_id_focus_history, &app_id_to_stuff_map);
	}
	app_switcher.move(backwards);
}

void WindowManager::handle_window_switching(bool backwards)
{
	LOG_TRACE("backwards = {}", backwards);
	if (app_switcher.is_active())
		app_switcher.focus_selected();
	if (!window_switcher.active) {
		window_switcher.active = true;
		auto last_window       = g_pCompositor->m_lastWindow;
		if (!last_window)
			return;
		auto it = app_id_to_stuff_map.find(last_window->m_class);
		if (it == app_id_to_stuff_map.end())
			return;
		log(INFO,
		    "Window switching {}: {}",
		    last_window->m_class,
		    it->second.windows | std::views::transform([](auto window_ptr) {
			    return std::format("0x{:x}", reinterpret_cast<uintptr_t>(window_ptr.get()));
		    }) | std::ranges::to<std::vector>());
		window_switcher.seed(it->second.windows);
	}
	window_switcher.move(backwards);
}

void WindowManager::render_app_switcher()
{
	if (app_switcher.is_active())
		g_pHyprRenderer->m_renderPass.add(makeUnique<AppSwitcherPassElement>(&app_switcher));
}
