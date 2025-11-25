#pragma once

#include <gch/small_vector.hpp>

#include "AppSwitcher.h"
#include "WindowSwitcher.h"

struct QuickAccessApp {
	/// The Wayland application ID of the app's windows.
	///
	/// Since it is small and frequently accessed, it makes sense to take
	/// advantage of std::string's SSO.
	std::string             app_id;
	/// The command used to launch the app.
	const Hyprlang::STRING *command;
};

class WindowManager {
	std::vector<std::string>                  app_id_focus_history; // mru order
	std::unordered_map<std::string, AppStuff> app_id_to_stuff_map;
	WindowSwitcher                            window_switcher;
	AppSwitcher                               app_switcher;
	AppInfoLoader                             app_info_loader;

	static std::array<QuickAccessApp, NUM_QUICK_ACCESS_APPS> quick_access_apps;

public:
	WindowManager();

	void reload_config();

	void on_open_window(const PHLWINDOW &window);
	void on_touch_window(const PHLWINDOW &window);
	void on_close_window(const PHLWINDOW &window);

	bool on_key_press(uint32_t key, wl_keyboard_key_state state);

	void render_app_switcher();

	/// Launch (a new window of) the `n`-th quick access app.
	static SDispatchResult exec(int n);
	/// Focus the last used window of the `n`-th quick access app or launch it.
	SDispatchResult        focus_or_exec(int n);
	/// Focus the last used window of the `n`-th quick access app after moving
	/// it to the current workspace if needed, or launch it.
	SDispatchResult        move_or_exec(int n);

	SDispatchResult        dump_debug_info();

private:
	void load_icon_textures();
	void handle_window_switching(bool backwards);
	void handle_app_switching(bool backwards);
};
