#include "theme/ThemeOptions.h"

// PHASE11-STUB: resolveSelectedOptions in PR 11b (the loader needs it), optionSettingKey in PR 11e
// (PROMPT.md 14.6).

namespace es::theme {

std::string optionSettingKey(std::string_view themeSet, std::string_view subsetId)
{
	return {}; // PHASE11-STUB(11e)
}

std::map<std::string, std::string, std::less<>> resolveSelectedOptions(
	const std::vector<ThemeSubset>& subsets, const std::map<std::string, std::string, std::less<>>& stored)
{
	return {}; // PHASE11-STUB(11b)
}

} // namespace es::theme
