#include "theme/ThemeOptions.h"
#include <cstdio>

// Option selection (docs/THEMES.md section 8). optionSettingKey was written in 11b, ahead of 11e.

namespace es::theme {

std::string optionSettingKey(std::string_view themeSet, std::string_view subsetId)
{
	return "ThemeOption." + std::string(themeSet) + "." + std::string(subsetId);
}

std::map<std::string, std::string, std::less<>> resolveSelectedOptions(
	const std::vector<ThemeSubset>& subsets, const std::map<std::string, std::string, std::less<>>& stored)
{
	std::map<std::string, std::string, std::less<>> result;
	for(const auto& sub : subsets)
	{
		if(sub.choices.empty()) continue;
		auto it = stored.find(sub.id);
		std::string chosen = sub.choices.front().name;
		if(it != stored.end())
		{
			for(const auto& c : sub.choices)
				if(c.name == it->second) { chosen = it->second; break; }
		}
		result[sub.id] = chosen;
	}
	return result;
}

} // namespace es::theme
