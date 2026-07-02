export module wm.WindowManager;

import std;
import hyprland.globals;
import hyprland.config;
import hyprland.devices;
import hyprland.desktop;
import hyprland.event;
import hyprutils.math;
import hyprutils.memory;
import wm.Support;
import absl;

export import wm.AppSwitcher;
export import wm.WindowSwitcher;

using Config::Actions::ActionResult, Config::Values::CFloatValue;
using Desktop::View::CWindow;
using Hyprutils::Math::Vector2D;
using Hyprutils::Memory::CSharedPointer;

export namespace wm {

struct [[gnu::visibility("hidden")]] WindowManagerConfig {
	AppSwitcherConfig   app_switcher;
	AppInfoLoaderConfig app_info_loader;

	WindowManagerConfig(void *handle, const CSharedPointer<CFloatValue> &icon_size_config);
};

struct WindowInfo {
	Vector2D        position;
	Vector2D        size;
	bool            floating;
	eFullscreenMode mode;
};

class [[gnu::visibility("hidden")]] WindowManager {
	StringPool                                  app_id_pool;
	std::vector<const char *>                   app_id_focus_history;
	absl::flat_hash_map<const char *, AppStuff> app_id_to_stuff_map;
	absl::flat_hash_map<CWindow *, WindowInfo>  window_info_map;
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
	/// Toggle maximized/fullscreen. `mode` can be `FSMODE_{MAXIMIZED,FULLSCREEN}`.
	ActionResult fullscreen(
	    eFullscreenMode mode, bool toggle, const std::optional<PHLWINDOW> &window = std::nullopt
	);

	ActionResult dump_debug_info();

	[[nodiscard]] bool is_app_switcher_active() const;

private:
	void handle_window_switching(bool backwards);
	void handle_app_switching(bool backwards);
	/// If `window` exists in `window_info_map` and is currently not
	/// fullscreened, re-apply the remembered mode (Hyprland displaced it
	/// when another window got maximized/fullscreened).
	void maybe_restore_fullscreen(const PHLWINDOW &window) const;
};

} // namespace wm
