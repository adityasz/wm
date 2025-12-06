#pragma once

#include <span>
#include <vector>

#include "Hyprland.h"

/// We can have a window switcher that displays previews (above/below the app
/// switcher if it is open). But that would require taking a snapshot of windows
/// and that's a ton of code that I do not want to write, especially when I find
/// the current setup to be good enough.
class WindowSwitcher {
	// WindowManager::on_touch_window does not modify this when active = true
	std::span<PHLWINDOWREF> app_windows;
	// All windows of the app are raised when window switching starts.
	// On abort (end), windows (other than focused window) are reset to their
	// original z-order.
	std::vector<PHLWINDOW>  initial_windows;
	int                     idx;
	bool                    active;

public:
	WindowSwitcher();

	void               seed(std::span<PHLWINDOWREF> app_windows);
	void               move(bool backwards);
	void               focus_selected();
	void               abort();
	[[nodiscard]] bool is_active() const;
};
