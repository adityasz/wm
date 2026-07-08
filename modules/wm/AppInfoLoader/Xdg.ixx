export module wm.AppInfoLoader.Xdg;

import std;

using std::uint64_t;

[[gnu::visibility("hidden")]]
auto aligned_deleter     = [](char *ptr) { ::operator delete[](ptr, std::align_val_t{32}); };
using unique_aligned_ptr = std::unique_ptr<char, decltype(aligned_deleter)>;

export namespace wm {

struct XdgAppDirs {
	// Each dir contains trailing `/`
	std::vector<const char *> dirs;
	std::unique_ptr<char[]>   arena;
};

struct [[gnu::visibility("hidden")]] DesktopFileInfo {
	std::string_view name;
	std::string_view iconstring;
	std::string_view startup_wm_class;

	// for tests
	bool operator==(const DesktopFileInfo &other) const = default;
};

[[nodiscard]] [[gnu::visibility("hidden")]]
std::pair<unique_aligned_ptr, int> read_desktop_file(int fd);

[[nodiscard]] [[gnu::visibility("hidden")]]
DesktopFileInfo get_desktop_file_info(const char *data, int size);

[[nodiscard]] [[gnu::visibility("hidden")]]
XdgAppDirs get_xdg_app_dirs();

} // namespace wm
