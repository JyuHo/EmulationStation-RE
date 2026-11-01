#pragma once

// Phase 11 skeleton (PROMPT.md section 14). Variables of the Recalbox format:
//   <variables><variable name="Primary" value="2e447c"/></variables>  then  ${Primary}
// plus the variables set by the program (${root}, ${system.name}...) and the legacy
// substitutions of the old format ($system, $language, $country).

#include "theme/ThemeDiagnostics.h"

#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace es::theme {

class ThemeVariables
{
public:
	// Valid name (recalbox-emulationstation 10.1.1, ThemeVariablesHolder::ValidateName): first
	// character a letter or '_', then letters, digits, '_' and single dots ("a.b" yes, "a..b" no).
	[[nodiscard]] static bool isValidName(std::string_view name);

	// Defines or replaces a variable. Invalid name: ignored, with a warning located at `where`.
	// The value is stored as written; references inside it are resolved by resolve().
	void set(std::string_view name, std::string value, Diagnostics& diagnostics, const SourceLocation& where);

	[[nodiscard]] std::optional<std::string_view> get(std::string_view name) const;

	// Replaces every ${name} of `text` whose name is defined, recursively (a value may refer to
	// other variables). Undefined names are left as written, without warning: the program
	// resolves some of them later (game-dependent values). A reference cycle or more than 256
	// replacements: the faulty reference is replaced by an empty string, with one warning.
	[[nodiscard]] std::string resolve(
		std::string_view text, Diagnostics& diagnostics, const SourceLocation& where) const;

private:
	std::map<std::string, std::string, std::less<>> mValues;
};

// Legacy substitutions inside paths and <include> (old Recalbox format, also read by
// recalbox-emulationstation 10.1.1, ThemeData::resolveSystemVariable): $system, $language,
// $country. The exact list and the fallback of $system are documented in docs/THEMES.md
// (PR 11a) before being written here (PR 11b).
struct LegacySubstitutions
{
	std::string system;	  // system theme folder ("nes")
	std::string language; // "fr"
	std::string country;  // "FR"
};

[[nodiscard]] std::string applyLegacySubstitutions(std::string_view text, const LegacySubstitutions& values);

} // namespace es::theme
