module;

#include <cassert>
#include <GLES3/gl32.h>
#include <wayland-server-core.h>

module wm.AppSwitcher;

import std;
import hyprland.globals;
import hyprland.render;
import wm.Support;

using namespace wm;
using namespace std::chrono_literals;

using std::size_t;
using std::uint64_t;
using Log::INFO;
using Render::GL::g_pHyprOpenGL;
using Render::GL::CHyprOpenGLImpl;

AppSwitcher::AppSwitcher(const AppSwitcherConfig &config) :
    app_id_focus_history(nullptr),
    app_stuff_map(nullptr),
    timer(nullptr),
    idx(0),
    active(false)
{ reset_config(config); }

bool AppSwitcher::is_active() const { return active; }

void AppSwitcher::show(
    std::vector<std::string>                  *app_id_focus_history,
    std::unordered_map<std::string, AppStuff> *app_stuff_map
)
{
	first_tab_press = std::chrono::system_clock::now();

	active = true;

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

	log(INFO, "show: {}", *app_id_focus_history);
	this->idx                  = 0;
	this->app_id_focus_history = app_id_focus_history;
	this->app_stuff_map        = app_stuff_map;

	load_icon_textures();
}

void AppSwitcher::move(bool backwards)
{
	// LOG_TRACE("backwards = {}, idx = {}", backwards, idx);

	if (app_id_focus_history->empty())
		return;

	if (backwards) {
		if (idx)
			idx--;
		else
			idx = static_cast<int>(app_id_focus_history->size()) - 1;
	} else {
		idx++;
		if (idx == static_cast<int>(app_id_focus_history->size()))
			idx = 0;
	}

	assert(
	    (idx >= 0 && idx < static_cast<int>(app_id_focus_history->size())) && "idx out of bounds"
	);

	if (auto monitor = Desktop::focusState()->monitor())
		g_pHyprRenderer->damageMonitor(monitor);
}

std::string AppSwitcher::get_current_selection() const { return (*app_id_focus_history)[idx]; }

void AppSwitcher::focus_selected()
{
	// LOG_TRACE("{}", "");

	assert(
	    (idx >= 0 && idx < static_cast<int>(app_id_focus_history->size())) && "idx out of bounds"
	);

	hide();
	if (app_id_focus_history->empty())
		return;
	auto &windows    = app_stuff_map->at((*app_id_focus_history)[idx]).windows;
	auto  window_ref = windows.front();
	if (auto window = window_ref.lock()) {
		focus_and_raise_window(window);
	} else {
		std::ranges::remove(windows, window_ref);
		log(INFO, "    {} became null", as_str(window_ref));
	}
}

void AppSwitcher::hide()
{
	// LOG_TRACE("{}", "");
	if (visible) {
		wl_event_source_remove(timer);
		timer = nullptr;
	}
	active  = false;
	visible = false;
}

void AppSwitcher::abort() { hide(); }

void AppSwitcher::on_close_app(std::string_view closing_app_id)
{
	if (!active)
		return;

	if (app_id_focus_history->empty() == 1)
		return hide();

	for (const auto &[i, app_id] : *app_id_focus_history | std::views::enumerate) {
		if (app_id == closing_app_id) {
			if (idx != 0 && idx >= i)
				idx--;
			return;
		}
	}
}

std::expected<CBox, std::monostate> AppSwitcher::get_container_box() const
{
	size_t num_icons  = app_id_focus_history->size();
	auto total_width  = container_padding * 2 + icon_size * num_icons + icon_sep * (num_icons - 1);
	auto total_height = 2 * container_padding + icon_size + label_sep + font_height;
	auto monitor      = Desktop::focusState()->monitor();
	if (!monitor) {
		log(INFO, "monitor {} is null", as_str(Desktop::focusState()->monitor()));
		return std::unexpected{std::monostate{}};
	}
	auto center = monitor->m_size * monitor->m_scale / 2;
	return CBox(
	    center.x - total_width / 2.0, center.y - total_height / 2.0, total_width, total_height
	);
}

void AppSwitcher::render()
{
	// LOG_TRACE("{}", "");

	auto monitor = Desktop::focusState()->monitor();
	if (!monitor) {
		log(INFO, "monitor {} is null", as_str(Desktop::focusState()->monitor()));
		return;
	}

	if (app_id_focus_history->empty())
		return;

	if (!visible) {
		if (std::chrono::system_clock::now() - first_tab_press < 100ms)
			return;
		visible = true;
	}

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
		g_pHyprOpenGL->renderRect(
		    border_box,
		    container_border_color,
		    CHyprOpenGLImpl::SRectRenderData{.round = container_radius, .blur = true}
		);
	}

	// Draw container background
	g_pHyprOpenGL->renderRect(
	    container_box,
	    container_background_color,
	    CHyprOpenGLImpl::SRectRenderData{.round = container_radius, .blur = true}
	);

	double icon_x   = container_box.x + container_padding;
	// TODO: use multiple rows when too many icons
	double icon_y   = container_box.y + container_padding;
	CBox   icon_box = {icon_x, icon_y, icon_size, icon_size};
	CBox   text_box = {
        icon_x + icon_size / 2.0, icon_y + icon_size + label_sep, icon_size, font_height
    };
	for (const auto &[i, app_id] : *app_id_focus_history | std::views::enumerate) {
		// Draw selection highlight if this is the selected app
		if (i == idx) {
			CBox selection_box = {
			    icon_x - selection_padding,
			    icon_y - selection_padding,
			    icon_size + 2 * selection_padding,
			    icon_size + label_sep + 2 * selection_padding + font_height
			};

			CHyprOpenGLImpl::SRectRenderData selection_data;
			selection_data.round = selection_radius;
			selection_data.blur  = true;
			g_pHyprOpenGL->renderRect(selection_box, selection_background_color, selection_data);
		}

		auto &[_, app_stuff] = *app_stuff_map->find(app_id); // must exist
		auto data_ptr        = std::get_if<AppRenderData>(&app_stuff.app_info);
		if (!data_ptr) {
			log(INFO, "AppSwitcher: data not available for class={}", app_id);
			icon_x     += icon_size + icon_sep;
			text_box.x += icon_size + icon_sep;
			continue;
		}
		auto &[app_name, icon_texture] = *data_ptr;

		if (icon_texture && icon_texture->m_texID) {
			CRegion                             damage = icon_box;
			CHyprOpenGLImpl::STextureRenderData tex_data;
			tex_data.damage   = &damage;
			tex_data.overallA = 1.0;
			tex_data.round    = 0;
			g_pHyprOpenGL->renderTexture(icon_texture, icon_box, tex_data);
		} else {
			log(INFO, "AppSwitcher: icon not available for {}", app_name);
		}

		icon_box.x += icon_size + icon_sep;

		// Draw label
		if (auto text_texture =
		        g_pHyprRenderer->renderText(app_name, font_color, font_size, false, font_family)) {
			double text_width                          = text_texture->m_size.x;
			text_box.x                                -= text_width / 2.0;
			text_box.w                                 = text_width;
			CRegion                             damage = text_box;
			CHyprOpenGLImpl::STextureRenderData render_data;
			render_data.overallA = 1.0;
			render_data.damage   = &damage;
			render_data.round    = 0;
			g_pHyprOpenGL->renderTexture(text_texture, text_box, render_data);
			text_box.x += text_width / 2.0;
		}

		text_box.x += icon_size + icon_sep;
		icon_x     += icon_size + icon_sep;
	}

	// FIXME: I do not think the whole monitor needs to be damaged. But if I
	// just damage the container/border boxes, wallpaper leaks through the
	// container instead of the window below if the container is open above a
	// JetBrains XWayland window. Tried RENDER_LAST_MOMENT as well.
	g_pHyprRenderer->damageMonitor(monitor);
}

void AppSwitcher::reset_config(const AppSwitcherConfig &config)
{
	container_background_color =
	    CHyprColor{static_cast<uint64_t>(config.container_background_color->value())};
	container_border_color =
	    CHyprColor{static_cast<uint64_t>(config.container_border_color->value())};
	container_padding      = config.container_padding->value();
	container_radius       = config.container_radius->value();
	container_border_width = config.container_border_width->value();

	selection_background_color =
	    CHyprColor{static_cast<uint64_t>(config.selection_background_color->value())};
	selection_padding = config.selection_padding->value();
	selection_radius  = static_cast<int>(config.selection_radius->value());

	font_family = config.font_family->value();
	font_color  = CHyprColor{static_cast<uint64_t>(config.font_color->value())};
	font_size   = static_cast<int>(config.font_size->value());
	label_sep   = config.label_sep->value();

	icon_size = config.icon_size->value();
	icon_sep  = config.icon_sep->value();

	font_height =
	    font_size
	        ? g_pHyprRenderer->renderText("X", font_color, font_size, false, font_family)->m_size.y
	        : 0;
}

void AppSwitcher::load_icon_textures() const
{
	for (auto &app_stuff : *app_stuff_map | std::views::values) {
		auto &[_, app_info] = app_stuff;
		if (auto future = std::get_if<std::future<AppInfo *>>(&app_info)) {
			if (future->wait_for(0s) != std::future_status::ready)
				continue;
			if (auto app_info_ptr = future->get(); !app_info_ptr->icon.buffer) {
				app_info = AppRenderData{app_info_ptr->name, {}};
			} else {
				auto &icon      = app_info_ptr->icon;
				auto  texture   = g_pHyprRenderer->createTexture();
				texture->m_size = {
				    static_cast<double>(icon.width), static_cast<double>(icon.height)
				};

				glGenTextures(1, &texture->m_texID);
				glBindTexture(GL_TEXTURE_2D, texture->m_texID);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

				auto format = icon.format != ImageFormat::RGB ? GL_RGBA : GL_RGB;
				switch (icon.format) {
				case ImageFormat::RGB:  format = GL_RGB; break;
				case ImageFormat::BGRA: // swizzling done later
				case ImageFormat::RGBA: format = GL_RGBA; break;
				}
				glTexImage2D(
				    GL_TEXTURE_2D,
				    0,
				    format,
				    icon.width,
				    icon.height,
				    0,
				    format,
				    GL_UNSIGNED_BYTE,
				    icon.buffer.get()
				);
				if (icon.format == ImageFormat::BGRA) {
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
				}
				glBindTexture(GL_TEXTURE_2D, 0);

				app_info = AppRenderData{app_info_ptr->name, texture};
			}
		}
	}
}
