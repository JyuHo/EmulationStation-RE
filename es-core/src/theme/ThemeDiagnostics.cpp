#include "theme/ThemeDiagnostics.h"

#include "Log.h"

#include <algorithm>

namespace es::theme {

std::string toString(const SourceLocation& location)
{
	if(location.line > 0)
		return location.file.string() + ":" + std::to_string(location.line);
	return location.file.string();
}

void Diagnostics::warn(SourceLocation where, std::string message)
{
	mItems.push_back({Severity::Warning, std::move(where), std::move(message)});
}

void Diagnostics::error(SourceLocation where, std::string message)
{
	mItems.push_back({Severity::Error, std::move(where), std::move(message)});
}

bool Diagnostics::hasErrors() const
{
	return std::any_of(mItems.begin(), mItems.end(), [](const Diagnostic& d) { return d.severity == Severity::Error; });
}

std::size_t Diagnostics::warningCount() const
{
	return (std::size_t)std::count_if(mItems.begin(), mItems.end(), [](const Diagnostic& d) { return d.severity == Severity::Warning; });
}

void Diagnostics::log() const
{
	for(const auto& d : mItems)
	{
		if(d.severity == Severity::Error)
			Log::write(LogError, "{}: {}", toString(d.where), d.message);
		else
			Log::write(LogWarning, "{}: {}", toString(d.where), d.message);
	}
}

LineIndex::LineIndex(std::string_view text)
{
	mSize = text.size();
	if(text.empty()) return;
	mLineStarts.push_back(0);
	for(size_t i = 0; i < text.size(); i++)
	{
		if(text[i] == '\n')
		{
			if(i + 1 < text.size())
				mLineStarts.push_back(i + 1);
		}
	}
	// remove trailing empty if file ends with newline? Keep standard: line count = lineStarts size
}

int LineIndex::lineAt(std::ptrdiff_t offset) const
{
	if(offset < 0 || mLineStarts.empty() || (size_t)offset >= mSize) return 0;
	if((size_t)offset >= mLineStarts.back() + 1000000) {} // noop guard
	// binary search
	size_t lo = 0, hi = mLineStarts.size();
	while(lo + 1 < hi)
	{
		size_t mid = (lo + hi) / 2;
		if(mLineStarts[mid] <= (size_t)offset)
			lo = mid;
		else
			hi = mid;
	}
	if(mLineStarts[lo] <= (size_t)offset)
		return (int)(lo + 1);
	return 0;
}

} // namespace es::theme
