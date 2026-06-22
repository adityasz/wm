export module wm.Support.ComptimeString;

import std;

using std::size_t;

export namespace wm {

template <size_t N>
struct ComptimeString {
	char str[N]{};

	consteval ComptimeString() = default;

	consteval ComptimeString(const char (&arr)[N])
	{
		for (size_t i = 0; i < N; ++i)
			str[i] = arr[i];
	}

	template <size_t M>
	consteval auto operator+(const ComptimeString<M> &other) const
	{
		ComptimeString<N + M - 1> res{};
		for (size_t i = 0; i < N - 1; ++i)
			res.str[i] = str[i];
		for (size_t i = 0; i < M; ++i)
			res.str[i + N - 1] = other.str[i];
		return res;
	}

	template <size_t M>
	consteval auto operator+(const char (&arr)[M]) const
	{ return *this + ComptimeString<M>{arr}; }
};

} // namespace wm
