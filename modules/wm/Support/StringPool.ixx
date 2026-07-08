export module wm.Support.StringPool;

import std;
import absl;

export namespace wm {

using std::size_t;

class OwnedString : public std::string_view {
	std::unique_ptr<char[]> data_;

public:
	OwnedString(std::unique_ptr<char[]> data, size_t len);

	static OwnedString from(std::string_view sv);
};

struct [[gnu::visibility("hidden")]] OwnedStringHash {
	using is_transparent = void;

	size_t operator()(std::string_view sv) const;
};

struct [[gnu::visibility("hidden")]] OwnedStringEq {
	using is_transparent = void;

	bool operator()(std::string_view lhs, std::string_view rhs) const;
};

class [[gnu::visibility("hidden")]] OwnedStringPool {
	absl::flat_hash_set<OwnedString, OwnedStringHash, OwnedStringEq> pool;

public:
	std::pair<const char *, bool> get(std::string_view sv);
	const char                   *find(std::string_view sv) const;
	void                          remove(std::string_view sv);
	[[nodiscard]] size_t          size() const;
};

} // namespace wm
