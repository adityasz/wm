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

#define LOG_TRACE(fmt, ...) log(TRACE, "{}: " fmt, __PRETTY_FUNCTION__, __VA_ARGS__)

template <typename T>
    requires std::is_convertible_v<T, PHLWINDOWREF>
std::string as_str(const T &window)
{
	return std::format("{}::0x{:x}", window->m_class, reinterpret_cast<uintptr_t>(window.get()));
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
