#include "AppSwitcherPassElement.h"
#include "AppSwitcher.h"

// Lifted from https://github.com/yz778/hyprview.
//
// I may be using these incorrectly (haven't looked at Hyprland's rendering pipeline)

AppSwitcherPassElement::AppSwitcherPassElement(AppSwitcher *instance) : instance(instance) {}

void AppSwitcherPassElement::draw(const CRegion &) { instance->render(); }

bool AppSwitcherPassElement::needsLiveBlur() { return false; }

bool AppSwitcherPassElement::needsPrecomputeBlur() { return true; }

std::optional<CBox> AppSwitcherPassElement::boundingBox() { return std::nullopt; }

CRegion AppSwitcherPassElement::opaqueRegion() { return {}; }
