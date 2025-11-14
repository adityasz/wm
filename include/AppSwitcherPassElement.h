#pragma once

#include "Hyprland.h"

class AppSwitcher;

// Credit: https://github.com/yz778/hyprview
class AppSwitcherPassElement final : public IPassElement {
public:
	explicit AppSwitcherPassElement(AppSwitcher *instance);
	~AppSwitcherPassElement() override = default;

	void                draw(const CRegion &damage) override;
	bool                needsLiveBlur() override;
	bool                needsPrecomputeBlur() override;
	std::optional<CBox> boundingBox() override;
	CRegion             opaqueRegion() override;

	const char *passName() override { return "AppSwitcherPassElement"; }

private:
	AppSwitcher *instance;
};
