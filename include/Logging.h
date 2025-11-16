#pragma once

#include "Hyprland.h"
#include <type_traits>

#ifdef DEBUG_LOGS
template <typename... Args>
static void log(eLogLevel level, std::format_string<Args...> fmt_string, Args &&...fmt_args)
{
	Debug::log(level, "[wm] {}", std::format(fmt_string, std::forward<Args>(fmt_args)...));
}
#else
template <typename... Args>
static void
log([[maybe_unused]] eLogLevel                   level,
    [[maybe_unused]] std::format_string<Args...> fmt,
    [[maybe_unused]] Args &&...fmt_args)
{}
#endif

#ifndef NDEBUG
#define LOG_TRACE(fmt, ...) log(LOG, "{}: " fmt, __PRETTY_FUNCTION__, __VA_ARGS__)
#else
#define LOG_TRACE(fmt, ...)                                                                       \
	do {                                                                                          \
	} while (0)
#endif

template <typename T>
    requires std::is_convertible_v<T, PHLWINDOWREF>
std::string as_str(const T &window)
{
	if (window) {
		return std::format(
		    "{}::{}:0x{:x}",
		    window->m_class,
		    window->m_title,
		    reinterpret_cast<uintptr_t>(window.get())
		);
	} else {
		return std::format(
		    "window::nullptr",
		    window->m_class,
		    window->m_title,
		    reinterpret_cast<uintptr_t>(window.get())
		);
	}
}

template <typename T>
    requires std::is_convertible_v<T, PHLWORKSPACEREF>
std::string as_str(const T &workspace)
{
	return std::format(
	    "{}::0x{:x}", workspace->m_name, reinterpret_cast<uintptr_t>(workspace.get())
	);
}

template <typename T>
std::string as_str(const T &thing)
{
	return std::format("0x{:x}", reinterpret_cast<uintptr_t>(thing.get()));
}
