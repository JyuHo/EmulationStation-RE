#include "theme/ThemeOptions.h"

#include <algorithm>
#include <charconv>
#include <cctype>
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

std::vector<ThemeOptionChoice> sortOptionChoices(const std::vector<ThemeOptionChoice>& choices)
{
	bool allIndexed = !choices.empty();
	for(const auto& c : choices)
	{
		if(c.name.empty() || !std::isdigit((unsigned char)c.name[0]))
		{ allIndexed = false; break; }
	}
	std::vector<ThemeOptionChoice> out = choices;
	if(allIndexed)
		std::sort(out.begin(), out.end(), [](const ThemeOptionChoice& a, const ThemeOptionChoice& b) {
			int ia = 0, ib = 0;
			std::from_chars(a.name.data(), a.name.data() + a.name.size(), ia);
			std::from_chars(b.name.data(), b.name.data() + b.name.size(), ib);
			return ia < ib;
		});
	else
		std::sort(out.begin(), out.end(), [](const ThemeOptionChoice& a, const ThemeOptionChoice& b) {
			return a.name < b.name;
		});
	return out;
}

std::string displayOptionName(const std::string& name)
{
	// strip leading "<number> - " or "<number>."
	size_t i = 0;
	while(i < name.size() && std::isdigit((unsigned char)name[i])) i++;
	if(i > 0 && i < name.size())
	{
		// skip separators
		size_t j = i;
		while(j < name.size() && (name[j] == ' ' || name[j] == '-' || name[j] == '.')) j++;
		if(j > i && j < name.size())
			return name.substr(j);
	}
	return name;
}

} // namespace es::theme
