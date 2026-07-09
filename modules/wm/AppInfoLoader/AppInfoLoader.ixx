module;

#include "nkutils.h"

export module wm.AppInfoLoader;

import std;
import absl;
import llvm.Support;

export import wm.AppInfoLoader.Image;
import wm.AppInfoLoader.Xdg;

using std::size_t, std::uint16_t, std::uint64_t;

struct XdgInfo {
	std::string_view                      name;
	const char                           *icon_path;
	const char                           *desktop_file_path;
	std::chrono::system_clock::time_point desktop_file_last_access;
};

struct Task {
	const char             *icon_path;
	std::promise<wm::Image> promise;
};

export namespace wm {

struct AppInfoLoaderConfig {
	double      icon_size;
	std::string icon_theme;
};

struct AppInfo {
	const char      *app_id;
	std::string_view name;
};

// TODO: watch app_dirs.
class AppInfoLoader {
	llvm::BumpPtrAllocator                         string_alloc;
	llvm::StringSaver                              string_saver;
	absl::flat_hash_map<std::string_view, XdgInfo> app_id_to_info_map;
	std::vector<const gchar *>                     icon_themes;
	NkXdgThemeContext                             *theme_context;
	mutable std::queue<Task>                       task_queue;
	mutable std::mutex                             mtx;
	mutable std::condition_variable                cv;
	std::thread                                    worker;
	uint16_t                                       icon_size;
	std::atomic<bool>                              scan_finished_flag;
	bool                                           worker_processing_tasks;
	bool                                           shutdown_flag;

	static const gchar *icon_fallbacks[];
	static const gchar *sound_fallbacks[];

public:
	explicit AppInfoLoader(const AppInfoLoaderConfig &config);

	~AppInfoLoader();

	void reset_config(const AppInfoLoaderConfig &config);

	[[nodiscard]] AppInfo get_app_info(std::string_view app_id) const;

	[[nodiscard]] std::optional<std::future<Image>> get_app_icon(std::string_view app_id) const;

	[[nodiscard]] bool is_available();

private:
	void scan();

	void worker_thread();

	[[nodiscard]] const char *get_icon_path(const char *iconstring);
};
} // namespace wm
