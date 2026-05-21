module;

#include <wayland-server-core.h>

export module wm.AppSwitcher;

import std;
import llvm.ADT;
import hyprland.desktop;
import hyprland.render;
import hyprland.helpers;
import hyprland.config;
import hyprutils.math;
import hyprutils.memory;

export import wm.AppSwitcher.AppInfoLoader;
export import wm.AppSwitcher.Image;

using Config::Values::CColorValue;
using Config::Values::CStringValue;
using Config::Values::CIntValue;
using Config::Values::CFloatValue;
using Hyprutils::Math::CBox;
using Hyprutils::Math::CRegion;
using Hyprutils::Memory::CSharedPointer;
using Hyprutils::Memory::CUniquePointer;

export namespace wm {

struct AppRenderData {
	std::string          app_name;
	CSharedPointer<Render::ITexture> icon_texture;
};

struct AppStuff {
	llvm::SmallVector<PHLWINDOWREF>                     windows;
	std::variant<std::future<AppInfo *>, AppRenderData> app_info;
};

struct AppSwitcherConfig {
	CSharedPointer<CColorValue>  container_background_color;
	CSharedPointer<CColorValue>  container_border_color;
	CSharedPointer<CFloatValue>  container_border_width;
	CSharedPointer<CFloatValue>  container_padding;
	CSharedPointer<CIntValue>    container_radius;
	CSharedPointer<CColorValue>  selection_background_color;
	CSharedPointer<CFloatValue>  selection_padding;
	CSharedPointer<CIntValue>    selection_radius;
	CSharedPointer<CStringValue> font_family;
	CSharedPointer<CColorValue>  font_color;
	CSharedPointer<CIntValue>    font_size;
	CSharedPointer<CFloatValue>  label_sep;
	CSharedPointer<CFloatValue>  icon_size;
	CSharedPointer<CFloatValue>  icon_sep;
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
	int         container_radius;
	int         selection_radius;
	int         font_size;

	int  idx;
	bool visible;
	bool active;

public:
	explicit AppSwitcher(const AppSwitcherConfig &config);

	void reset_config(const AppSwitcherConfig &config);

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

	std::vector<CUniquePointer<IPassElement>> draw() override;
	bool                          needsLiveBlur() override;
	bool                          needsPrecomputeBlur() override;
	std::optional<CBox>           boundingBox() override;
	CRegion                       opaqueRegion() override;

	const char      *passName() override { return "AppSwitcherPassElement"; }
	ePassElementType type() override { return EK_CUSTOM; }

private:
	AppSwitcher *instance;
};

} // namespace wm
