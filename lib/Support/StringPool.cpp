module wm.Support.StringPool;

import std;
import absl;

namespace wm {

size_t StringPoolHash::operator()(const StringPoolEntry &t) const
{ return absl::HashOf(std::string_view{t.first.get(), t.second}); }

size_t StringPoolHash::operator()(std::string_view sv) const { return absl::HashOf(sv); }

bool StringPoolEq::operator()(const StringPoolEntry &lhs, std::string_view rhs) const
{ return std::string_view{lhs.first.get(), lhs.second} == rhs; }

bool StringPoolEq::operator()(const StringPoolEntry &lhs, const StringPoolEntry &rhs) const
{ return (*this)(lhs, std::string_view{rhs.first.get(), rhs.second}); }

bool StringPoolEq::operator()(std::string_view lhs, const StringPoolEntry &rhs) const
{ return (*this)(rhs, lhs); }

std::pair<const char *, bool> StringPool::get(std::string_view sv)
{
	auto it = pool.find(sv);
	if (it != pool.end())
		return {it->first.get(), false};
	auto len = sv.size();
	auto ptr = std::make_unique<char[]>(len + 1);
	std::memcpy(ptr.get(), sv.data(), len);
	ptr[len]              = '\0';
	auto [inserted_it, _] = pool.insert(std::make_pair(std::move(ptr), len));
	return {inserted_it->first.get(), true};
}

const char *StringPool::find(std::string_view sv) const
{
	auto it = pool.find(sv);
	if (it != pool.end())
		return it->first.get();
	return nullptr;
}

void StringPool::remove(std::string_view sv) { pool.erase(sv); }

size_t StringPool::size() const { return pool.size(); }

} // namespace wm
