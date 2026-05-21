export module wm.WindowSwitcher;

import std;
import llvm.ADT;
import hyprland.desktop;

export namespace wm {
/// We can have a window switcher that displays previews (above/below the app
/// switcher if it is open). But that would require taking a snapshot of windows
/// and that's a ton of code that I do not want to write, especially when I find
/// the current setup to be good enough.
class WindowSwitcher {
	// WindowManager::on_touch_window does not modify this when active = true
	llvm::SmallVectorImpl<PHLWINDOWREF> *app_windows;
	// All windows of the app are raised when window switching starts.
	// On abort (end), windows (other than focused window) are reset to their
	// original z-order. Using vector because of small vector here because
	// this is only created once and can go to the heap.
	std::vector<PHLWINDOW>               initial_windows;
	int                                  idx;
	bool                                 active;

public:
	WindowSwitcher();

	void               seed(llvm::SmallVectorImpl<PHLWINDOWREF> *app_windows);
	void               move(bool backwards);
	void               focus_selected();
	void               abort();
	void               on_close_window(const PHLWINDOW &closing_window);
	[[nodiscard]] bool is_active() const;
};
} // namespace wm
