export module wm.Support.StringPool;

import std;
import absl;

export namespace wm {

using std::size_t;

using StringPoolEntry = std::pair<std::unique_ptr<char[]>, size_t>;

struct StringPoolHash {
	using is_transparent = void;

	size_t operator()(const StringPoolEntry &t) const;
	size_t operator()(std::string_view sv) const;
};

struct StringPoolEq {
	using is_transparent = void;

	bool operator()(const StringPoolEntry &lhs, const StringPoolEntry &rhs) const;
	bool operator()(const StringPoolEntry &lhs, std::string_view rhs) const;
	bool operator()(std::string_view lhs, const StringPoolEntry &rhs) const;
};

class StringPool {
	absl::flat_hash_set<StringPoolEntry, StringPoolHash, StringPoolEq> pool;

public:
	const char *get(std::string_view sv);
	const char *find(std::string_view sv) const;
	void        remove(std::string_view sv);
};

} // namespace wm
