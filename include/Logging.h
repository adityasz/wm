#pragma once

#include "Hyprland.h"

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

namespace detail {
template <typename T>
using DerefType = std::remove_cvref_t<decltype(*std::declval<T>())>;

template <typename T, typename... Types>
concept IsOneOf = (std::same_as<T, Types> || ...);
} // namespace detail

// FIXME: This is just a quick and dirty workaround. Hyprland defines
// std::formatter for PHLWINDOW. It is a bit too verbose for my usage.
// Revisit after I know enough C++ to somehow override that.
template <typename T>
    requires requires(T t) {
	    { *t };
	    { !t } -> std::convertible_to<bool>;
	    std::to_address(t);
    } && detail::IsOneOf<detail::DerefType<T>, CWindow, CWorkspace, CMonitor>
std::string as_str(const T &thing)
{
	using Pointee = detail::DerefType<T>;

	if (!thing)
		return std::format("nullptr");

	std::string desc;
	if constexpr (std::same_as<Pointee, CWindow>)
		desc = std::format("{}::{}", thing->m_class, thing->m_title);
	else if constexpr (detail::IsOneOf<Pointee, CWorkspace, CMonitor>)
		desc = std::format("{}::{}", thing->m_id, thing->m_name);

	return std::format("{}:0x{:x}", desc, reinterpret_cast<uintptr_t>(std::to_address(thing)));
}
