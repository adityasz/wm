module;

#include <cassert>

module wm.WindowSwitcher;

import std;
import hyprland.globals;
import hyprland.desktop;
import hyprland.render;
import wm.Support.Logging;
import wm.Support.Utils;

using std::size_t;

using namespace wm;

WindowSwitcher::WindowSwitcher() : app_windows(nullptr), idx(0), active(false) {}

void WindowSwitcher::activate(const char *app_id, llvm::SmallVectorImpl<PHLWINDOWREF> *app_windows)
{
	if (app_windows->size() == 1)
		return;
	idx               = 0;
	active            = true;
	this->app_id      = app_id;
	this->app_windows = app_windows;
}

void WindowSwitcher::focus_next(bool backwards)
{
	if (app_windows->empty()) {
		log<LogLevel::DEBUG, "WindowSwitcher: no windows to switch, refusing to activate">();
		return;
	}

	if (backwards) {
		if (idx)
			idx--;
		else
			idx = static_cast<int>(app_windows->size() - 1);
	} else {
		idx++;
		if (idx == static_cast<int>(app_windows->size()))
			idx = 0;
	}

	assert((idx >= 0 && idx < static_cast<int>(app_windows->size())) && "idx out of bounds");

	focus_and_raise_window(idx);
}

void WindowSwitcher::deactivate()
{
	auto curr_window = std::next(app_windows->begin(), idx);
	std::rotate(app_windows->begin(), curr_window, std::next(curr_window));
	active = false;
}

const char *WindowSwitcher::current_app_id() const { return app_id; }

void WindowSwitcher::update_app_windows(llvm::SmallVectorImpl<PHLWINDOWREF> *app_windows)
{ this->app_windows = app_windows; }

void WindowSwitcher::on_close_window(const PHLWINDOW &closing_window)
{
	for (const auto &[i, window] : *app_windows | std::views::enumerate) {
		if (window == closing_window) {
			if (idx == i) {
				if (static_cast<size_t>(idx) == app_windows->size() - 1) {
					idx = 0;
					focus_and_raise_window(idx);
				} else {
					focus_and_raise_window(idx + 1);
				}
			} else if (idx > i) {
				idx--;
			}
			// only one window will remain after this one is closed
			if (app_windows->size() == 2)
				return deactivate();
			break;
		}
	}
}

bool WindowSwitcher::is_active() const { return active; }

void WindowSwitcher::focus_and_raise_window(size_t idx)
{ return wm::focus_and_raise_window((*app_windows)[idx].lock()); }
