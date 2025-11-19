#include "Hyprland.h"

void focus_and_raise_window(PHLWINDOW window, SP<CWLSurfaceResource> pSurface, bool preserveFocusHistory)
{
	if (window->m_workspace->m_hasFullscreenWindow) {
		if (auto fullscreen_window = window->m_workspace->getFullscreenWindow();
		    fullscreen_window != window) {
			g_pCompositor->setWindowFullscreenInternal(fullscreen_window, FSMODE_NONE);
		}
	}
	g_pCompositor->focusWindow(window, pSurface, preserveFocusHistory);
	if (!window->m_groupData.pNextWindow.expired())
		window->setGroupCurrent(window);
	g_pCompositor->changeWindowZOrder(window, true);
}
