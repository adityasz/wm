module;

#include <gch/small_vector.hpp>

#include <future>
#include "Hyprland.h"

export module wm.AppSwitcher;

import std;
import wm.Support.AppInfoLoader;

using Hyprlang::SVector2D;

using namespace wm;

namespace wm {
export struct AppRenderData {
	std::string  app_name;
	SP<CTexture> icon_texture;
};

export struct AppStuff {
	gch::small_vector<PHLWINDOWREF, 3>                  windows;
	std::variant<std::future<AppInfo *>, AppRenderData> app_info;
};

export class AppSwitcherPassElement;

/// Once we have a better window switcher, we can have a common base class or
/// something
export class AppSwitcher {
	std::span<std::string>                             app_id_focus_history;
	std::unordered_map<std::string, AppStuff>         *app_stuff_map;
	size_t                                             idx;
	SP<HOOK_CALLBACK_FN>                               render_hook;
	std::chrono::time_point<std::chrono::system_clock> first_tab_press;
	wl_event_source                                   *timer;

	CHyprColor  container_background_color;
	CHyprColor  container_border_color;
	CHyprColor  selection_background_color;
	CHyprColor  font_color;
	std::string font_family;
	int         font_size;
	int         label_sep;
	int         icon_size;
	int         icon_sep;
	int         container_border_width;
	int         container_padding;
	int         selection_padding;
	int         container_radius;
	int         selection_radius;
	float       font_height;

	bool visible;
	bool active;

public:
	AppSwitcher();

	void reload_config();

	void show(
	    std::span<std::string>                     app_id_focus_history,
	    std::unordered_map<std::string, AppStuff> *app_stuff_map
	);
	void               move(bool backwards);
	void               focus_selected();
	[[nodiscard]] bool is_active() const;

private:
	[[nodiscard]] std::expected<CBox, std::monostate> get_container_box() const;
	void                                              render();
	void                                              hide();

	friend class AppSwitcherPassElement;
};

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
} // namespace wm
