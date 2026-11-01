#pragma once

// Phase 11 skeleton (PROMPT.md section 14). Theme options (Recalbox "subsets"): which option of
// each subset is active (used by the loader, PR 11b), and how it is stored in es_settings.cfg
// (PR 11e). Pure functions: the Settings glue is in the "Theme options" menu (PR 11e).

#include "theme/ThemeLoader.h"

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace es::theme {

// es_settings.cfg key of one option: "ThemeOption.<themeSet>.<subsetId>" (string setting).
// Project format (Recalbox keeps them in recalbox.conf): documented in docs/THEMES.md.
[[nodiscard]] std::string optionSettingKey(std::string_view themeSet, std::string_view subsetId);

// Active option of every subset: the stored choice when it is still one of the subset's
// choices, otherwise the first choice. Subsets without any choice are left out. Stored entries
// of subsets that no longer exist are ignored (and left in the settings file, rule 2).
[[nodiscard]] std::map<std::string, std::string, std::less<>> resolveSelectedOptions(
	const std::vector<ThemeSubset>& subsets, const std::map<std::string, std::string, std::less<>>& stored);

// Sorts the choices of an option the way Recalbox does: numerically if every name starts with
// an index ("10 - b", "2 - a" -> 2 then 10), otherwise alphabetically.
[[nodiscard]] std::vector<ThemeOptionChoice> sortOptionChoices(const std::vector<ThemeOptionChoice>& choices);

// Display name: drops a leading "<number> - " / "<number>." prefix used for ordering.
[[nodiscard]] std::string displayOptionName(const std::string& name);

} // namespace es::theme
