export module wm.AppInfoLoader.Xdg;

import std;

using std::uint64_t;

auto aligned_deleter     = [](char *ptr) { ::operator delete[](ptr, std::align_val_t{32}); };
using unique_aligned_ptr = std::unique_ptr<char, decltype(aligned_deleter)>;

export namespace wm {

struct XdgAppDirs {
	// Each dir contains trailing `/`
	std::vector<const char *> dirs;
	std::unique_ptr<char[]>   arena;
};

struct DesktopFileInfo {
	std::string_view name;
	std::string_view iconstring;
	std::string_view startup_wm_class;

	// for tests
	bool operator==(const DesktopFileInfo &other) const = default;
};

[[nodiscard]] std::pair<unique_aligned_ptr, int> read_desktop_file(int fd);

[[nodiscard]] DesktopFileInfo get_desktop_file_info(const char *data, int size);

[[nodiscard]] XdgAppDirs get_xdg_app_dirs();

} // namespace wm
