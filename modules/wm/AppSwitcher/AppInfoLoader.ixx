module;

#include "nkutils.h"

export module wm.AppSwitcher.AppInfoLoader;

import std;
import hyprland.config;
import hyprutils.memory;
import wm.AppSwitcher.Image;

using std::uint16_t;
using Hyprutils::Memory::CSharedPointer;
using Config::Values::CStringValue;
using Config::Values::CFloatValue;

export namespace wm {
struct AppInfo {
	std::string name;
	Image       icon;
};

struct AppInfoLoaderConfig {
	CSharedPointer<CFloatValue>  icon_size;
	CSharedPointer<CStringValue> theme;
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
	std::vector<const gchar *>                                themes;
	NkXdgThemeContext                                        *theme_context;
	std::queue<Task>                                          task_queue;
	std::mutex                                                mtx;
	std::condition_variable                                   cv;
	std::thread                                               worker;
	uint16_t                                                  icon_size;
	uint16_t                                                  max_entries;
	bool                                                      shutdown_flag;

	static const gchar *icon_fallbacks[];
	static const gchar *sound_fallbacks[];

public:
	explicit AppInfoLoader(const AppInfoLoaderConfig &config);

	~AppInfoLoader();

	void reset_config(const AppInfoLoaderConfig &config);

	std::future<AppInfo *>
	get_app_info(const std::string &app_id, const std::string &initial_app_id);

	void prune(std::span<std::string> app_ids_to_keep);

private:
	void worker_thread();
	std::string
	get_desktop_file_path(std::string_view app_id, std::string_view initial_app_id) const;
	std::unique_ptr<AppInfo>
	load_app_info(const std::string &app_id, const std::string &initial_app_id) const;
};
} // namespace wm
