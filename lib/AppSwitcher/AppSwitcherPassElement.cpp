module wm.AppSwitcher;

import std;
import hyprland.render;

using namespace wm;

AppSwitcherPassElement::AppSwitcherPassElement(AppSwitcher *instance) : instance(instance) {}

std::vector<CUniquePointer<IPassElement>> AppSwitcherPassElement::draw()
{ return instance->render(); }

bool AppSwitcherPassElement::needsLiveBlur()
{ return !instance->dirty && instance->container_surface.opacity < 1.F; }

bool AppSwitcherPassElement::needsPrecomputeBlur() { return false; }

std::optional<CBox> AppSwitcherPassElement::boundingBox()
{
	auto box = instance->get_container_box();
	if (!box.has_value())
		return std::nullopt;

	const auto &shadow = instance->shadow;
	if (shadow.enabled && shadow.range > 0 && shadow.scale > 0.F && shadow.color.a > 0.0) {
		*box = instance->get_shadow_box(
		    *box, shadow, g_pHyprRenderer->m_renderData.pMonitor->m_scale
		);
	} else {
		box->expand(
		    instance->container_border_width * g_pHyprRenderer->m_renderData.pMonitor->m_scale
		);
	}

	return box->scale(1.F / g_pHyprRenderer->m_renderData.pMonitor->m_scale).round();
}

CRegion AppSwitcherPassElement::opaqueRegion() { return {}; }
