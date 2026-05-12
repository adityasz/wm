module;

#include "Hyprland.h"

module wm.AppSwitcher;

using namespace wm;

// Lifted from https://github.com/yz778/hyprview.

AppSwitcherPassElement::AppSwitcherPassElement(AppSwitcher *instance) : instance(instance) {}

void AppSwitcherPassElement::draw(const CRegion &) { instance->render(); }

bool AppSwitcherPassElement::needsLiveBlur() { return false; }

bool AppSwitcherPassElement::needsPrecomputeBlur() { return true; }

std::optional<CBox> AppSwitcherPassElement::boundingBox() { return std::nullopt; }

CRegion AppSwitcherPassElement::opaqueRegion() { return {}; }
