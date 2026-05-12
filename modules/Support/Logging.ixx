module;

#include "Hyprland.h"

export module wm.Support.Logging;

namespace wm {
#ifdef DEBUG_LOGS
export template <typename... Args>
void log(eLogLevel level, std::format_string<Args...> fmt_string, Args &&...fmt_args)
{ Debug::log(level, "[wm] {}", std::format(fmt_string, std::forward<Args>(fmt_args)...)); }
#else
export template <typename... Args>
void log(eLogLevel, std::format_string<Args...>, Args &&...)
{}
#endif

namespace detail {
template <typename T>
using DerefType = std::remove_cvref_t<decltype(*std::declval<T>())>;

template <typename T, typename... Types>
concept IsOneOf = (std::same_as<T, Types> || ...);
} // namespace detail

// Hyprland's std::formatter for PHLWINDOW is a bit too verbose for my usage.
export template <typename T>
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
} // namespace wm
