module wm.Support.StringPool;

import std;
import absl;

namespace wm {

OwnedString::OwnedString(std::unique_ptr<char[]> data, size_t len) :
    std::string_view(data.get(), len),
    data_(std::move(data))
{}

OwnedString OwnedString::from(std::string_view sv)
{
	auto buf = std::make_unique_for_overwrite<char[]>(sv.size() + 1);
	std::memcpy(buf.get(), sv.data(), sv.size());
	buf.get()[sv.size()] = '\0';
	return OwnedString(std::move(buf), sv.size());
}

size_t OwnedStringHash::operator()(std::string_view sv) const { return absl::HashOf(sv); }

bool OwnedStringEq::operator()(std::string_view lhs, std::string_view rhs) const
{ return lhs == rhs; }

std::pair<const char *, bool> OwnedStringPool::get(std::string_view sv)
{
	auto it = pool.find(sv);
	if (it != pool.end())
		return {it->data(), false};

	auto [inserted_it, _] = pool.emplace(OwnedString::from(sv));
	return {inserted_it->data(), true};
}

const char *OwnedStringPool::find(std::string_view sv) const
{
	auto it = pool.find(sv);
	if (it != pool.end())
		return it->data();
	return nullptr;
}

void OwnedStringPool::remove(std::string_view sv) { pool.erase(sv); }

size_t OwnedStringPool::size() const { return pool.size(); }

} // namespace wm
