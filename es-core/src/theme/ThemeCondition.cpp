#include "theme/ThemeCondition.h"

// PHASE11-STUB(11b): every function of this file is to be written in PR 11b (PROMPT.md 14.6).

namespace es::theme {

bool evaluateCondition(std::string_view expression, const ConditionResolver& resolver, Diagnostics& diagnostics,
	const SourceLocation& where)
{
	return true; // PHASE11-STUB(11b)
}

ConditionResolver makeConditionResolver(ConditionEnvironment environment)
{
	return [](std::string_view) -> std::optional<bool> {
		return std::nullopt;
	}; // PHASE11-STUB(11b)
}

} // namespace es::theme
