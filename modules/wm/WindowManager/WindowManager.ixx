export module wm.WindowManager;

import std;
import hyprland.globals;
import hyprland.config;
import hyprland.devices;
import hyprland.desktop;
import hyprland.event;
import wm.Support;
import absl;

export import wm.AppSwitcher;
export import wm.WindowSwitcher;

using Config::Actions::ActionResult;

export namespace wm {

struct WindowManagerConfig {
	AppSwitcherConfig   app_switcher;
	AppInfoLoaderConfig app_info_loader;
};

class WindowManager {
	StringPool                                  app_id_pool;
	std::vector<const char *>                   app_id_focus_history;
	absl::flat_hash_map<const char *, AppStuff> app_id_to_stuff_map;
	WindowSwitcher                              window_switcher;
	AppSwitcher                                 app_switcher;
	AppInfoLoader                               app_info_loader;
	WindowManagerConfig                         config;

public:
	explicit WindowManager(const WindowManagerConfig &config);

	void reset_config();

	void on_open_window(const PHLWINDOW &window);
	void on_touch_window(const PHLWINDOW &window, Desktop::eFocusReason);
	void on_close_window(const PHLWINDOW &window);

	void on_key_press(IKeyboard::SKeyEvent e, Event::SCallbackInfo &info);

	void render_app_switcher(eRenderStage stage);

	/// Focus the last used window of app or launch it.
	ActionResult focus_or_exec(const char *app_id, const char *command);
	/// Focus the last used window of an app after moving it to the current
	/// workspace if needed, or launch it.
	ActionResult move_or_exec(const char *app_id, const char *command);

	ActionResult dump_debug_info();

	[[nodiscard]] bool is_app_switcher_active() const;

private:
	void handle_window_switching(bool backwards);
	void handle_app_switching(bool backwards);
};

} // namespace wm
