export module globals;

import std;
import wm.WindowManager;

export [[gnu::visibility("hidden")]] std::optional<wm::WindowManager> window_manager;
