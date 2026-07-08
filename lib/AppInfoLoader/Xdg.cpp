module;

#include "immintrin.h"

#include <errno.h>
#include <unistd.h>

module wm.AppInfoLoader.Xdg;

import std;
import wm.Support.ComptimeString;

using std::uint32_t;

using namespace wm;

[[gnu::always_inline]]
static inline const char *find_newline(const char *s)
{
	const __m256i newline_vec = _mm256_set1_epi8('\n');
	while (true) {
		uint32_t mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(
		    _mm256_loadu_si256(reinterpret_cast<const __m256i *>(s)), newline_vec
		));
		if (mask)
			return s + __builtin_ctz(mask);
		s += 32;
	}
}

template <ComptimeString Key>
[[gnu::always_inline]]
static inline std::string_view extract_field(const char *line_start)
{
	// 1 because ComptimeString::str is null-terminated
	// 4 bytes already compared before calling this
	constexpr size_t key_len = sizeof(Key.str) - 1 - 4;
	if (std::string_view{line_start + 4, key_len} == std::string_view{Key.str + 4, key_len}) {
		const char *ptr = line_start + 4 + key_len;
		// Spec:
		// > Space before and after the equals sign should be ignored;
		// > the = sign is the actual delimiter.
		// "Space" and not "spaces" implies at most one space around the = sign.
		// Usually, there are zero spaces, and [[unlikely]] is supposed to
		// prevent `cmov`s.
		if (*ptr == ' ') [[unlikely]]
			++ptr;
		if (*ptr == '=') {
			++ptr;
			if (*ptr == ' ') [[unlikely]]
				++ptr;
			return std::string_view{ptr, static_cast<size_t>(find_newline(ptr) - ptr)};
		}
	}
	return {};
}

constexpr uint32_t fourcc(const char (&s)[5])
{
	return static_cast<uint32_t>(s[0])
	       | (static_cast<uint32_t>(s[1]) << 8)
	       | (static_cast<uint32_t>(s[2]) << 16)
	       | (static_cast<uint32_t>(s[3]) << 24);
}

namespace wm {

// Extracts Name, Icon, and StartupWMClass from a desktop file.
// Does as little as possible; does not even verify if the desktop file is
// well-formed.
DesktopFileInfo get_desktop_file_info(const char *data, int size)
{
	if (size <= 16 || std::string_view{data, 16} != "[Desktop Entry]\n") [[unlikely]]
		return {};

	DesktopFileInfo info{};
	const __m256i   newline_vec = _mm256_set1_epi8('\n');

	int remaining_keys = 3;

	for (int i = 0; i < size; i += 32) {
		uint32_t mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(
		    _mm256_load_si256(reinterpret_cast<const __m256i *>(data + i)), newline_vec
		));

		while (mask) {
			const char *line_start = data + i + __builtin_ctz(mask) + 1;

			mask &= (mask - 1);

			if (*line_start == '#') [[unlikely]]
				continue;

			uint32_t prefix;
			__builtin_memcpy(&prefix, line_start, sizeof(prefix));

			switch (prefix) {
			case fourcc("Icon"):
				if (info.iconstring.empty()) {
					if (auto val = extract_field<"Icon">(line_start); !val.empty()) {
						info.iconstring = val;
						remaining_keys -= 1;
					}
				}
				break;

			case fourcc("Name"):
				if (info.name.empty()) {
					if (auto val = extract_field<"Name">(line_start); !val.empty()) {
						info.name       = val;
						remaining_keys -= 1;
					}
				}
				break;

			case fourcc("Star"):
				if (info.startup_wm_class.empty()) {
					if (auto val = extract_field<"StartupWMClass">(line_start); !val.empty()) {
						info.startup_wm_class = val;
						remaining_keys       -= 1;
					}
				}
				break;
			}

			if (!remaining_keys)
				return info;
		}
	}

	return info;
}

std::pair<unique_aligned_ptr, int> read_desktop_file(int fd)
{
	auto size = lseek(fd, 0, SEEK_END);
	if (size <= 0 || lseek(fd, 0, SEEK_SET) < 0) [[unlikely]]
		return {unique_aligned_ptr(nullptr, aligned_deleter), 0};

	int                alloc_size = (size + 64 + 31) & ~31;
	unique_aligned_ptr buffer(
	    static_cast<char *>(::operator new[](alloc_size, std::align_val_t{32})), aligned_deleter
	);

	char *ptr       = buffer.get();
	int   remaining = static_cast<int>(size);

	while (remaining > 0) {
		auto bytes_read = read(fd, ptr, remaining);
		if (bytes_read > 0) {
			ptr       += bytes_read;
			remaining -= bytes_read;
		} else if (bytes_read == 0 || errno != EINTR) [[unlikely]] {
			return {unique_aligned_ptr(nullptr, aligned_deleter), 0};
		}
	}

	std::memset(buffer.get() + size, '\n', alloc_size - size);

	return {std::move(buffer), size};
}

XdgAppDirs get_xdg_app_dirs()
{
	XdgAppDirs app_dirs;

	static constexpr std::string_view suffix         = "/applications/";
	static constexpr std::string_view fallback_local = "/.local/share";
	static constexpr const char      *default_sys1   = "/usr/local/share/applications/";
	static constexpr const char      *default_sys2   = "/usr/share/applications/";

	constexpr auto strip = [](std::string_view v) {
		while (v.ends_with('/') && v.length() > 1)
			v.remove_suffix(1);
		return v;
	};

	size_t           total_bytes = 0;
	std::string_view home_base;
	std::string_view home_sub = "";

	if (auto xdg = std::getenv("XDG_DATA_HOME"); xdg && xdg[0]) {
		home_base = strip(xdg);
	} else if (auto home = std::getenv("HOME"); home && home[0]) {
		home_base = strip(home);
		home_sub  = fallback_local;
	}

	if (!home_base.empty())
		total_bytes += home_base.length() + home_sub.length() + suffix.length() + 1;

	std::string_view sys_views[16];
	size_t           sys_count = 0;

	if (auto xdg_sys = std::getenv("XDG_DATA_DIRS"); xdg_sys && xdg_sys[0]) {
		for (const auto &data_dir :
		     std::string_view{xdg_sys}
		         | std::views::split(std::string_view{":"})
		         | std::views::transform([&](auto s) { return strip(std::string_view{s}); })
		         | std::views::filter([](auto s) { return !s.empty(); })) {
			if (sys_count < 16) {
				sys_views[sys_count] = data_dir;
				total_bytes         += sys_views[sys_count].length() + suffix.length() + 1;
				sys_count++;
			}
		}
	}

	app_dirs.dirs.reserve((home_base.empty() ? 0 : 1) + sys_count + (!sys_count ? 2 : 0));

	if (total_bytes > 0) {
		app_dirs.arena = std::make_unique_for_overwrite<char[]>(total_bytes);
		char *ptr      = app_dirs.arena.get();

		auto append = [&](std::string_view s) {
			std::memcpy(ptr, s.data(), s.length());
			ptr += s.length();
		};

		if (!home_base.empty()) {
			app_dirs.dirs.push_back(ptr);
			append(home_base);
			append(home_sub);
			append(suffix);
			*ptr++ = '\0';
		}

		for (size_t i = 0; i < sys_count; ++i) {
			app_dirs.dirs.push_back(ptr);
			append(sys_views[i]);
			append(suffix);
			*ptr++ = '\0';
		}
	}

	if (!sys_count) {
		app_dirs.dirs.push_back(default_sys1);
		app_dirs.dirs.push_back(default_sys2);
	}

	return app_dirs;
}

} // namespace wm
