export module wm.Support.Logging;

import std;
import hyprland.config;
import hyprland.debug;
import hyprland.desktop;
import hyprland.helpers;
import hyprutils.cli;

using std::uint8_t, std::uintptr_t;
using Desktop::View::CWindow;

template <typename T>
using DerefType = std::remove_cvref_t<decltype(*std::declval<T>())>;

template <typename T, typename... Types>
concept IsOneOf = (std::same_as<T, Types> || ...);

export namespace wm {

enum class LogLevel : uint8_t {
	TRACE = 0,
	DEBUG,
	WARN,
	ERR,
	CRIT,
};

#ifdef DEBUG_LOGS
template <LogLevel Level, typename... Args>
void log(std::format_string<Args...> fmt_string, Args &&...fmt_args)
{
	Log::logger->log(
	    static_cast<Hyprutils::CLI::eLogLevel>(Level),
	    "[wm] {}",
	    std::format(fmt_string, std::forward<Args>(fmt_args)...)
	);
}
#else
template <LogLevel Level, typename... Args>
void log(std::format_string<Args...>, Args &&...)
{}
#endif

// Hyprland's std::formatter for PHLWINDOW is a bit too verbose for my usage.
template <typename T>
    requires requires(T t) {
	    { *t };
	    { !t } -> std::convertible_to<bool>;
	    std::to_address(t);
    } && IsOneOf<DerefType<T>, CWindow, CWorkspace, CMonitor>
std::string as_str(const T &thing)
{
	using Pointee = DerefType<T>;

	if (!thing)
		return std::format("nullptr");

	std::string desc;
	if constexpr (std::same_as<Pointee, CWindow>)
		desc = std::format("{}::{}", thing->m_class, thing->m_title);
	else if constexpr (IsOneOf<Pointee, CWorkspace, CMonitor>)
		desc = std::format("{}::{}", thing->m_id, thing->m_name);

	return std::format("{}:0x{:x}", desc, reinterpret_cast<uintptr_t>(std::to_address(thing)));
}
} // namespace wm
