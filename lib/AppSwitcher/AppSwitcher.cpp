module;

#include <GLES3/gl32.h>
#include <cassert>
#include <wayland-server-core.h>

module wm.AppSwitcher;

import std;
import hyprland.globals;
import hyprland.render;

import wm.AppInfoLoader;
import wm.Support.Logging;
import wm.Support.Utils;

using namespace wm;
using namespace std::chrono_literals;

using std::size_t;
using std::uint64_t;
using Render::GL::g_pHyprOpenGL;
using Render::GL::CHyprOpenGLImpl;


AppSwitcherConfig::AppSwitcherConfig(void *handle) :
    container_background_color(
        add_config<CColorValue, "app_switcher:container:background_color">(handle, 0x11'ff'ff'ff)
    ),
    container_border_color(
        add_config<CColorValue, "app_switcher:container:border_color">(handle, 0x11'80'80'80)
    ),
    container_border_width(
        add_config<CFloatValue, "app_switcher:container:border_width">(handle, 1)
    ),
    container_padding(add_config<CFloatValue, "app_switcher:container:padding">(handle, 20)),
    container_radius(add_config<CIntValue, "app_switcher:container:radius">(handle, 50)),
    selection_background_color(
        add_config<CColorValue, "app_switcher:selection:background_color">(handle, 0x11'00'00'00)
    ),
    selection_padding(add_config<CFloatValue, "app_switcher:selection:padding">(handle, 10)),
    selection_radius(add_config<CIntValue, "app_switcher:selection:radius">(handle, 40)),
    font_family(add_config<CStringValue, "app_switcher:label:font_family">(handle, "Inter")),
    font_color(add_config<CColorValue, "app_switcher:label:font_color">(handle, 0xff'ff'ff)),
    font_size(add_config<CIntValue, "app_switcher:label:font_size">(handle, 0)),
    label_sep(add_config<CFloatValue, "app_switcher:label:separation">(handle, 0)),
    icon_size(add_config<CFloatValue, "app_switcher:icons:size">(handle, 120)),
    icon_sep(add_config<CFloatValue, "app_switcher:icons:separation">(handle, 40)),
    icon_theme(add_config<CStringValue, "app_switcher:icons:theme">(handle, ""))
{}

AppSwitcher::AppSwitcher(const AppSwitcherConfig &config) :
    app_info_loader(
        AppInfoLoaderConfig{
            .icon_size = config.icon_size->value(), .icon_theme = config.icon_theme->value()
        }
    ),
    app_id_focus_history(nullptr),
    app_stuff_map(nullptr),
    timer(nullptr),
    active(false),
    visible(false),
    dirty(false),
    idx(0),
    max_entries(20),
    config(config)
{
	icon_texture_cache.reserve(max_entries);
	load_config();
}

void AppSwitcher::load_config()
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

	font_height = 0;
	if (font_size) {
		if (auto texture =
		        g_pHyprRenderer->renderText("X", font_color, font_size, false, font_family))
		    [[likely]] {
			font_height = texture->m_size.y;
		} else [[unlikely]] {
			log<LogLevel::CRIT, "could not create font texture; is {} a valid font?">(font_family);
		}
	}
}

void AppSwitcher::reset_config()
{
	load_config();
	app_info_loader.reset_config(
	    {.icon_size = icon_size, .icon_theme = config.icon_theme->value()}
	);
}

void AppSwitcher::activate(
    std::vector<const char *>                   *app_id_focus_history,
    absl::flat_hash_map<const char *, AppStuff> *app_stuff_map
)
{
	if (app_id_focus_history->empty()) [[unlikely]] {
		log<LogLevel::DEBUG, "AppSwitcher: no apps to switch, refusing to activate">();
		return;
	}

	first_tab_press = std::chrono::system_clock::now();

	active = true;

	if (!dirty) [[likely]] {
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
	}

	log<LogLevel::TRACE, "show: {}">(*app_id_focus_history);
	this->idx                  = 0;
	this->app_id_focus_history = app_id_focus_history;
	this->app_stuff_map        = app_stuff_map;

	if (!dirty) [[likely]]
		load_icon_textures();
}

bool AppSwitcher::is_active() const { return active; }

void AppSwitcher::highlight_next(bool backwards)
{
	if (!backwards) {
		idx++;
		if (idx == static_cast<int>(app_id_focus_history->size()))
			idx = 0;
	} else {
		if (idx)
			idx--;
		else
			idx = static_cast<int>(app_id_focus_history->size()) - 1;
	}

	assert(
	    (idx >= 0 && idx < static_cast<int>(app_id_focus_history->size())) && "idx out of bounds"
	);

	if (auto monitor = Desktop::focusState()->monitor()) [[likely]]
		g_pHyprRenderer->damageMonitor(monitor);
}

void AppSwitcher::focus_selected()
{
	assert(
	    (idx >= 0 && idx < static_cast<int>(app_id_focus_history->size())) && "idx out of bounds"
	);

	focus_and_raise_window(app_stuff_map->at((*app_id_focus_history)[idx]).windows.front().lock());

	deactivate();
}

void AppSwitcher::deactivate()
{
	if (timer) [[likely]] {
		wl_event_source_remove(timer);
		timer = nullptr;
	}
	active  = false;
	visible = false;
}

void AppSwitcher::on_close_app(const char *closing_app_id)
{
	// no app will be left after this is closed, so deactivate
	if (app_id_focus_history->size() == 1)
		return deactivate();

	for (const auto &[i, app_id] : *app_id_focus_history | std::views::enumerate) {
		if (app_id == closing_app_id) {
			if (idx == i && static_cast<size_t>(idx) == app_id_focus_history->size() - 1)
				idx = 0;
			else if (idx > i)
				idx--;
			return;
		}
	}
}

std::expected<CBox, std::monostate> AppSwitcher::get_container_box() const
{
	auto num_icons    = app_id_focus_history->size();
	auto total_width  = container_padding * 2 + icon_size * num_icons + icon_sep * (num_icons - 1);
	auto total_height = 2 * container_padding + icon_size + label_sep + font_height;
	auto monitor      = Desktop::focusState()->monitor();
	if (!monitor) [[unlikely]] {
		log<LogLevel::DEBUG, "monitor {} is null">(Desktop::focusState()->monitor().get());
		return std::unexpected{std::monostate{}};
	}
	auto center = monitor->m_size * monitor->m_scale / 2;
	return CBox(
	    center.x - total_width / 2.0, center.y - total_height / 2.0, total_width, total_height
	);
}

void AppSwitcher::render()
{
	auto monitor = Desktop::focusState()->monitor();
	if (!monitor) [[unlikely]] {
		log<LogLevel::DEBUG, "monitor {} is null">(Desktop::focusState()->monitor().get());
		return;
	}

	if (dirty) [[unlikely]]
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
		auto app_name        = app_stuff.app_name;
		auto texture_ptr = std::get_if<CSharedPointer<Render::ITexture>>(&app_stuff.icon_texture);
		if (!texture_ptr) [[unlikely]] {
			log<LogLevel::TRACE, "AppSwitcher: data not available for class={}">(app_id);
			icon_x     += icon_size + icon_sep;
			text_box.x += icon_size + icon_sep;
			icon_box.x += icon_size + icon_sep;
			continue;
		}
		auto icon_texture = *texture_ptr;

		if (icon_texture && icon_texture->m_texID) {
			CRegion                             damage = icon_box;
			CHyprOpenGLImpl::STextureRenderData tex_data;
			tex_data.damage   = &damage;
			tex_data.overallA = 1.0;
			tex_data.round    = 0;
			g_pHyprOpenGL->renderTexture(icon_texture, icon_box, tex_data);
		} else {
			log<LogLevel::DEBUG, "AppSwitcher: icon not available for {}">(app_name);
		}

		icon_box.x += icon_size + icon_sep;

		// Draw label
		if (auto text_texture = g_pHyprRenderer->renderText(
		        std::string{app_name}, font_color, font_size, false, font_family
		    )) {
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

void AppSwitcher::load_icon_textures()
{
	for (auto &[app_id, app_stuff] : *app_stuff_map) {
		if (std::holds_alternative<CSharedPointer<Render::ITexture>>(app_stuff.icon_texture)
		    || std::holds_alternative<std::monostate>(app_stuff.icon_texture)) {
			continue;
		}

		auto it = icon_texture_cache.find(app_id);
		if (it == icon_texture_cache.end()) [[unlikely]]
			continue;

		auto *future = std::get_if<std::future<Image>>(&it->second);
		if (!future)
			continue;

		if (future->wait_for(0s) != std::future_status::ready) [[unlikely]]
			continue;

		auto icon = future->get();
		if (!icon.buffer) [[unlikely]] {
			it->second             = std::monostate{};
			app_stuff.icon_texture = std::monostate{};
			continue;
		}

		auto texture    = g_pHyprRenderer->createTexture();
		texture->m_size = {static_cast<double>(icon.width), static_cast<double>(icon.height)};

		glGenTextures(1, &texture->m_texID);
		glBindTexture(GL_TEXTURE_2D, texture->m_texID);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		auto format = icon.format != ImageFormat::RGB ? GL_RGBA : GL_RGB;
		switch (icon.format) {
		case ImageFormat::RGB:  format = GL_RGB; break;
		case ImageFormat::BGRA: [[fallthrough]]; // swizzling done later
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

		it->second             = texture;
		app_stuff.icon_texture = texture;
	}
}

std::variant<std::monostate, IconPending, CSharedPointer<Render::ITexture>>
AppSwitcher::load_app_icon(const char *app_id)
{
	auto [it, inserted] = icon_texture_cache.try_emplace(app_id, std::monostate{});
	if (inserted) {
		if (auto icon = app_info_loader.get_app_icon(app_id)) [[likely]] {
			it->second = std::move(*icon);
			return IconPending{};
		}
		return {};
	}
	return std::visit(
	    [](auto &value)
	        -> std::variant<std::monostate, IconPending, CSharedPointer<Render::ITexture>> {
		    using T = std::decay_t<decltype(value)>;
		    if constexpr (std::is_same_v<T, std::monostate>)
			    return {};
		    else if constexpr (std::is_same_v<T, std::future<Image>>)
			    return IconPending{};
		    else if constexpr (std::is_same_v<T, CSharedPointer<Render::ITexture>>)
			    return value;
	    },
	    it->second
	);
}

void AppSwitcher::prune_cache(std::span<const char *> app_ids_to_keep)
{
	max_entries = std::max(20uz, app_ids_to_keep.size() + app_ids_to_keep.size() / 4);
	if (icon_texture_cache.size() <= max_entries) [[likely]]
		return;

	size_t size_before_pruning = icon_texture_cache.size();
	for (auto it = icon_texture_cache.begin(); it != icon_texture_cache.end();) {
		if (icon_texture_cache.size() == std::max(20uz, app_ids_to_keep.size()))
			break;
		if (!std::ranges::contains(app_ids_to_keep, it->first))
			icon_texture_cache.erase(it++);
		else
			++it;
	}

	if (icon_texture_cache.size() * 2 < size_before_pruning
	    && icon_texture_cache.size() <= max_entries) {
		icon_texture_cache = decltype(icon_texture_cache)(
		    std::make_move_iterator(icon_texture_cache.begin()),
		    std::make_move_iterator(icon_texture_cache.end())
		);
	}
}
