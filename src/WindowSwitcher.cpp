#include "WindowSwitcher.h"
#include "Logging.h"

void WindowSwitcher::move(bool backwards)
{
	if (app_windows.empty())
		return;

	if (backwards) {
		if (idx)
			idx--;
		else
			idx = app_windows.size() - 1;
	} else {
		idx++;
		if (idx == app_windows.size())
			idx = 0;
	}
	if (auto window = app_windows[idx].lock()) {
		log(INFO, "    switching to {}", as_str(window));
		g_pCompositor->m_windows = this->all_windows;
		g_pCompositor->focusWindow(window, nullptr, true);
		g_pCompositor->changeWindowZOrder(window, true);
		return;
	}
	log(INFO, "    {} became null", as_str(app_windows[idx]));
	app_windows.erase(app_windows.begin() + idx);
}

void WindowSwitcher::seed(std::span<PHLWINDOWREF> windows)
{
	idx    = 0;
	active = true;
	this->app_windows.clear();
	this->app_windows.reserve(windows.size());
	for (const auto &window : windows)
		this->app_windows.push_back(window);
	this->all_windows = g_pCompositor->m_windows;
}

void WindowSwitcher::focus_selected()
{
	active = false;
	if (auto window = app_windows[idx].lock()) {
		log(INFO, "    switching to {}", as_str(window));
		g_pCompositor->m_windows = this->all_windows;
		g_pCompositor->focusWindow(window);
		g_pCompositor->changeWindowZOrder(window, true);
		return;
	}
	// should not happen since we have shared pointers in all_windows
	std::runtime_error(
	    std::format("[wm] window switcher: {} became null", as_str(app_windows[idx]))
	);
}

void WindowSwitcher::abort()
{
	active                   = false;
	g_pCompositor->m_windows = this->all_windows;
}

bool WindowSwitcher::is_active() { return active; }
