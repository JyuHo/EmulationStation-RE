#include "utils/I18n.h"

#include "utils/Paths.h"
#include "Log.h"

#include <algorithm>
#include <bit>
#include <charconv>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <vector>

namespace I18n {

namespace {

constexpr std::uint32_t kMagic = 0x950412de;
constexpr std::uint32_t kMagicSwapped = 0xde120495;
constexpr char kContextSeparator = '\x04';

// Recursive descent over the C subset used by Plural-Forms, with C precedence:
// ?: < || < && < == != < < > <= >= < + - < * / % < ! < n, numbers, ( ).
class PluralParser
{
public:
	PluralParser(std::string_view text, long n) : mText(text), mN(static_cast<unsigned long>(n < 0 ? -n : n)) {}

	std::expected<unsigned long, std::string> run()
	{
		const auto value = ternary();
		skipSpaces();
		if(!mError.empty())
			return std::unexpected(mError);
		if(mPos != mText.size())
			return std::unexpected(std::format("unexpected '{}' in plural expression", mText.substr(mPos)));
		return value;
	}

private:
	void skipSpaces()
	{
		while(mPos < mText.size() && (mText[mPos] == ' ' || mText[mPos] == '\t' || mText[mPos] == '\n'))
			mPos++;
	}

	bool accept(std::string_view token)
	{
		skipSpaces();
		if(mText.substr(mPos).starts_with(token))
		{
			mPos += token.size();
			return true;
		}
		return false;
	}

	void fail(std::string message)
	{
		if(mError.empty())
			mError = std::move(message);
		mPos = mText.size();
	}

	unsigned long ternary()
	{
		const unsigned long condition = logicalOr();
		if(!accept("?"))
			return condition;
		const unsigned long ifTrue = ternary();
		if(!accept(":"))
		{
			fail("missing ':' in plural expression");
			return 0;
		}
		const unsigned long ifFalse = ternary();
		return condition ? ifTrue : ifFalse;
	}

	unsigned long logicalOr()
	{
		unsigned long value = logicalAnd();
		while(accept("||"))
		{
			const unsigned long right = logicalAnd();
			value = (value || right) ? 1 : 0;
		}
		return value;
	}

	unsigned long logicalAnd()
	{
		unsigned long value = equality();
		while(accept("&&"))
		{
			const unsigned long right = equality();
			value = (value && right) ? 1 : 0;
		}
		return value;
	}

	unsigned long equality()
	{
		unsigned long value = relational();
		while(true)
		{
			if(accept("=="))
				value = value == relational();
			else if(accept("!="))
				value = value != relational();
			else
				return value;
		}
	}

	unsigned long relational()
	{
		unsigned long value = additive();
		while(true)
		{
			if(accept("<="))
				value = value <= additive();
			else if(accept(">="))
				value = value >= additive();
			else if(accept("<"))
				value = value < additive();
			else if(accept(">"))
				value = value > additive();
			else
				return value;
		}
	}

	unsigned long additive()
	{
		unsigned long value = multiplicative();
		while(true)
		{
			if(accept("+"))
				value += multiplicative();
			else if(accept("-"))
				value -= multiplicative();
			else
				return value;
		}
	}

	unsigned long multiplicative()
	{
		unsigned long value = unary();
		while(true)
		{
			const bool divide = accept("/");
			const bool modulo = !divide && accept("%");
			if(!divide && !modulo && !accept("*"))
				return value;
			const unsigned long right = unary();
			if(divide || modulo)
			{
				if(right == 0)
				{
					fail("division by zero in plural expression");
					return 0;
				}
				value = divide ? value / right : value % right;
			}
			else
				value *= right;
		}
	}

	unsigned long unary()
	{
		// "!=" is an operator, not a negation
		skipSpaces();
		if(mPos < mText.size() && mText[mPos] == '!' && !mText.substr(mPos).starts_with("!="))
		{
			mPos++;
			return !unary();
		}
		return primary();
	}

	unsigned long primary()
	{
		if(++mDepth > 64)
		{
			fail("plural expression nested too deeply");
			return 0;
		}
		unsigned long value = 0;
		skipSpaces();
		if(accept("("))
		{
			value = ternary();
			if(!accept(")"))
				fail("missing ')' in plural expression");
		}
		else if(accept("n"))
			value = mN;
		else
		{
			const char* begin = mText.data() + mPos;
			const char* end = mText.data() + mText.size();
			const auto [next, error] = std::from_chars(begin, end, value);
			if(error != std::errc() || next == begin)
				fail("expected n, a number or '(' in plural expression");
			else
				mPos += static_cast<std::size_t>(next - begin);
		}
		mDepth--;
		return value;
	}

	std::string_view mText;
	unsigned long mN;
	std::size_t mPos = 0;
	int mDepth = 0;
	std::string mError;
};

std::vector<std::string> splitNul(std::string_view text)
{
	std::vector<std::string> parts;
	std::size_t start = 0;
	while(true)
	{
		const std::size_t end = text.find('\0', start);
		parts.emplace_back(text.substr(start, end - start));
		if(end == std::string_view::npos)
			return parts;
		start = end + 1;
	}
}

// "Plural-Forms: nplurals=3; plural=(n==1 ? 0 : n<5 ? 1 : 2);" of the header entry.
void readPluralForms(std::string_view header, unsigned long& count, std::string& expression)
{
	const std::size_t line = header.find("Plural-Forms:");
	if(line == std::string_view::npos)
		return;
	std::string_view forms = header.substr(line);
	forms = forms.substr(0, forms.find('\n'));

	const std::size_t nplurals = forms.find("nplurals=");
	const std::size_t plural = forms.find("plural=", nplurals == std::string_view::npos ? 0 : nplurals + 9);
	if(nplurals == std::string_view::npos || plural == std::string_view::npos)
		return;
	unsigned long parsedCount = 0;
	const char* begin = forms.data() + nplurals + 9;
	if(std::from_chars(begin, forms.data() + forms.size(), parsedCount).ec != std::errc() || parsedCount == 0)
		return;
	std::string_view parsedExpression = forms.substr(plural + 7);
	parsedExpression = parsedExpression.substr(0, parsedExpression.rfind(';'));
	if(!evaluatePlural(parsedExpression, 1))
		return; // keep the default rule rather than a broken one
	count = parsedCount;
	expression = std::string(parsedExpression);
}

MoCatalog sCatalog;
bool sLoaded = false;

std::optional<std::filesystem::path> findCatalog(const std::string& language)
{
	const auto find = [](std::string_view lang) {
		return es::paths::find(es::paths::Kind::Data, std::format("locale/{}/LC_MESSAGES/emulationstation2.mo", lang));
	};
	if(auto found = find(language))
		return found;
	// "fr_FR.UTF-8", "fr_FR@euro" -> "fr_FR" -> "fr"
	const std::string_view base(language.data(), std::min(language.find_first_of(".@"), language.size()));
	if(base.size() != language.size())
		if(auto found = find(base))
			return found;
	const std::size_t underscore = base.find('_');
	if(underscore != std::string_view::npos)
		return find(base.substr(0, underscore));
	return std::nullopt;
}

} // namespace

std::expected<unsigned long, std::string> evaluatePlural(std::string_view expression, long n)
{
	return PluralParser(expression, n).run();
}

std::expected<MoCatalog, std::string> MoCatalog::parse(std::span<const std::byte> data)
{
	const auto* bytes = reinterpret_cast<const unsigned char*>(data.data());
	const std::size_t size = data.size();
	if(size < 20)
		return std::unexpected(std::string("file too small for a .mo header"));

	const auto le32 = [&](std::size_t at) {
		return std::uint32_t(bytes[at]) | std::uint32_t(bytes[at + 1]) << 8 | std::uint32_t(bytes[at + 2]) << 16 |
			std::uint32_t(bytes[at + 3]) << 24;
	};
	const std::uint32_t magic = le32(0);
	if(magic != kMagic && magic != kMagicSwapped)
		return std::unexpected(std::string("not a .mo file (bad magic number)"));
	const bool swapped = magic == kMagicSwapped;
	const auto u32 = [&](std::size_t at) {
		const std::uint32_t value = le32(at);
		return swapped ? std::byteswap(value) : value;
	};
	if(u32(4) >> 16 != 0)
		return std::unexpected(std::string("unsupported .mo major revision"));

	const std::size_t count = u32(8);
	const std::size_t originals = u32(12);
	const std::size_t translations = u32(16);
	// each table has count entries of two 32-bit words (length, offset)
	const auto tableFits = [&](std::size_t table) {
		return table <= size && count <= (size - table) / 8;
	};
	if(!tableFits(originals) || !tableFits(translations))
		return std::unexpected(std::string("string tables outside the file"));

	const auto string = [&](std::size_t table, std::size_t index) -> std::optional<std::string_view> {
		const std::size_t length = u32(table + index * 8);
		const std::size_t offset = u32(table + index * 8 + 4);
		if(offset > size || length >= size - offset) // the string is followed by a NUL
			return std::nullopt;
		return std::string_view(reinterpret_cast<const char*>(bytes) + offset, length);
	};

	MoCatalog catalog;
	for(std::size_t i = 0; i < count; i++)
	{
		const auto original = string(originals, i);
		const auto translation = string(translations, i);
		if(!original || !translation)
			return std::unexpected(std::format("string {} outside the file", i));

		// "context\x04msgid\0msgid_plural": the key keeps the context, drops the plural
		const std::string_view key = original->substr(0, original->find('\0'));
		if(key.empty())
		{
			readPluralForms(*translation, catalog.mPluralCount, catalog.mPluralExpression);
			continue;
		}
		Entry entry;
		if(original->find('\0') != std::string_view::npos)
		{
			entry.plurals = splitNul(*translation);
			entry.singular = entry.plurals.front();
		}
		else
			entry.singular = std::string(*translation);
		catalog.mEntries.insert_or_assign(std::string(key), std::move(entry));
	}
	return catalog;
}

const MoCatalog::Entry* MoCatalog::lookup(std::string_view context, std::string_view msgid) const
{
	std::string key;
	if(!context.empty())
	{
		key.reserve(context.size() + 1 + msgid.size());
		key.append(context).push_back(kContextSeparator);
	}
	key.append(msgid);
	const auto found = mEntries.find(key);
	return found == mEntries.end() ? nullptr : &found->second;
}

const std::string* MoCatalog::find(std::string_view context, std::string_view msgid) const
{
	const Entry* entry = lookup(context, msgid);
	// an empty msgstr means "not translated yet"
	return entry && !entry->singular.empty() ? &entry->singular : nullptr;
}

const std::string* MoCatalog::findPlural(std::string_view context, std::string_view msgid, long n) const
{
	const Entry* entry = lookup(context, msgid);
	if(!entry || entry->plurals.empty())
		return nullptr;
	const unsigned long index = pluralIndex(n);
	if(index >= entry->plurals.size() || entry->plurals[index].empty())
		return nullptr;
	return &entry->plurals[index];
}

unsigned long MoCatalog::pluralIndex(long n) const
{
	const unsigned long index = evaluatePlural(mPluralExpression, n).value_or(n == 1 ? 0 : 1);
	return index < mPluralCount ? index : 0;
}

void init(const std::string& language)
{
	shutdown();
	if(language.empty() || language == "en" || language.starts_with("en_"))
		return;

	const auto path = findCatalog(language);
	if(!path)
	{
		Log::write(LogWarning, "No translation found for language {}", language);
		return;
	}
	std::ifstream in(*path, std::ios::binary);
	const std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	auto catalog = MoCatalog::parse(std::as_bytes(std::span(bytes)));
	if(!catalog)
	{
		Log::write(LogError, "Invalid translation catalog {}: {}", path->string(), catalog.error());
		return;
	}
	Log::write(LogInfo, "Loaded {} translations from {}", catalog->size(), path->string());
	sCatalog = std::move(*catalog);
	sLoaded = true;
}

void shutdown()
{
	sCatalog = MoCatalog();
	sLoaded = false;
}

std::string translate(std::string_view text)
{
	const std::string* found = sLoaded ? sCatalog.find({}, text) : nullptr;
	return found ? *found : std::string(text);
}

std::string translatePlural(std::string_view singular, std::string_view plural, long n)
{
	if(const std::string* found = sLoaded ? sCatalog.findPlural({}, singular, n) : nullptr)
		return *found;
	return std::string(n == 1 ? singular : plural);
}

std::string translateContext(std::string_view context, std::string_view text)
{
	const std::string* found = sLoaded ? sCatalog.find(context, text) : nullptr;
	return found ? *found : std::string(text);
}

namespace {
char32_t upper(char32_t c)
{
	if(c >= U'a' && c <= U'z')
		return c - 0x20;
	if(c < 0xB5)
		return c;
	if(c == 0xB5)
		return 0x39C; // micro sign -> Greek capital mu
	if((c >= 0xE0 && c <= 0xFE && c != 0xF7))
		return c - 0x20;
	if(c == 0xFF)
		return 0x178;
	if(c == 0x131)
		return U'I'; // dotless i
	// Latin Extended-A: pairs (upper, lower), even then odd, except two odd-first runs
	if((c >= 0x100 && c <= 0x137) || (c >= 0x14A && c <= 0x177))
		return (c & 1) ? c - 1 : c;
	if((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E))
		return (c & 1) ? c : c - 1;
	if(c == 0x17F)
		return U'S'; // long s
	// Greek
	if(c >= 0x3B1 && c <= 0x3CB && c != 0x3C2)
		return c - 0x20;
	if(c == 0x3C2)
		return 0x3A3; // final sigma
	if(c == 0x3AC)
		return 0x386;
	if(c >= 0x3AD && c <= 0x3AF)
		return c - 0x25;
	if(c == 0x3CC)
		return 0x38C;
	if(c == 0x3CD || c == 0x3CE)
		return c - 0x3F;
	// Cyrillic
	if(c >= 0x430 && c <= 0x44F)
		return c - 0x20;
	if(c >= 0x450 && c <= 0x45F)
		return c - 0x50;
	return c;
}

void appendUtf8(std::string& out, char32_t c)
{
	if(c < 0x80)
		out.push_back(static_cast<char>(c));
	else if(c < 0x800)
	{
		out.push_back(static_cast<char>(0xC0 | (c >> 6)));
		out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
	}
	else if(c < 0x10000)
	{
		out.push_back(static_cast<char>(0xE0 | (c >> 12)));
		out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
		out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
	}
	else
	{
		out.push_back(static_cast<char>(0xF0 | (c >> 18)));
		out.push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3F)));
		out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
		out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
	}
}
} // namespace

std::string toUpperUtf8(std::string_view text)
{
	std::string out;
	out.reserve(text.size());
	for(std::size_t i = 0; i < text.size();)
	{
		const auto lead = static_cast<unsigned char>(text[i]);
		const std::size_t length = lead < 0x80 ? 1
			: (lead >> 5) == 0x6			   ? 2
			: (lead >> 4) == 0xE			   ? 3
			: (lead >> 3) == 0x1E			   ? 4
											   : 0;
		bool valid = length != 0 && i + length <= text.size();
		char32_t c = length == 1 ? lead : length == 2 ? lead & 0x1F : length == 3 ? lead & 0x0F : lead & 0x07;
		for(std::size_t k = 1; valid && k < length; k++)
		{
			const auto next = static_cast<unsigned char>(text[i + k]);
			valid = (next & 0xC0) == 0x80;
			c = (c << 6) | (next & 0x3F);
		}
		if(!valid)
		{
			out.push_back(text[i]); // not UTF-8: copied as is
			i++;
			continue;
		}
		if(c == 0xDF)
			out += "SS"; // ß has no single-letter capital in common use
		else
			appendUtf8(out, upper(c));
		i += length;
	}
	return out;
}

} // namespace I18n
