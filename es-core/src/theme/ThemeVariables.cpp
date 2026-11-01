#include "theme/ThemeVariables.h"

// PHASE11-STUB(11b): every function of this file is to be written in PR 11b (PROMPT.md 14.6).

namespace es::theme {

bool ThemeVariables::isValidName(std::string_view name)
{
	return false; // PHASE11-STUB(11b)
}

void ThemeVariables::set(
	std::string_view name, std::string value, Diagnostics& diagnostics, const SourceLocation& where)
{
	// PHASE11-STUB(11b)
}

std::optional<std::string_view> ThemeVariables::get(std::string_view name) const
{
	return std::nullopt; // PHASE11-STUB(11b)
}

std::string ThemeVariables::resolve(std::string_view text, Diagnostics& diagnostics, const SourceLocation& where) const
{
	return std::string(text); // PHASE11-STUB(11b)
}

std::string applyLegacySubstitutions(std::string_view text, const LegacySubstitutions& values)
{
	return std::string(text); // PHASE11-STUB(11b)
}

} // namespace es::theme
