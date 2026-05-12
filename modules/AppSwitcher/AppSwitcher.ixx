module;

#include "llvm/ADT/SmallVector.h"

#include "Hyprland.h"

export module wm.AppSwitcher;

import std;
export import wm.AppSwitcher.AppInfoLoader;
export import wm.AppSwitcher.Image;

using Hyprlang::SVector2D;

export namespace wm {
struct AppRenderData {
	std::string  app_name;
	SP<CTexture> icon_texture;
};

struct AppStuff {
	llvm::SmallVector<PHLWINDOWREF>                     windows;
	std::variant<std::future<AppInfo *>, AppRenderData> app_info;
};

class AppSwitcher {
	std::vector<std::string>                          *app_id_focus_history;
	std::unordered_map<std::string, AppStuff>         *app_stuff_map;
	std::chrono::time_point<std::chrono::system_clock> first_tab_press;
	wl_event_source                                   *timer;

	CHyprColor  container_background_color;
	CHyprColor  container_border_color;
	CHyprColor  selection_background_color;
	CHyprColor  font_color;
	std::string font_family;
	double      font_height;
	double      label_sep;
	double      icon_size;
	double      icon_sep;
	double      container_border_width;
	double      container_padding;
	double      selection_padding;
	double      container_radius;
	int         font_size;
	int         selection_radius;

	int  idx;
	bool visible;
	bool active;

public:
	AppSwitcher();

	void reload_config();

	void show(
	    std::vector<std::string>                  *app_id_focus_history,
	    std::unordered_map<std::string, AppStuff> *app_stuff_map
	);
	void                      move(bool backwards);
	void                      abort();
	[[nodiscard]] std::string get_current_selection() const;
	void                      focus_selected();
	void                      on_close_app(std::string_view closing_app_id);
	[[nodiscard]] bool        is_active() const;

private:
	void                                              load_icon_textures() const;
	[[nodiscard]] std::expected<CBox, std::monostate> get_container_box() const;
	void                                              render();
	void                                              hide();

	friend class AppSwitcherPassElement;
};

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
} // namespace wm
