module;

#include <emmintrin.h>
#include <immintrin.h>
#include <pmmintrin.h>
#include <smmintrin.h>
#include <tmmintrin.h>

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "nkutils.h"

module wm.AppInfoLoader;

import std;
import llvm.Support;

import wm.AppInfoLoader.Image;
import wm.AppInfoLoader.Xdg;

using namespace wm;

const gchar *AppInfoLoader::icon_fallbacks[]  = {"hicolor", nullptr};
const gchar *AppInfoLoader::sound_fallbacks[] = {nullptr};

AppInfoLoader::AppInfoLoader(const AppInfoLoaderConfig &config) :
    string_saver(string_alloc),
    theme_context(nk_xdg_theme_context_new(icon_fallbacks, sound_fallbacks)),
    scan_finished_flag(false),
    worker_processing_tasks(false),
    shutdown_flag(false)
{
	reset_config(config);
	worker = std::thread(&AppInfoLoader::scan, this);
}

void AppInfoLoader::reset_config(const AppInfoLoaderConfig &config)
{
	bool was_running = worker.joinable();
	if (was_running) {
		// worker is either scanning or in the task loop
		if (!worker_processing_tasks) {
			worker.join();
			worker_processing_tasks = true;
			// will start the task loop after this
		} else {
			// wait until task queue is empty
			std::promise<Image> promise;
			auto                fut = promise.get_future();
			{
				std::lock_guard lk(mtx);
				task_queue.emplace("", std::move(promise));
			}
			cv.notify_one();
			fut.wait();
			{
				std::lock_guard lk(mtx);
				shutdown_flag = true;
			}
			cv.notify_all();
			worker.join();
			shutdown_flag = false;
		}
	}

	icon_size = static_cast<uint16_t>(config.icon_size);

	for (auto &theme : icon_themes)
		delete[] theme;
	icon_themes.clear();
	icon_themes.reserve(2); // people would usually have only one theme
	for (auto theme :
	     config.icon_theme
	         | std::views::split(std::string_view(","))
	         | std::views::transform([](auto s) {
		           auto is_space = [](unsigned char c) { return std::isspace(c); };
		           auto left     = std::ranges::find_if_not(s, is_space);
		           auto right = std::ranges::find_if_not(s | std::views::reverse, is_space).base();
		           if (left == s.end() || left >= right)
			           return std::string_view{""};
		           return std::string_view{left, right};
	           })
	         | std::views::filter([](auto s) { return !s.empty(); })) {
		auto s = new gchar[theme.length() + 1];
		std::memcpy(s, theme.data(), theme.length());
		s[theme.length()] = 0;
		icon_themes.push_back(s);
	}
	icon_themes.push_back(nullptr);

	if (was_running)
		worker = std::thread(&AppInfoLoader::worker_thread, this);
}

AppInfoLoader::~AppInfoLoader()
{
	{
		std::lock_guard lk(mtx);
		shutdown_flag = true;
	}
	cv.notify_all();

	if (worker.joinable())
		worker.join();

	for (auto &theme : icon_themes)
		delete[] theme; // no-op when nullptr
}

// In this implementation, it is assumed that either the desktop file ID or
// the StartupWMClass key are the WM class. If a desktop file ID of one file
// is the same as the value of the StartupWMClass key in some other file,
// the behavior is undefined.
void AppInfoLoader::scan()
{
	static const auto                 app_dirs = get_xdg_app_dirs();
	// Spec:
	// > If multiple files have the same desktop file ID, the first one in the
	// > $XDG_DATA_DIRS precedence order is used.
	absl::flat_hash_set<std::string>  used_desktop_file_ids;
	static constexpr std::string_view extension = ".desktop";
	for (const char *dir : app_dirs.dirs) {
		DIR *dirp = opendir(dir);
		if (!dirp)
			continue;

		auto path_len = std::strlen(dir);

		int dfd = dirfd(dirp);

		while (struct dirent *dp = readdir(dirp)) {
			if (dp->d_name[0] == '.'
			    && (dp->d_name[1] == '\0' || (dp->d_name[1] == '.' && dp->d_name[2] == '\0'))) {
				continue;
			}

			if (dp->d_type != DT_REG && dp->d_type != DT_LNK && dp->d_type != DT_UNKNOWN)
				continue;

			std::string_view filename(dp->d_name);
			if (!filename.ends_with(extension))
				continue;

			auto desktop_file_id = filename;
			desktop_file_id.remove_suffix(extension.length());

			auto [_, inserted] = used_desktop_file_ids.emplace(desktop_file_id);
			if (!inserted)
				continue;

			if (int filefd = openat(dfd, dp->d_name, O_RDONLY | O_CLOEXEC); filefd != -1) {
				auto [buffer, size] = read_desktop_file(filefd);
				auto entries        = get_desktop_file_info(buffer.get(), size);
				// auto app_id         = string_saver.save(
				//     llvm::StringRef{
				//         entries.startup_wm_class.empty() ? desktop_file_id
				//                                          : entries.startup_wm_class
				//     }
				// );

				auto name = entries.name.empty()
				                ? llvm::StringRef{}
				                : string_saver.save(llvm::StringRef{entries.name});

				auto iconstring = entries.iconstring.data();
				if (!entries.iconstring.empty()) {
					// SAFETY: This byte is guaranteed to be '\n' before this.
					const_cast<char *>(iconstring)[entries.iconstring.length()] = '\0';
				}
				auto icon_path = get_icon_path(iconstring);

				auto desktop_file_path =
				    string_saver.save(llvm::Twine{llvm::StringRef{dir, path_len}} + filename)
				        .data(); // dir has trailing '/'

				// Thunderbird's desktop file has ID org.mozilla.Thunderbird
				// (which matches its initial class) but StartupWMClass is
				// thunderbird.
				app_id_to_info_map.try_emplace(
				    string_saver.save(llvm::StringRef{desktop_file_id}),
				    name,
				    icon_path,
				    desktop_file_path,
				    std::chrono::system_clock::now()
				);
				if (!entries.startup_wm_class.empty()
				    && entries.startup_wm_class != desktop_file_id) {
					// For JetBrains software, StartupWMClass matches initial class.
					app_id_to_info_map.try_emplace(
					    string_saver.save(llvm::StringRef{entries.startup_wm_class}),
					    name,
					    icon_path,
					    desktop_file_path,
					    std::chrono::system_clock::now()
					);
				}
				close(filefd);
			}
		}
		closedir(dirp);
	}
	scan_finished_flag = true;
}

const char *AppInfoLoader::get_icon_path(const char *iconstring)
{
	if (!iconstring) [[unlikely]]
		return nullptr;

	if (iconstring[0] == '/')
		return string_saver.save(iconstring).data();

	auto *raw_path = nk_xdg_theme_get_icon(
	    theme_context, icon_themes.data(), "Applications", iconstring, icon_size, 1, 1
	);
	if (!raw_path) [[unlikely]]
		return nullptr;
	auto path = string_saver.save(raw_path).data();
	g_free(raw_path);
	return path;
}


AppInfo AppInfoLoader::get_app_info(std::string_view app_id) const
{
	if (auto it = app_id_to_info_map.find(app_id); it != app_id_to_info_map.end()) [[likely]]
		return AppInfo{.app_id = it->first.data(), .name = it->second.name};
	return AppInfo{.app_id = nullptr, .name = std::string_view{}};
}

std::optional<std::future<Image>> AppInfoLoader::get_app_icon(std::string_view app_id) const
{
	std::string_view name;
	const char      *icon_path;

	if (auto it = app_id_to_info_map.find(app_id); it != app_id_to_info_map.end()) [[likely]] {
		name      = it->second.name;
		icon_path = it->second.icon_path;
		if (!icon_path) [[unlikely]]
			return {};
	} else {
		return {};
	}

	std::promise<Image> promise;
	auto                ret = promise.get_future();
	{
		std::lock_guard lk(mtx);
		task_queue.emplace(icon_path, std::move(promise));
	}
	cv.notify_one();

	return ret;
}

[[nodiscard]] bool AppInfoLoader::is_available()
{
	if (!worker_processing_tasks) [[unlikely]] {
		if (scan_finished_flag) {
			worker_processing_tasks = true;
			worker.join();
			worker = std::thread(&AppInfoLoader::worker_thread, this);
		}
	}
	return worker_processing_tasks;
}

void AppInfoLoader::worker_thread()
{
	while (true) {
		Task task;
		{
			std::unique_lock lk(mtx);
			cv.wait(lk, [this] { return shutdown_flag || !task_queue.empty(); });
			if (shutdown_flag) [[unlikely]]
				break;
			task = std::move(task_queue.front());
			task_queue.pop();
		}
		task.promise.set_value(read_image(task.icon_path, icon_size));
	}
}
