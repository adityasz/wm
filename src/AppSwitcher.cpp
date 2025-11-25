#include <expected>
#include <variant>

#include "AppSwitcher.h"
#include "Globals.h"
#include "Logging.h"

using namespace std::chrono_literals;

AppSwitcher::AppSwitcher() : app_stuff_map(nullptr), idx(0), render_hook(nullptr)
{
	active = false;
	reload_config();
}

bool AppSwitcher::is_active() const { return active; }

void AppSwitcher::move(bool backwards)
{
	LOG_TRACE("backwards = {}, idx = {}", backwards, idx);

	if (app_id_focus_history.empty())
		return;

	if (backwards) {
		if (idx)
			idx--;
		else
			idx = app_id_focus_history.size() - 1;
	} else {
		idx++;
		if (idx == app_id_focus_history.size())
			idx = 0;
	}

	if (auto monitor = g_pCompositor->m_lastMonitor.lock())
		g_pHyprRenderer->damageMonitor(monitor);
}

void AppSwitcher::focus_selected()
{
	LOG_TRACE("{}", "");
	hide();
	if (app_id_focus_history.empty())
		return;
	auto &windows    = app_stuff_map->at(app_id_focus_history[idx]).windows;
	auto  window_ref = windows.front();
	if (auto window = window_ref.lock()) {
		focus_and_raise_window(window);
	} else {
		gch::erase(windows, window_ref);
		log(INFO, "    {} became null", as_str(window_ref));
	}
}

void AppSwitcher::show(
    std::span<std::string>                     app_id_focus_history,
    std::unordered_map<std::string, AppStuff> *app_stuff_map
)
{
	active = true;

	first_tab_press = std::chrono::system_clock::now();

	timer = wl_event_loop_add_timer(
	    g_pCompositor->m_wlEventLoop,
	    [](void *data) {
		    auto *self = static_cast<AppSwitcher *>(data);
		    if (auto res = self->get_container_box(); res.has_value())
			    g_pHyprRenderer->damageRegion(res.value());
		    return 0;
	    },
	    this
	);
	wl_event_source_timer_update(timer, 100);

	log(INFO, "show: {}", app_id_focus_history);
	this->idx                  = 0;
	this->app_id_focus_history = app_id_focus_history;
	this->app_stuff_map        = app_stuff_map;

	// Damage the monitor to trigger rendering
	if (auto monitor = g_pCompositor->m_lastMonitor.lock())
		g_pHyprRenderer->damageMonitor(monitor);
}

void AppSwitcher::hide()
{
	LOG_TRACE("{}", "");
	if (visible) {
		wl_event_source_remove(timer);
		timer = nullptr;
	}
	active  = false;
	visible = false;
}

std::expected<CBox, std::monostate> AppSwitcher::get_container_box() const
{
	size_t num_icons = app_id_focus_history.size();
	double total_width =
	    container_padding * 2 + icon_size * num_icons + icon_sep * (num_icons - 1);
	double total_height =
	    container_padding + icon_size + label_sep + font_height + container_padding;
	auto monitor = g_pCompositor->m_lastMonitor.lock();
	if (!monitor) {
		log(INFO, "monitor {} is null", as_str(g_pCompositor->m_lastMonitor));
		return std::unexpected{std::monostate{}};
	}
	auto center = monitor->m_size * monitor->m_scale / 2;
	return CBox{
	    center.x - total_width / 2.0, center.y - total_height / 2.0, total_width, total_height
	};
}

void AppSwitcher::render()
{
	LOG_TRACE("{}", "");

	if (auto monitor = g_pCompositor->m_lastMonitor.lock(); !monitor) {
		log(INFO, "monitor {} is null", as_str(g_pCompositor->m_lastMonitor));
		return;
	}

	if (app_id_focus_history.empty()) {
		log(INFO, "render: no apps open");
		return;
	}

	if (!visible && std::chrono::system_clock::now() - first_tab_press < 100ms)
		return;
	visible = true;

	CBox container_box;
	if (auto res = get_container_box(); res.has_value())
		container_box = res.value();
	else
		return;

	// TODO: shadow

	// Draw container border if width > 0
	CBox border_box = {
	    container_box.x - container_border_width,
	    container_box.y - container_border_width,
	    container_box.w + 2 * container_border_width,
	    container_box.h + 2 * container_border_width
	};
	if (container_border_width > 0) {
		CHyprOpenGLImpl::SRectRenderData border_data;
		border_data.round = container_radius;
		border_data.blur  = true;
		g_pHyprOpenGL->renderRect(border_box, container_border_color, border_data);
	}

	// Draw container background
	CHyprOpenGLImpl::SRectRenderData container_data;
	container_data.round = container_radius;
	container_data.blur  = true;
	g_pHyprOpenGL->renderRect(container_box, container_background_color, container_data);

	double icon_x   = container_box.x + container_padding;
	// TODO: use multiple rows when too many icons
	double icon_y   = container_box.y + container_padding;
	CBox   icon_box = {
        icon_x, icon_y, static_cast<double>(icon_size), static_cast<double>(icon_size)
    };
	CBox text_box = {
	    icon_x + icon_size / 2.0,
	    icon_y + icon_size + label_sep,
	    static_cast<double>(icon_size),
	    static_cast<double>(font_height)
	};
	for (const auto &[i, app_id] : app_id_focus_history | std::views::enumerate) {
		// Draw selection highlight if this is the selected app
		if (static_cast<size_t>(i) == idx) {
			CBox selection_box = {
			    icon_x - selection_padding,
			    icon_y - selection_padding,
			    static_cast<double>(icon_size) + 2 * selection_padding,
			    static_cast<double>(icon_size + label_sep + font_height) + 2 * selection_padding
			};

			CHyprOpenGLImpl::SRectRenderData selection_data;
			selection_data.round = selection_radius;
			selection_data.blur  = true;
			g_pHyprOpenGL->renderRect(selection_box, selection_background_color, selection_data);
		}

		auto &[_, app_stuff] = *app_stuff_map->find(app_id);
		auto data_ptr        = std::get_if<AppRenderData>(&app_stuff.app_info);
		if (!data_ptr) {
			log(INFO, "AppSwitcher: data not available for class={}", app_id);
			icon_x     += icon_size + icon_sep;
			text_box.x += icon_size + icon_sep;
			continue;
		}
		auto &[app_name, icon_texture] = *data_ptr;

		if (icon_texture && icon_texture->m_texID) {
			CRegion damage = icon_box;
			g_pHyprOpenGL->renderTextureInternal(
			    icon_texture,
			    icon_box,
			    {.damage = &damage, .a = 1.0, .round = 0, .cmBackToSRGBSource = nullptr}
			);
		} else {
			log(INFO, "AppSwitcher: icon not available for {}", app_name);
		}

		icon_box.x += icon_size + icon_sep;

		// Draw label
		if (auto text_texture =
		        g_pHyprOpenGL->renderText(app_name, font_color, font_size, false, font_family)) {
			double text_width  = text_texture->m_size.x;
			text_box.x        -= text_width / 2.0;
			text_box.w         = text_width;
			CRegion damage     = text_box;
			g_pHyprOpenGL->renderTextureInternal(
			    text_texture,
			    text_box,
			    {.damage = &damage, .a = 1.0, .round = 0, .cmBackToSRGBSource = nullptr}
			);
			text_box.x += text_width / 2.0;
		}

		text_box.x += icon_size + icon_sep;
		icon_x     += icon_size + icon_sep;
	}

	// TODO: damage shadow box
	if (container_border_width > 0)
		g_pHyprRenderer->damageBox(border_box);
	else
		g_pHyprRenderer->damageBox(container_box);
}

void AppSwitcher::reload_config()
{
	LOG_TRACE("{}", "AppSwitcher: reloading config");

	container_background_color =
	    CHyprColor(**get_config<Hyprlang::INT>("app_switcher:container:background_color"));
	container_border_color =
	    CHyprColor(**get_config<Hyprlang::INT>("app_switcher:container:border_color"));
	container_padding      = **get_config<Hyprlang::INT>("app_switcher:container:padding");
	container_radius       = **get_config<Hyprlang::INT>("app_switcher:container:radius");
	container_border_width = **get_config<Hyprlang::INT>("app_switcher:container:border_width");

	selection_background_color =
	    CHyprColor(**get_config<Hyprlang::INT>("app_switcher:selection:background_color"));
	selection_padding = **get_config<Hyprlang::INT>("app_switcher:selection:padding");
	selection_radius  = **get_config<Hyprlang::INT>("app_switcher:selection:radius");

	font_family = *get_config<char>("app_switcher:label:font_family");
	font_color  = CHyprColor(**get_config<Hyprlang::INT>("app_switcher:label:font_color"));
	font_size   = **get_config<Hyprlang::INT>("app_switcher:label:font_size");
	label_sep   = **get_config<Hyprlang::INT>("app_switcher:label:separation");

	icon_size = **get_config<Hyprlang::INT>("app_switcher:icons:size");
	icon_sep  = **get_config<Hyprlang::INT>("app_switcher:icons:separation");

	// TODO: this is ugly, figure out the right way to do this
	font_height = font_size
	                  ? g_pHyprOpenGL
	                        ->renderText("X", font_color, font_size, false, font_family.c_str())
	                        ->m_size.y
	                  : 0;
}
