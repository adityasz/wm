/**
 * `Actions::closeWindow` hook is supposed to close all windows of the app
 * highlighted in the app switcher. In a previous version of Hyprland, close
 * events were not emitted when I wrote code to close windows the way they are
 * supposed to be closed, and therefore I have no reason to do it until the
 * relevant parts in Hyprland codebase get cleaned up.
 *
 * Hyprland lacks an always on top toggle (something like
 * `CWindow::m_alwaysOnTop`, which is different from `CWindow::m_pinned`).
 * Instead, it hardcodes floating windows to render on top of tiled windows,
 * which is strictly worse: (1) A setting like `general.floating_always_on_top =
 * true` can be used to set `CWindow::m_alwaysOnTop` to true for all floating
 * windows at launch to get the existing behavior, and (2) it is trivial to
 * find a use case where a floating window rendering behind a tiled window is
 * objectively the best solution. I don't see a reason to implement
 * `CWindow::m_alwaysOnTop` like functionality in this plugin since Hyprland
 * code is ugly (and therefore unstable), and I want as little maintenance as
 * possible. So, `CWindow::m_pinned` serves as a close enough (but semantically
 * different) workaround for when I want a window above all others.
 *
 * Functions that render windows and handle mouse input are hooked so that
 * windows render the way they are supposed to render. The hooks are untested
 * outside of my use cases (which include zero X11 windows among other quirks).
 */

module hooks;

import std;

import llvm.Support;

import hyprland.config;
import hyprland.desktop;
import hyprland.event;
import hyprland.globals;
import hyprland.layout;
import hyprland.plugins;
import hyprland.protocols;
import hyprland.render;
import hyprland.xwayland;
import hyprutils.math;

import globals;

using Config::Actions::ActionResult;

static bool shud_i_render_tha_windo(
    Render::IHyprRenderer *thisptr,
    const PHLWINDOW       &w,
    const PHLWORKSPACE    &pWorkspace,
    const PHLMONITOR      &pMonitor
)
{
	if (w->isHidden() || (!w->m_isMapped && !w->m_fadingOut))
		return false;

	if (w->m_pinned)
		return false;

	// some things may force us to ignore the special/not special disparity
	const bool IGNORE_SPECIAL_CHECK = w->m_monitorMovedFrom != -1
	                                  && (w->m_workspace && !w->m_workspace->isVisible());

	if (!IGNORE_SPECIAL_CHECK && pWorkspace->m_isSpecialWorkspace != w->onSpecialWorkspace())
		return false;

	if (w->m_isFloating
	    && pWorkspace->m_isSpecialWorkspace
	    && w->m_monitor != pWorkspace->m_monitor) {
		return false; // special on another are rendered as a part of the base pass
	}

	if (!thisptr->shouldRenderWindow(w, pMonitor))
		return false;

	return true;
}

namespace hooks {

namespace Actions_closeWindow {

static CFunctionHook *hook = nullptr;

ActionResult fn(std::optional<PHLWINDOW> w)
{
	// TODO: If I just close all windows of the currently highlighted app in the
	// app switcher in a for loop, close events are not emitted.
	if (window_manager->is_app_switcher_active()) {
		return Config::Actions::actionError(
		    "AppSwitcher active; ignoring closeWindow",
		    Config::Actions::eActionErrorLevel::INFO,
		    Config::Actions::eActionErrorCode::EXECUTION_FAILED
		);
	}

	using src_type = ActionResult (*)(std::optional<PHLWINDOW>);
	return (reinterpret_cast<src_type>(hook->m_original))(w);
}

} // namespace Actions_closeWindow

namespace IHyprRenderer_renderWorkspaceWindows {

using namespace Render;

static CFunctionHook *hook = nullptr;

void fn(
    IHyprRenderer         *thisptr,
    PHLMONITOR             pMonitor,
    PHLWORKSPACE           pWorkspace,
    const Time::steady_tp &time
)
{
	Event::bus()->m_events.render.stage.emit(RENDER_PRE_WINDOWS);

	llvm::SmallVector<PHLWINDOWREF, 64> fading_out;

	for (const auto &w : g_pCompositor->m_windows) {
		if (!shud_i_render_tha_windo(thisptr, w, pWorkspace, pMonitor))
			continue;

		if (w->m_fadingOut)
			fading_out.emplace_back(w);
		else
			thisptr->renderWindow(w, pMonitor, time, true, RENDER_PASS_ALL);
	}

	// render fading out windows above others
	for (const auto &w : fading_out)
		thisptr->renderWindow(w.lock(), pMonitor, time, true, RENDER_PASS_MAIN);
}

} // namespace IHyprRenderer_renderWorkspaceWindows

namespace IHyprRenderer_renderWorkspaceWindowsFullscreen {

using namespace Render;
using namespace Desktop::View;

static CFunctionHook *hook = nullptr;

// The original function was ..., or as Trump would say:
// > It did a lot of redundant checks, it did a lot of redundant passes.
//
// The following treats CCompositor::m_windows to be in z-order. Any window
// below a fullscreen window is rendered only if it would be visible, and all
// windows above a fullscreen window are always rendered.
//
// There may be ways to prevent windows below fullscreen windows from being
// rendered, I don't care enough to respect them.
void fn(
    IHyprRenderer         *thisptr,
    PHLMONITOR             pMonitor,
    PHLWORKSPACE           pWorkspace,
    const Time::steady_tp &time
)
{
	auto it = std::ranges::find_if(g_pCompositor->m_windows, [&](const auto &w) {
		return w->m_workspace == pWorkspace
		       && w->isFullscreen()
		       && shud_i_render_tha_windo(thisptr, w, pWorkspace, pMonitor);
	});

	if (it == g_pCompositor->m_windows.end()) [[unlikely]] {
		// does happen in the original code; side-effect of terrible code quality
		// may be likely, who knows
		return thisptr->renderWorkspaceWindows(pMonitor, pWorkspace, time);
	}

	llvm::SmallVector<PHLWINDOWREF, 64> fading_out;

	auto fullscreen_idx = std::distance(g_pCompositor->m_windows.begin(), it);

	if ((*it)->effectiveAlpha() < 1.0f) {
		// windows below fullscreen window will be visible
		for (const auto &w : g_pCompositor->m_windows | std::views::take(fullscreen_idx)) {
			if (!shud_i_render_tha_windo(thisptr, w, pWorkspace, pMonitor))
				continue;
			if (w->m_fadingOut)
				fading_out.emplace_back(w);
			else
				thisptr->renderWindow(w, pMonitor, time, true, RENDER_PASS_ALL);
		}
	} else {
		// fullscreen window is opaque
		for (const auto &w : g_pCompositor->m_windows | std::views::take(fullscreen_idx)) {
			if (!shud_i_render_tha_windo(thisptr, w, pWorkspace, pMonitor))
				continue;
			if (w->m_fadingOut) {
				fading_out.emplace_back(w);
			} else if (w->m_workspace != pWorkspace) {
				// when switching from one workspace to another, windows on
				// other workspaces still need to be rendered
				thisptr->renderWindow(w, pMonitor, time, true, RENDER_PASS_ALL);
			}
		}
	}

	thisptr->renderWindow(*it, pMonitor, time, true, RENDER_PASS_ALL);

	for (const auto &w : g_pCompositor->m_windows | std::views::drop(fullscreen_idx + 1)) {
		if (!shud_i_render_tha_windo(thisptr, w, pWorkspace, pMonitor))
			continue;
		if (w->m_fadingOut)
			fading_out.emplace_back(w);
		else
			thisptr->renderWindow(w, pMonitor, time, true, RENDER_PASS_ALL);
	}

	for (const auto &w : fading_out)
		thisptr->renderWindow(w.lock(), pMonitor, time, true, RENDER_PASS_MAIN);
}

} // namespace IHyprRenderer_renderWorkspaceWindowsFullscreen

namespace CCompositor_vectorToWindowUnified {

using std::uint16_t;
using namespace Hyprutils::Math;

static CFunctionHook *hook = nullptr;

using src_type = PHLWINDOW (*)(const Vector2D &pos, uint16_t properties, PHLWINDOW pIgnoreWindow);

PHLWINDOW
fn(CCompositor *thisptr, const Vector2D &pos, uint16_t properties, PHLWINDOW pIgnoreWindow)
// clang-format off
{
    const auto PMONITOR = thisptr->getMonitorFromVector(pos);
    if (!PMONITOR)
        return nullptr;

    static auto PRESIZEONBORDER      = CConfigValue<Config::INTEGER>("general:resize_on_border");
    static auto PBORDERSIZE          = CConfigValue<Config::INTEGER>("general:border_size");
    static auto PBORDERGRABEXTEND    = CConfigValue<Config::INTEGER>("general:extend_border_grab_area");
    static auto PSPECIALFALLTHRU     = CConfigValue<Config::INTEGER>("input:special_fallthrough");
    static auto PMODALPARENTBLOCKING = CConfigValue<Config::INTEGER>("general:modal_parent_blocking");
    static auto PFOLLOWMOUSESHRINK   = CConfigValue<Config::INTEGER>("input:follow_mouse_shrink");
    const auto  BORDER_GRAB_AREA     = *PRESIZEONBORDER ? *PBORDERSIZE + *PBORDERGRABEXTEND : 0;
    const bool  ONLY_PRIORITY        = properties & Desktop::View::FOCUS_PRIORITY;
    const bool  FOLLOW_MOUSE_CHECK   = properties & Desktop::View::FOLLOW_MOUSE_CHECK;
    const auto  HITBOX_SHRINK        = FOLLOW_MOUSE_CHECK ? *PFOLLOWMOUSESHRINK : 0;
    const auto  LASTFOCUSED          = Desktop::focusState()->window();

    const auto  isShadowedByModal = [](PHLWINDOW w) -> bool {
        return *PMODALPARENTBLOCKING && w->m_xdgSurface && w->m_xdgSurface->m_toplevel && w->m_xdgSurface->m_toplevel->anyChildModal();
    };

    // pinned windows on top of floating regardless
    if (properties & Desktop::View::ALLOW_FLOATING) {
        for (auto const& w : thisptr->m_windows | std::views::reverse) {
            if (ONLY_PRIORITY && !w->priorityFocus())
                continue;

            if (w->m_pinned && w->m_isMapped && w->acceptsInput() && !w->m_X11ShouldntFocus && !w->m_ruleApplicator->noFocus().valueOrDefault() &&
                w != pIgnoreWindow && !isShadowedByModal(w)) {
                const auto BB  = w->getWindowBoxUnified(properties);
                CBox       box = BB.copy().expand(!w->isX11OverrideRedirect() ? BORDER_GRAB_AREA : 0);
                if (HITBOX_SHRINK > 0 && w != LASTFOCUSED)
                    box = box.copy().expand(-HITBOX_SHRINK);
                if (box.containsPoint(pos))
                    return w;

                if (!w->m_isX11) {
                    if (w->hasPopupAt(pos))
                        return w;
                }
            }
        }
    }

    auto windowForWorkspace = [&](bool special) -> PHLWINDOW {
        const WORKSPACEID WSPID      = special ? PMONITOR->activeSpecialWorkspaceID() : PMONITOR->activeWorkspaceID();
        const auto        PWORKSPACE = thisptr->getWorkspaceByID(WSPID);

        // for windows, we need to check their extensions too, first.
        for (auto const& w : thisptr->m_windows | std::views::reverse) {
            if (ONLY_PRIORITY && !w->priorityFocus())
                continue;

            if (special != w->onSpecialWorkspace())
                continue;

            if (!w->m_workspace)
                continue;

            if (!w->m_isX11 && w->m_isMapped && w->workspaceID() == WSPID && w->acceptsInput() && !w->m_X11ShouldntFocus &&
                !w->m_ruleApplicator->noFocus().valueOrDefault() && w != pIgnoreWindow && !isShadowedByModal(w)) {
                if (w->hasPopupAt(pos))
                    return w;
            }
        }

        for (auto const& w : thisptr->m_windows | std::views::reverse) {
            if (ONLY_PRIORITY && !w->priorityFocus())
                continue;

            if (special != w->onSpecialWorkspace())
                continue;

            if (!w->m_workspace)
                continue;

            if (w->m_isMapped && w->workspaceID() == WSPID && w->acceptsInput() && !w->m_X11ShouldntFocus && !w->m_ruleApplicator->noFocus().valueOrDefault() &&
                w != pIgnoreWindow && !isShadowedByModal(w)) {
                const bool isFullscreen = PWORKSPACE->m_hasFullscreenWindow && PWORKSPACE->getFullscreenWindow() == w;
                CBox box = (w->m_isFloating || isFullscreen || (properties & Desktop::View::USE_PROP_TILED))
                            ? w->getWindowBoxUnified(properties)
                            : CBox{w->m_position, w->m_size};
                if ((properties & Desktop::View::INPUT_EXTENTS) && BORDER_GRAB_AREA > 0 && !w->isX11OverrideRedirect()) {
                    const auto WORKAREA                    = PWORKSPACE->m_space->workArea();
                    auto       isWindowCloseToWorkAreaEdge = [&](const Math::eDirection dir) -> bool {
                        constexpr double STICK_THRESHOLD = 2.0; // This constant is taken from isAdjacent in CCompositor::getWindowInDirection
                        double           aEdge           = -1;
                        double           bEdge           = -1;

                        switch (dir) {
                            case Math::DIRECTION_LEFT:
                                aEdge = WORKAREA.x;
                                bEdge = box.x;
                                break;
                            case Math::DIRECTION_RIGHT:
                                aEdge = WORKAREA.x + WORKAREA.width;
                                bEdge = box.x + box.width;
                                break;
                            case Math::DIRECTION_UP:
                                aEdge = WORKAREA.y;
                                bEdge = box.y;
                                break;
                            case Math::DIRECTION_DOWN:
                                aEdge = WORKAREA.y + WORKAREA.height;
                                bEdge = box.y + box.height;
                                break;
                            default: break;
                        }
                        const double delta = aEdge - bEdge;
                        return std::abs(delta) < STICK_THRESHOLD;
                    };

                    if (isWindowCloseToWorkAreaEdge(Math::eDirection::DIRECTION_LEFT)) {
                        box.x -= BORDER_GRAB_AREA;
                        box.width += BORDER_GRAB_AREA;
                    }

                    if (isWindowCloseToWorkAreaEdge(Math::eDirection::DIRECTION_RIGHT))
                        box.width += BORDER_GRAB_AREA;

                    if (isWindowCloseToWorkAreaEdge(Math::eDirection::DIRECTION_UP)) {
                        box.y -= BORDER_GRAB_AREA;
                        box.height += BORDER_GRAB_AREA;
                    }

                    if (isWindowCloseToWorkAreaEdge(Math::eDirection::DIRECTION_DOWN))
                        box.height += BORDER_GRAB_AREA;
                }
                if (HITBOX_SHRINK > 0 && w != LASTFOCUSED)
                    box = box.copy().expand(-HITBOX_SHRINK);

                if (box.containsPoint(pos)) {
                    // Gemini says this is important I don't have X11 apps to verify
                    if (w->m_isX11 && w->isX11OverrideRedirect() && !w->m_xwaylandSurface->wantsFocus()) {
                        return Desktop::focusState()->window();
                    }
                    return w;
                }
            }
        }

        return nullptr;
    };

    // special workspace
    if (PMONITOR->m_activeSpecialWorkspace && !*PSPECIALFALLTHRU)
        return windowForWorkspace(true);

    if (PMONITOR->m_activeSpecialWorkspace) {
        const auto PWINDOW = windowForWorkspace(true);

        if (PWINDOW)
            return PWINDOW;
    }

    return windowForWorkspace(false);
}
// clang-format on

} // namespace CCompositor_vectorToWindowUnified

} // namespace hooks

bool register_hooks(void *handle)
{
	bool success = true;

	hooks::Actions_closeWindow::hook = HyprlandAPI::createFunctionHook(
	    handle,
	    HyprlandAPI::findFunctionsByName(handle, "closeWindow").front().address,
	    reinterpret_cast<void *>(&hooks::Actions_closeWindow::fn)
	);
	success &= hooks::Actions_closeWindow::hook->hook();

	hooks::IHyprRenderer_renderWorkspaceWindows::hook = HyprlandAPI::createFunctionHook(
	    handle,
	    HyprlandAPI::findFunctionsByName(handle, "renderWorkspaceWindows").front().address,
	    reinterpret_cast<void *>(&hooks::IHyprRenderer_renderWorkspaceWindows::fn)
	);
	success &= hooks::IHyprRenderer_renderWorkspaceWindows::hook->hook();

	hooks::IHyprRenderer_renderWorkspaceWindowsFullscreen::hook = HyprlandAPI::createFunctionHook(
	    handle,
	    HyprlandAPI::findFunctionsByName(handle, "renderWorkspaceWindowsFullscreen")
	        .front()
	        .address,
	    reinterpret_cast<void *>(&hooks::IHyprRenderer_renderWorkspaceWindowsFullscreen::fn)
	);
	success &= hooks::IHyprRenderer_renderWorkspaceWindowsFullscreen::hook->hook();

	hooks::CCompositor_vectorToWindowUnified::hook = HyprlandAPI::createFunctionHook(
	    handle,
	    HyprlandAPI::findFunctionsByName(handle, "vectorToWindowUnified").front().address,
	    reinterpret_cast<void *>(&hooks::CCompositor_vectorToWindowUnified::fn)
	);
	success &= hooks::CCompositor_vectorToWindowUnified::hook->hook();

	return success;
}
