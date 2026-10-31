#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <utility>

// Keeps recently used shared objects alive after their last user lets them go, so that
// coming back to them (scrolling back up a game list) costs nothing. trim() releases the
// least recently used ones that nobody else holds until the memory in use fits the budget;
// an object still held elsewhere (a texture on screen) is never released.
template <typename Key, typename Value> class LruCache
{
public:
	// Marks the object as the most recently used one, adding it if needed.
	void touch(const Key& key, const std::shared_ptr<Value>& value)
	{
		auto found = mIndex.find(key);
		if(found != mIndex.end())
		{
			mEntries.splice(mEntries.end(), mEntries, found->second);
			found->second->second = value;
			return;
		}
		mEntries.emplace_back(key, value);
		mIndex.emplace(key, std::prev(mEntries.end()));
	}

	// usedBytes: memory in use by every object, cached or not. Returns it after trimming.
	template <typename SizeOf> std::size_t trim(std::size_t usedBytes, std::size_t budget, SizeOf sizeOf)
	{
		for(auto it = mEntries.begin(); it != mEntries.end() && usedBytes > budget;)
		{
			if(it->second.use_count() > 1)
			{
				++it;
				continue;
			}
			usedBytes -= std::min(usedBytes, sizeOf(*it->second));
			mIndex.erase(it->first);
			it = mEntries.erase(it);
		}
		return usedBytes;
	}

	void clear()
	{
		mIndex.clear();
		mEntries.clear();
	}

	[[nodiscard]] std::size_t size() const { return mEntries.size(); }
	[[nodiscard]] bool contains(const Key& key) const { return mIndex.contains(key); }

private:
	using Entries = std::list<std::pair<Key, std::shared_ptr<Value>>>;
	Entries mEntries; // least recently used first
	std::map<Key, typename Entries::iterator> mIndex;
};
