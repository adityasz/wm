#pragma once

#include <gch/small_vector.hpp>

#include "AppInfoLoader.h"
#include "Hyprland.h"

using Hyprlang::SVector2D;

struct AppRenderData {
	std::string  app_name;
	SP<CTexture> icon_texture;
};

struct AppStuff {
	gch::small_vector<PHLWINDOWREF, 3>                  windows;
	std::variant<std::future<AppInfo *>, AppRenderData> app_info;
};

/// Once we have a better window switcher, we can have a common base class or
/// something
class AppSwitcher {
	std::span<std::string>                     app_id_focus_history;
	std::unordered_map<std::string, AppStuff> *app_stuff_map;
	size_t                                     idx;
	SP<HOOK_CALLBACK_FN>                       render_hook;

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

	static bool active;

public:
	AppSwitcher() : app_stuff_map(nullptr), idx(0), render_hook(nullptr)
	{
		active = false;
		reload_config();
	}

	void reload_config();

	static bool is_active();

	void show(
	    std::span<std::string>                     app_id_focus_history,
	    std::unordered_map<std::string, AppStuff> *app_stuff_map
	);
	void move(bool backwards);
	void focus_selected();

private:
	void        render();
	static void hide();

	friend class AppSwitcherPassElement;
};
