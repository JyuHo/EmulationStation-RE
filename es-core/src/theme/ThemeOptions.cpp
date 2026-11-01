#include "theme/ThemeOptions.h"
#include <cstdio>

// PHASE11-STUB: resolveSelectedOptions in PR 11b (the loader needs it), optionSettingKey in PR 11e
// (PROMPT.md 14.6).

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
