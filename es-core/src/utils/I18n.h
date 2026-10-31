#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace I18n {

// A gettext .mo catalog, read without glibc's gettext (it ignores LANGUAGE when the
// system locale is C, frequent in kiosk sessions; see the plan, phase 10).
// Format: https://www.gnu.org/software/gettext/manual/html_node/MO-Files.html
class MoCatalog
{
public:
	// Every offset and length is checked: a truncated or corrupted file is an error.
	[[nodiscard]] static std::expected<MoCatalog, std::string> parse(std::span<const std::byte> data);

	// Translation of msgid in context (empty: no context), or nullptr.
	[[nodiscard]] const std::string* find(std::string_view context, std::string_view msgid) const;
	// Plural form of msgid for n, or nullptr.
	[[nodiscard]] const std::string* findPlural(std::string_view context, std::string_view msgid, long n) const;

	// Index of the plural form for n, from the Plural-Forms header (Germanic rule by default).
	[[nodiscard]] unsigned long pluralIndex(long n) const;
	[[nodiscard]] std::size_t size() const { return mEntries.size(); }

private:
	struct Entry
	{
		std::string singular;			  // msgstr, or msgstr[0]
		std::vector<std::string> plurals; // msgstr[0..n-1] of a plural entry
	};

	const Entry* lookup(std::string_view context, std::string_view msgid) const;

	std::unordered_map<std::string, Entry> mEntries; // key: msgid, or context "\x04" msgid
	std::string mPluralExpression = "n != 1";
	unsigned long mPluralCount = 2;
};

// Evaluates a C plural expression such as "(n%10==1 && n%100!=11 ? 0 : 1)" for n.
// Returns an error for anything that is not a well-formed expression of n.
[[nodiscard]] std::expected<unsigned long, std::string> evaluatePlural(std::string_view expression, long n);

// Loads emulationstation2.mo for language ("fr_FR", then "fr"); "en" or an empty string
// (the source language) or a missing catalog leave the strings untranslated.
void init(const std::string& language);
void shutdown();

std::string translate(std::string_view text);
std::string translatePlural(std::string_view singular, std::string_view plural, long n);
std::string translateContext(std::string_view context, std::string_view text);

// Upper case after translation, for menus: handles Latin, Greek and Cyrillic letters
// ("é" -> "É"), unlike toupper().
std::string toUpperUtf8(std::string_view text);

} // namespace I18n

#define _(x) I18n::translate(x)
#define _N(s, p, n) I18n::translatePlural(s, p, n)
#define _P(c, t) I18n::translateContext(c, t)
#define _U(t) I18n::toUpperUtf8(I18n::translate(t))
