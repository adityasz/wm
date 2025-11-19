#include <algorithm>
#include <ranges>

#include "Logging.h"
#include "WindowSwitcher.h"

WindowSwitcher::WindowSwitcher() : idx(0), active(false) {}

void WindowSwitcher::move(bool backwards)
{
	LOG_TRACE("backwards={}", backwards);

	if (app_windows.empty())
		return;

	if (backwards) {
		if (idx)
			idx--;
		else
			idx = app_windows.size() - 1;
	} else {
		idx++;
		if (idx == static_cast<int>(app_windows.size()))
			idx = 0;
	}

	[[likely]]
	if (auto window = app_windows[idx].lock()) {
		log(INFO, "    switching to {}", as_str(window));
		g_pCompositor->focusWindow(window, nullptr, true);
		g_pCompositor->changeWindowZOrder(window, true);
		return;
	}

	throw std::runtime_error(
	    std::format("[wm] WindowSwitcher::move: {} became null", as_str(app_windows[idx]))
	);
}

void WindowSwitcher::seed(std::span<PHLWINDOWREF> app_windows)
{
	LOG_TRACE("{}", app_windows | std::views::transform([](auto &window) {
		                return as_str(window);
	                }));

	idx    = 0;
	active = true;

	this->app_windows = app_windows;

	initial_windows = g_pCompositor->m_windows;

	std::vector<size_t> initial_z_orders;
	initial_z_orders.reserve(app_windows.size());
	for (auto &window : app_windows) {
		[[likely]]
		if (auto it = std::ranges::find(g_pCompositor->m_windows, window.get(), &PHLWINDOW::get);
		    it != g_pCompositor->m_windows.end()) {
			initial_z_orders.push_back(std::distance(g_pCompositor->m_windows.begin(), it));
		} else {
			throw std::runtime_error(
			    std::format(
			        "[wm] WindowSwitcher::seed: window {} not found in "
			        "g_pCompositor->m_windows",
			        as_str(window)
			    )
			);
		}
	}

	// raise all windows of this app
	auto argsort_idxs = std::views::iota(0, static_cast<int>(initial_z_orders.size()))
	                    | std::ranges::to<std::vector>();
	std::ranges::sort(argsort_idxs, [&](int a, int b) {
		return initial_z_orders[a] < initial_z_orders[b];
	});
	for (auto idx : argsort_idxs)
		g_pCompositor->changeWindowZOrder(g_pCompositor->m_windows[idx], true);
}

void WindowSwitcher::focus_selected()
{
	active                   = false;
	g_pCompositor->m_windows = initial_windows;

	[[likely]]
	if (auto window = app_windows[idx].lock()) {
		log(INFO, "    switching to {}", as_str(window));
		g_pCompositor->focusWindow(window);
		g_pCompositor->changeWindowZOrder(window, true);
		if (auto monitor = g_pCompositor->m_lastMonitor.lock())
			g_pHyprRenderer->damageMonitor(monitor);
		// else not our concern
		return;
	}

	throw std::runtime_error(
	    std::format("[wm] window switcher: {} became null", as_str(app_windows[idx]))
	);
}

void WindowSwitcher::abort()
{
	active                   = false;
	g_pCompositor->m_windows = initial_windows;
	if (auto monitor = g_pCompositor->m_lastMonitor.lock())
		g_pHyprRenderer->damageMonitor(monitor);
	// else not our concern
}

bool WindowSwitcher::is_active() { return active; }
