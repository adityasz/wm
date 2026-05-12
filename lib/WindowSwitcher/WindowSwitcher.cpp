#include <algorithm>
#include <cassert>
#include <ranges>

#include "Support/Logging.h"
#include "Support/Utils.h"
#include "WindowSwitcher/WindowSwitcher.h"

WindowSwitcher::WindowSwitcher() : app_windows(nullptr), idx(0), active(false) {}

void WindowSwitcher::seed(llvm::SmallVectorImpl<PHLWINDOWREF> *app_windows)
{
	LOG_TRACE("{}", *app_windows | std::views::transform([](auto &window) {
		          return as_str(window);
	          }));

	idx    = 0;
	active = true;

	this->app_windows = app_windows;

	initial_windows = g_pCompositor->m_windows;

	std::vector<size_t> initial_z_orders;
	initial_z_orders.reserve(app_windows->size());
	for (auto &window : *app_windows) {
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

void WindowSwitcher::move(bool backwards)
{
	LOG_TRACE("backwards={}", backwards);

	if (app_windows->empty())
		return;

	if (backwards) {
		if (idx)
			idx--;
		else
			idx = static_cast<int>(app_windows->size()) - 1;
	} else {
		idx++;
		if (idx == static_cast<int>(app_windows->size()))
			idx = 0;
	}

	assert((idx >= 0 && idx < static_cast<int>(app_windows->size())) && "idx out of bounds");

	[[likely]]
	if (auto window = (*app_windows)[idx].lock()) {
		log(INFO, "    switching to {}", as_str(window));
		focus_and_raise_window(window, nullptr, true);
		return;
	}

	throw std::runtime_error(
	    std::format("[wm] WindowSwitcher::move: {} became null", as_str((*app_windows)[idx]))
	);
}

void WindowSwitcher::focus_selected()
{
	assert((idx >= 0 && idx < static_cast<int>(app_windows->size())) && "idx out of bounds");

	active                   = false;
	g_pCompositor->m_windows = initial_windows;

	[[likely]]
	if (auto window = (*app_windows)[idx].lock()) {
		log(INFO, "    switching to {}", as_str(window));
		focus_and_raise_window(window);
		if (auto monitor = g_pCompositor->m_lastMonitor.lock())
			g_pHyprRenderer->damageMonitor(monitor);
		// else not our concern
		return;
	}

	throw std::runtime_error(
	    std::format("[wm] window switcher: {} became null", as_str((*app_windows)[idx]))
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

void WindowSwitcher::on_close_window(const PHLWINDOW &closing_window)
{
	if (!active)
		return;

	std::erase(initial_windows, closing_window);

	// all windows of this app closed
	if (app_windows->size() == 1) {
		active = false;
		return;
	}

	for (const auto &[i, window] : *app_windows | std::views::enumerate) {
		if (window == closing_window) {
			if (idx != 0 && idx >= i)
				idx--;
			break;
		}
	}

	[[likely]]
	if (auto next_window = (*app_windows)[idx].lock()) {
		log(INFO, "    switching to {}", as_str(next_window));
		focus_and_raise_window(next_window, nullptr, true);
	}
}

bool WindowSwitcher::is_active() const { return active; }
