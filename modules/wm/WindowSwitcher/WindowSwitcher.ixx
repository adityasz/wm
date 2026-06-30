export module wm.WindowSwitcher;

import std;
import llvm.Support;
import hyprland.desktop;

using std::size_t;

export namespace wm {
/// We can have a window switcher that displays previews (above/below the app
/// switcher if it is open). But that would require taking a snapshot of windows
/// and that's a ton of code that I do not want to write, especially when I find
/// the current setup to be good enough.
class [[gnu::visibility("hidden")]] WindowSwitcher {
	// WindowManager::on_touch_window does not modify this when active = true
	llvm::SmallVectorImpl<PHLWINDOWREF> *app_windows;
	const char                          *app_id;
	int                                  idx;
	bool                                 active;

public:
	WindowSwitcher();

	void activate(const char *app_id, llvm::SmallVectorImpl<PHLWINDOWREF> *app_windows);
	void focus_next(bool backwards);
	void deactivate();
	/// Pointers to keys in absl::flat_hash_map are not stable, so this is needed.
	void update_app_windows(llvm::SmallVectorImpl<PHLWINDOWREF> *app_windows);
	void on_close_window(const PHLWINDOW &closing_window);
	[[nodiscard]] bool        is_active() const;
	[[nodiscard]] const char *current_app_id() const;

private:
	void focus_and_raise_window(size_t idx);
};
} // namespace wm
