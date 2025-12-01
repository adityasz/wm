module;

#include "nkutils.h"

export module wm.Support.AppInfoLoader;

import std;
import wm.Support.Image;

export namespace wm {
struct AppInfo {
	std::string name;
	Image       icon;
};

/// Has one thread that does IO crap so that the main thread is not blocked.
class AppInfoLoader {
	struct Task {
		std::string             app_id;
		std::string             initial_app_id;
		std::promise<AppInfo *> promise;
	};

	/// open apps are never removed from this cache
	std::unordered_map<std::string, std::unique_ptr<AppInfo>> cache;
	std::vector<std::string>                                  app_dirs;
	std::vector<const char *>                                 themes;
	NkXdgThemeContext                                        *theme_context;
	std::queue<Task>                                          task_queue;
	std::mutex                                                mtx;
	std::condition_variable                                   cv;
	std::jthread                                              worker;
	int                                                       icon_size;
	bool                                                      shutdown_flag;

	static constexpr int                                      max_cache_entries = 50;

public:
	AppInfoLoader();

	~AppInfoLoader();

	void reload_config();

	std::future<AppInfo *>
	get_app_info(const std::string &app_id, const std::string &initial_app_id);

	void prune(std::span<std::string> app_ids_to_keep);

private:
	void worker_thread();

	[[nodiscard]] std::string
	get_desktop_file_path(std::string_view app_id, std::string_view initial_app_id) const;

	[[nodiscard]] std::unique_ptr<AppInfo>
	load_app_info(const std::string &app_id, const std::string &initial_app_id) const;
};
} // namespace wm
