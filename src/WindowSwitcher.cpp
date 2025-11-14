#include "WindowSwitcher.h"
#include "Logging.h"

bool WindowSwitcher::active = false;

void WindowSwitcher::move(bool backwards)
{
	if (windows.empty())
		return;

	if (backwards) {
		if (idx)
			idx--;
		else
			idx = windows.size() - 1;
	} else {
		idx++;
		if (idx == windows.size())
			idx = 0;
	}
	if (auto window = windows[idx].lock()) {
		log(INFO, "    switching to {}", as_str(window));
		g_pCompositor->focusWindow(window);
		g_pCompositor->changeWindowZOrder(window, true);
		return;
	}
	log(INFO, "    {} became null", as_str(windows[idx]));
	windows.erase(windows.begin() + idx);
}

void WindowSwitcher::seed(std::span<PHLWINDOWREF> windows)
{
	idx = 0;
	this->windows.clear();
	this->windows.reserve(windows.size());
	for (const auto &window : windows)
		this->windows.push_back(window);
}
