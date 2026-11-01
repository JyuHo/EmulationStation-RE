#include "theme/ThemeDiagnostics.h"

// PHASE11-STUB(11b): every function of this file is to be written in PR 11b (PROMPT.md 14.6).

namespace es::theme {

std::string toString(const SourceLocation& location)
{
	return {}; // PHASE11-STUB(11b)
}

void Diagnostics::warn(SourceLocation where, std::string message)
{
	// PHASE11-STUB(11b)
}

void Diagnostics::error(SourceLocation where, std::string message)
{
	// PHASE11-STUB(11b)
}

bool Diagnostics::hasErrors() const
{
	return false; // PHASE11-STUB(11b)
}

std::size_t Diagnostics::warningCount() const
{
	return 0; // PHASE11-STUB(11b)
}

void Diagnostics::log() const
{
	// PHASE11-STUB(11b)
}

LineIndex::LineIndex(std::string_view text)
{
	// PHASE11-STUB(11b)
}

int LineIndex::lineAt(std::ptrdiff_t offset) const
{
	return 0; // PHASE11-STUB(11b)
}

} // namespace es::theme
