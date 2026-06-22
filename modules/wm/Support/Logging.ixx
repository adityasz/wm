export module wm.Support.Logging;

import std;
import hyprland.config;
import hyprland.debug;
import hyprland.desktop;
import hyprland.helpers;
import hyprutils.cli;

using std::size_t, std::uint8_t, std::uintptr_t;
using Desktop::View::CWindow;

template <typename T, typename... Types>
concept IsOneOf = (std::same_as<T, Types> || ...);

export namespace wm {

template <std::size_t N>
struct ComptimeString {
	char str[N]{};

	consteval ComptimeString() = default;

	consteval ComptimeString(const char (&arr)[N])
	{
		for (size_t i = 0; i < N; ++i)
			str[i] = arr[i];
	}

	template <std::size_t M>
	consteval auto operator+(const ComptimeString<M> &other) const
	{
		ComptimeString<N + M - 1> res{};
		for (size_t i = 0; i < N - 1; ++i)
			res.str[i] = str[i];
		for (size_t i = 0; i < M; ++i)
			res.str[i + N - 1] = other.str[i];
		return res;
	}

	template <std::size_t M>
	consteval auto operator+(const char (&arr)[M]) const
	{ return *this + ComptimeString<M>(arr); }
};

enum class LogLevel : uint8_t {
	TRACE = 0,
	DEBUG,
	WARN,
	ERR,
	CRIT,
};

#ifdef DEBUG_LOGS
template <LogLevel Level, ComptimeString FmtStr, typename... Args>
void log(Args &&...fmt_args)
{
	static constexpr auto fmt_str = ComptimeString{"[wm] "} + FmtStr;
	Log::logger->log(
	    static_cast<Hyprutils::CLI::eLogLevel>(Level), fmt_str.str, std::forward<Args>(fmt_args)...
	);
}
#else
template <LogLevel Level, ComptimeString FmtStr, typename... Args>
void log(Args &&...)
{}
#endif

} // namespace wm

template <>
struct std::formatter<CWindow *> {
	constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }

	auto format(const CWindow *thing, format_context &ctx) const
	{
		return std::format_to(
		    ctx.out(),
		    "0x{:x} {{ id: {}, title: {} }}",
		    reinterpret_cast<uintptr_t>(std::to_address(thing)),
		    thing->m_initialClass,
		    thing->m_title
		);
	}
};

template <typename T>
    requires IsOneOf<T, CWorkspace *, CMonitor *>
struct std::formatter<T> {
	constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }

	auto format(const T thing, format_context &ctx) const
	{
		return std::format_to(
		    ctx.out(),
		    "0x{:x} {{ id: {}, name: {} }}",
		    reinterpret_cast<uintptr_t>(std::to_address(thing)),
		    thing->m_id,
		    thing->m_name
		);
	}
};
