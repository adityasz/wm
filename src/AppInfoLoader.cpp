#include <condition_variable>
#include <filesystem>
#include <future>
#include <mutex>
#include <thread>

#include <glibmm/keyfile.h>
#include <gtkmm.h>

#include "AppInfoLoader.h"
#include "Globals.h"
#include "Image.h"
#include "Logging.h"

const gchar *AppInfoLoader::icon_fallbacks[]  = {"hicolor", nullptr};
const gchar *AppInfoLoader::sound_fallbacks[] = {nullptr};

AppInfoLoader::AppInfoLoader() :
    theme_context(nk_xdg_theme_context_new(icon_fallbacks, sound_fallbacks)),
    worker(&AppInfoLoader::worker_thread, this),
    shutdown_flag(false)
{
	cache.reserve(MAX_ENTRIES);
	app_dirs = Glib::get_system_data_dirs();
	app_dirs.insert(app_dirs.begin(), Glib::get_user_data_dir());
	for (auto &data_dir : app_dirs)
		data_dir += "/applications/";
	reload_config();
}

void AppInfoLoader::reload_config()
{
	icon_size = **get_config<Hyprlang::INT>("app_switcher:icons:size");

	themes.reserve(2); // people would usually have only one theme
	for (auto theme :
	     std::string_view(*get_config<char>("app_switcher:icons:theme"))
	         | std::views::split(std::string_view(","))
	         | std::views::transform([](auto s) {
		           auto is_space = [](unsigned char c) { return std::isspace(c); };
		           auto left     = std::ranges::find_if_not(s, is_space);
		           auto right = std::ranges::find_if_not(s | std::views::reverse, is_space).base();
		           if (left == s.end() || left >= right)
			           return std::string_view("");
		           return std::string_view(left, right);
	           })
	         | std::views::filter([](auto s) { return !s.empty(); })) {
		auto s = new gchar[theme.length() + 1];
		memcpy(s, theme.data(), theme.length());
		s[theme.length()] = 0;
		themes.push_back(s);
	}
	themes.push_back(nullptr);
}

AppInfoLoader::~AppInfoLoader()
{
	{
		std::lock_guard lk(mtx);
		shutdown_flag = true;
	}
	cv.notify_all();
	for (auto &theme : themes)
		delete[] theme; // no-op when nullptr
}

// From waybar (but faster)
static std::string
get_file_by_suffix(std::string_view dir, std::string_view suffix, bool check_lower_case)
{
	if (!std::filesystem::exists(dir))
		return {};

	std::string suffix_lowercase;
	suffix_lowercase.resize(suffix.size());
	std::ranges::transform(suffix, suffix_lowercase.begin(), [](unsigned char c) {
		return std::tolower(c);
	});

	try { // file permissions etc. can cause something to throw maybe
		for (const auto &entry : std::filesystem::recursive_directory_iterator(dir)) {
			if (entry.is_regular_file()) {
				auto filename = entry.path().filename().string();
				if (filename.size() < suffix.size())
					continue;

				if (filename.ends_with(suffix)
				    || (check_lower_case && filename.ends_with(suffix_lowercase))) {
					return entry.path().string();
				}
			}
		}
	} catch (...) {
		return {};
	}
	return {};
}

// Adapted from waybar's codebase.
//
// The implementation assumes that all desktop files end with <app_id>.desktop.
// This is not something I saw as being required by the XDG Desktop Entry
// specification, but most distros name them this way (at least Fedora does).
// Also, suffix matching can lead to false positives.
//
// The right way would be to get cmdline from the pid and then use that to
// search the Exec key of desktop files (matching the binary path, or binary
// name as a fallback), caching that to something like
// ${XDG_CACHE_HOME:$HOME/.cache}/wm/app_id_to_name_and_icon_path, along with
// the timestamp of when the desktop files were last modified, to prevent
// searching for non-existent files when nothing changed.
//
// At runtime, a map from Exec key to the name and icon path can be cached to
// not re-read the same set of files over and over again (maybe this can be
// added to the cache file as well, along with timestamp, on plugin exit).
//
// This sounds nice but wouldn't affect what I see in my app switcher (since my
// desktop files end with <app_id>.desktop), and the workarounds are trivial
// enough. Even if a desktop file managed by the package manager and is named
// adversarially, just create a symlink to it at
// ${XDG_DATA_HOME:$HOME/.local/share}/applications/<app_id>.desktop, and this
// fixes everything since this path is searched before system-level paths.
//
// Fixes are welcome, but this isn't something I would spend my time on (at most
// an hour of work, but still). After all, a plugin is not the right place to
// implement what I wrote above: There should ideally be a standard that handles
// this, so that everyone benefits. (Vicinae probably has a good cache, and idk
// if this plugin can access it.)
std::string
AppInfoLoader::get_desktop_file_path(std::string_view app_id, std::string_view initial_app_id) const
{
	if (app_id.empty() && initial_app_id.empty())
		return {};

	for (const auto &app_dir : app_dirs) {
		if (!app_id.empty()) {
			auto desktop_file_path =
			    get_file_by_suffix(app_dir, std::format("{}.desktop", app_id), true);
			if (!desktop_file_path.empty())
				return desktop_file_path;
		}
		if (!initial_app_id.empty()) {
			auto desktop_file_path =
			    get_file_by_suffix(app_dir, std::format("{}.desktop", initial_app_id), true);
			if (!desktop_file_path.empty())
				return desktop_file_path;
		}
	}

	return {};
}

// Similar to Waybar's hyprland/window module.
//
// If it doesn't find your app: Make sure your app's .desktop file is in a
// standard location and ends with "<app_id>.desktop".
std::unique_ptr<AppInfo>
AppInfoLoader::load_app_info(const std::string &app_id, const std::string &initial_app_id) const
{
	auto desktop_file_path = get_desktop_file_path(app_id, initial_app_id);
	if (desktop_file_path.empty()) {
		log(INFO, "desktop file not found for {}/{}", app_id, initial_app_id);
		return std::make_unique<AppInfo>(app_id, Image{});
	}

	auto desktop_file = Glib::KeyFile::create();
	if (!desktop_file)
		return {};

	std::string app_name;
	std::string icon_name;
	try {
		desktop_file->load_from_file(desktop_file_path);
		app_name = desktop_file->get_string("Desktop Entry", "Name");
	} catch (...) {
		return std::make_unique<AppInfo>(app_id, Image{});
	}
	if (app_name.empty())
		app_name = app_id;
	try {
		icon_name = desktop_file->get_string("Desktop Entry", "Icon");
	} catch (...) {
		return std::make_unique<AppInfo>(app_name, Image{});
	}

	if (icon_name.empty())
		return std::make_unique<AppInfo>(app_name, Image{});

	if (icon_name[0] == '/') // desktop file has path to icon
		return std::make_unique<AppInfo>(app_name, read_image(std::string(icon_name), icon_size));

	if (!theme_context)
		return std::make_unique<AppInfo>(app_name, Image{});

	if (auto icon_path = nk_xdg_theme_get_icon(
	        theme_context, themes.data(), "Applications", icon_name.c_str(), icon_size, 1, TRUE
	    )) {
		return std::make_unique<AppInfo>(
		    app_name, read_image(std::string_view(icon_path), icon_size)
		);
	}

	return std::make_unique<AppInfo>(app_name, Image{});
}

void AppInfoLoader::worker_thread()
{
	while (true) {
		Task task;
		{
			std::unique_lock lk(mtx);
			cv.wait(lk, [this] { return shutdown_flag || !task_queue.empty(); });
			if (shutdown_flag && task_queue.empty())
				break;
			task = std::move(task_queue.front());
			task_queue.pop();
		}
		auto res = load_app_info(task.app_id, task.initial_app_id);
		auto ref = res.get();
		{
			std::lock_guard lk(mtx);
			cache.emplace(task.app_id, std::move(res));
		}
		task.promise.set_value(ref);
	}
}

std::future<AppInfo *>
AppInfoLoader::get_app_info(const std::string &app_id, const std::string &initial_app_id)
{
	{
		std::lock_guard lk(mtx);
		if (auto it = cache.find(app_id); it != cache.end()) {
			std::promise<AppInfo *> p;
			p.set_value(it->second.get());
			return p.get_future();
		}
		if (app_id != initial_app_id) {
			if (auto it2 = cache.find(initial_app_id); it2 != cache.end()) {
				std::promise<AppInfo *> p;
				p.set_value(it2->second.get());
				return p.get_future();
			}
		}
	}

	std::promise<AppInfo *> promise;
	auto                    fut = promise.get_future();
	{
		std::lock_guard lk(mtx);
		task_queue.emplace(app_id, initial_app_id, std::move(promise));
	}
	cv.notify_one();
	return fut;
}

/// TODO: MAX_ENTRIES should be dynamic.
void AppInfoLoader::prune(std::span<std::string> app_ids_to_keep)
{
	std::lock_guard lk(mtx);
	if (cache.size() <= MAX_ENTRIES)
		return;

	size_t size_before_pruning = cache.size();
	for (auto it = cache.begin(); it != cache.end();) {
		if (cache.size() <= MAX_ENTRIES)
			break;
		if (!std::ranges::contains(app_ids_to_keep, it->first))
			it = cache.erase(it);
		else
			++it;
	}

	if (cache.size() < size_before_pruning && cache.size() <= MAX_ENTRIES) {
		std::unordered_map new_map(
		    std::make_move_iterator(cache.begin()), std::make_move_iterator(cache.end())
		);
		cache.swap(new_map);
	}
}
