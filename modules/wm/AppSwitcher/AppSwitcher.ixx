module;

#include <wayland-server-core.h>

export module wm.AppSwitcher;

import std;
import llvm.Support;
import hyprland.desktop;
import hyprland.render;
import hyprland.helpers;
import hyprland.config;
import hyprutils.math;
import hyprutils.memory;
import absl;

import wm.AppInfoLoader;

using Config::Values::CColorValue;
using Config::Values::CStringValue;
using Config::Values::CIntValue;
using Config::Values::CFloatValue;
using Hyprutils::Math::CBox;
using Hyprutils::Math::CRegion;
using Hyprutils::Memory::CSharedPointer;
using Hyprutils::Memory::CUniquePointer;

export namespace wm {

struct IconPending {};

struct AppStuff {
	llvm::SmallVector<PHLWINDOWREF>                                             windows;
	std::string_view                                                            app_name;
	// shared pointer is used because Hyprland "needs" it
	std::variant<std::monostate, IconPending, CSharedPointer<Render::ITexture>> icon_texture;
};

struct [[gnu::visibility("hidden")]] AppSwitcherConfig {
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
	CSharedPointer<CStringValue> icon_theme;

	AppSwitcherConfig(void *handle);
};

class [[gnu::visibility("hidden")]] AppSwitcher {
public: // TODO: clean (zero-overhead) abstractions without duplicating methods
	AppInfoLoader app_info_loader;

private:
	// shared pointer is used because Hyprland "needs" it
	absl::flat_hash_map<
	    const char *,
	    std::variant<std::monostate, std::future<Image>, CSharedPointer<Render::ITexture>>>
	                                                   icon_texture_cache;
	std::vector<const char *>                         *app_id_focus_history;
	absl::flat_hash_map<const char *, AppStuff>       *app_stuff_map;
	std::chrono::time_point<std::chrono::system_clock> first_tab_press;
	wl_event_source                                   *timer;
	bool                                               active;
	bool                                               visible;

public:
	bool dirty;

private:
	int      idx;
	uint32_t max_entries;

	int         container_radius;
	int         selection_radius;
	CHyprColor  container_background_color;
	CHyprColor  selection_background_color;
	CHyprColor  container_border_color;
	double      container_border_width;
	double      container_padding;
	double      selection_padding;
	double      icon_size;
	double      icon_sep;
	double      label_sep;
	std::string font_family;
	CHyprColor  font_color;
	double      font_height;
	int         font_size;

	AppSwitcherConfig config;

public:
	explicit AppSwitcher(const AppSwitcherConfig &config);

	void reset_config();

	void activate(
	    std::vector<const char *>                   *app_id_focus_history,
	    absl::flat_hash_map<const char *, AppStuff> *app_stuff_map
	);
	[[nodiscard]] bool is_active() const;
	void               highlight_next(bool backwards);
	void               focus_selected();
	void               deactivate();
	void               on_close_app(const char *closing_app_id);
	std::variant<std::monostate, IconPending, CSharedPointer<Render::ITexture>>
	     load_app_icon(const char *app_id);
	void prune_cache(std::span<const char *> app_ids_to_keep);

private:
	void                                              load_config();
	void                                              load_icon_textures();
	[[nodiscard]] std::expected<CBox, std::monostate> get_container_box() const;
	[[gnu::hot]] void                                 render();

	friend class AppSwitcherPassElement;
};

// Credit: https://github.com/yz778/hyprview
class [[gnu::visibility("hidden")]] AppSwitcherPassElement final : public IPassElement {
public:
	explicit AppSwitcherPassElement(AppSwitcher *instance);
	~AppSwitcherPassElement() override = default;

	[[gnu::hot]] std::vector<CUniquePointer<IPassElement>> draw() override;
	bool                                                   needsLiveBlur() override;
	bool                                                   needsPrecomputeBlur() override;
	std::optional<CBox>                                    boundingBox() override;
	CRegion                                                opaqueRegion() override;

	static constexpr const char *pass_name = "AppSwitcherPassElement";

	const char      *passName() override { return pass_name; }
	ePassElementType type() override { return EK_CUSTOM; }

private:
	AppSwitcher *instance;
};

} // namespace wm
