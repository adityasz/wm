module wm.AppSwitcher;

import std;
import hyprland.render;

using namespace wm;

// Lifted from https://github.com/yz778/hyprview.

AppSwitcherPassElement::AppSwitcherPassElement(AppSwitcher *instance) : instance(instance) {}

std::vector<CUniquePointer<IPassElement>> AppSwitcherPassElement::draw()
{
	instance->render();
	return {};
}

bool AppSwitcherPassElement::needsLiveBlur() { return false; }

bool AppSwitcherPassElement::needsPrecomputeBlur() { return true; }

std::optional<CBox> AppSwitcherPassElement::boundingBox() { return std::nullopt; }

CRegion AppSwitcherPassElement::opaqueRegion() { return {}; }
