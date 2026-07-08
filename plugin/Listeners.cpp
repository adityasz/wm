module listeners;

import std;

import hyprland.desktop;
import hyprland.devices;
import hyprland.event;
import hyprland.globals;
import hyprland.render;

import globals;

/// https://wiki.hypr.land/Configuring/Advanced-and-Cool/Expanding-functionality/#events
void register_listeners()
{
	static auto open_window = Event::bus()->m_events.window.openEarly.listen(
	    [](const PHLWINDOW &w) { window_manager->on_open_window(w); }
	);
	static auto active_window = Event::bus()->m_events.window.active.listen(
	    [](const PHLWINDOW &w, Desktop::eFocusReason r) { window_manager->on_touch_window(w, r); }
	);
	static auto destroy_window = Event::bus()->m_events.window.destroy.listen(
	    [](const PHLWINDOW &w) { window_manager->on_close_window(w); }
	);
	static auto key_press = Event::bus()->m_events.input.keyboard.key.listen(
	    [](IKeyboard::SKeyEvent e, Event::SCallbackInfo &i) { window_manager->on_key_press(e, i); }
	);
	static auto render = Event::bus()->m_events.render.stage.listen([](eRenderStage s) {
		window_manager->render_app_switcher(s);
	});
	static auto config_reloaded =
	    Event::bus()->m_events.config.reloaded.listen([]() { window_manager->reset_config(); });
}
