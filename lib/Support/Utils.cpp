module;

#include "Hyprland.h"

module wm.Support.Utils;

namespace wm {
void focus_and_raise_window(
    const PHLWINDOW &window, const SP<CWLSurfaceResource> &pSurface, bool preserveFocusHistory
)
{
	if (auto last_monitor = g_pCompositor->m_lastMonitor.lock();
	    last_monitor
	    && last_monitor->m_activeSpecialWorkspace
	    && window->m_workspace != last_monitor->m_activeSpecialWorkspace) {
		last_monitor->setSpecialWorkspace(nullptr);
	}
	if (window->m_workspace->m_hasFullscreenWindow) {
		if (auto fullscreen_window = window->m_workspace->getFullscreenWindow();
		    !window->m_pinned && fullscreen_window != window) {
			g_pCompositor->setWindowFullscreenInternal(fullscreen_window, FSMODE_NONE);
		}
	}
	g_pCompositor->focusWindow(window, pSurface, preserveFocusHistory);
	if (!window->m_groupData.pNextWindow.expired())
		window->setGroupCurrent(window);
	g_pCompositor->changeWindowZOrder(window, true);
	g_pInputManager->simulateMouseMovement();
}
} // namespace wm
