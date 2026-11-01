#pragma once

// Phase 11 skeleton (PROMPT.md section 14). Theme errors never stop the loading: every problem
// becomes a diagnostic located at "file:line" and is logged once (rule 14.2-R6).

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace es::theme {

// Position in a theme file. line == 0 means "unknown" (file not read, or synthesized value).
struct SourceLocation
{
	std::filesystem::path file;
	int line = 0;

	friend bool operator==(const SourceLocation&, const SourceLocation&) = default;
};

// "<generic path>:<line>", or "<generic path>" alone when line == 0.
[[nodiscard]] std::string toString(const SourceLocation& location);

enum class Severity
{
	Warning, // the faulty node is ignored, the rest of the theme is used
	Error	 // the file cannot be used at all (unreadable, not XML, no <theme>)
};

struct Diagnostic
{
	Severity severity = Severity::Warning;
	SourceLocation where;
	std::string message; // English, not translated (log message, PROMPT.md 5.4)
};

// Collects the diagnostics of one loading. Not thread-safe: one instance per loading.
class Diagnostics
{
public:
	void warn(SourceLocation where, std::string message);
	void error(SourceLocation where, std::string message);

	[[nodiscard]] const std::vector<Diagnostic>& all() const { return mItems; }
	[[nodiscard]] bool hasErrors() const;
	[[nodiscard]] std::size_t warningCount() const;

	// Writes every diagnostic with Log::write: "<file>:<line>: <message>", Warning -> LogWarning,
	// Error -> LogError. Called once per loading by ThemeData, never by the loader itself.
	void log() const;

private:
	std::vector<Diagnostic> mItems;
};

// Maps a byte offset of a file buffer (pugi::xml_node::offset_debug()) to a 1-based line number.
class LineIndex
{
public:
	explicit LineIndex(std::string_view text);

	// 1-based line of the byte at `offset`; 0 when offset is negative or past the end.
	[[nodiscard]] int lineAt(std::ptrdiff_t offset) const;

private:
	std::vector<std::size_t> mLineStarts; // byte offset of the first character of each line
	std::size_t mSize = 0;
};

} // namespace es::theme
