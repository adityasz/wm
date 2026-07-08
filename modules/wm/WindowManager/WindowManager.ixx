export module wm.WindowManager;

import std;
import absl;

import hyprland.globals;
import hyprland.config;
import hyprland.devices;
import hyprland.desktop;
import hyprland.event;
import hyprutils.math;
import hyprutils.memory;

import wm.Support.StringPool;

export import wm.AppInfoLoader;
export import wm.AppSwitcher;
export import wm.WindowSwitcher;

using Config::Actions::ActionResult;
using Desktop::View::CWindow;
using Hyprutils::Math::Vector2D;
using Hyprutils::Memory::CSharedPointer;

export namespace wm {

struct [[gnu::visibility("hidden")]] WindowManagerConfig {
	AppSwitcherConfig app_switcher;

	WindowManagerConfig(void *handle);
};

struct WindowInfo {
	Vector2D        position;
	Vector2D        size;
	bool            floating;
	eFullscreenMode mode;
};

using AppEntryResult = std::pair<absl::flat_hash_map<const char *, AppStuff>::iterator, bool>;

enum class DesktopFileStatus { Scanning, NoDesktopFile, HasDesktopFile };

class [[gnu::visibility("hidden")]] WindowManager {
	std::vector<const char *>                   app_id_focus_history;
	absl::flat_hash_map<const char *, AppStuff> app_id_to_stuff_map;
	/// If the desktop file for an app ID is not found, app ID is stored here.
	/// No BumpPtrAllocator because this is rare and can be used adversarially.
	OwnedStringPool                             app_id_pool;

public:
	absl::flat_hash_map<CWindow *, WindowInfo> window_info_map;

private:
	WindowSwitcher window_switcher;
	AppSwitcher    app_switcher;

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
	std::tuple<const char *, std::string_view, DesktopFileStatus>
	               resolve_app_id(std::string_view hl_class);
	AppEntryResult get_or_create_app_entry(std::string_view hl_class);
	void           handle_window_switching(bool backwards);
	void           handle_app_switching(bool backwards);
	/// If `window` exists in `window_info_map` and is currently not
	/// fullscreened, re-apply the remembered mode (Hyprland displaced it
	/// when another window got maximized/fullscreened).
	void           maybe_restore_fullscreen(const PHLWINDOW &window) const;
	std::variant<PHLWINDOW, ActionResult>
	find_window_or_spawn(const char *app_id, const char *command);
};

} // namespace wm
