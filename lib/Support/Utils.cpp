module wm.Support.Utils;

import std;
import hyprland.desktop;
import hyprland.globals;
import hyprland.managers;
import hyprutils.memory;

namespace wm {
void focus_and_raise_window(
    const PHLWINDOW &window, const CSharedPointer<CWLSurfaceResource> &pSurface, [[maybe_unused]] bool preserveFocusHistory
)
{
	if (auto last_monitor = Desktop::focusState()->monitor();
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
	Desktop::focusState()->fullWindowFocus(window, Desktop::FOCUS_REASON_OTHER, pSurface, false);
	if (window->m_group)
		window->m_group->setCurrent(window);
	g_pCompositor->changeWindowZOrder(window, true);
	g_pInputManager->simulateMouseMovement();
}
} // namespace wm
