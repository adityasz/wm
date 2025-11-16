#pragma once

#include <span>
#include <vector>

#include "Hyprland.h"

/// We can have a window switcher that displays previews (above/below the app
/// switcher if it is open). But that would require taking a snapshot of windows
/// and that's a ton of code that I do not want to write, especially when I find
/// the current setup to be good enough.
class WindowSwitcher {
	std::vector<PHLWINDOWREF> app_windows;
	std::vector<PHLWINDOW>    all_windows; // to restore z order
	size_t                    idx;
	bool                      active;

public:
	WindowSwitcher() : idx(0), active(false) { app_windows.reserve(10); }

	void seed(std::span<PHLWINDOWREF> windows);
	void move(bool backwards);
	void focus_selected();
	void abort();
	bool is_active();
};
