// The catalogs shipped in locale/lang/, as compiled by the es-compile-mo target.
#include "utils/I18n.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct PotEntry
{
	std::string msgid;
	bool plural = false;
};

// Value of a quoted PO string, escapes resolved.
std::string unquote(const std::string& line)
{
	const std::size_t first = line.find('"');
	const std::size_t last = line.rfind('"');
	std::string out;
	for(std::size_t i = first + 1; i < last; i++)
	{
		if(line[i] == '\\' && i + 1 < last)
		{
			const char escaped = line[++i];
			out += escaped == 'n' ? '\n' : escaped == 't' ? '\t' : escaped;
		}
		else
			out += line[i];
	}
	return out;
}

// Every entry of locale/emulationstation2.pot (none has a context yet).
std::vector<PotEntry> readPot()
{
	std::ifstream in(ES_POT_FILE);
	REQUIRE(in);
	std::vector<PotEntry> entries;
	std::string line;
	std::string* current = nullptr;
	while(std::getline(in, line))
	{
		if(line.starts_with("msgid "))
		{
			entries.push_back({unquote(line), false});
			current = &entries.back().msgid;
		}
		else if(line.starts_with("msgid_plural "))
		{
			entries.back().plural = true;
			current = nullptr;
		}
		else if(line.starts_with("\"") && current)
			*current += unquote(line);
		else
			current = nullptr;
	}
	// the header
	std::erase_if(entries, [](const PotEntry& entry) { return entry.msgid.empty(); });
	return entries;
}

std::expected<I18n::MoCatalog, std::string> loadCatalog(const std::string& language)
{
	const fs::path path = fs::path(ES_LOCALE_BUILD_DIR) / language / "LC_MESSAGES" / "emulationstation2.mo";
	std::ifstream in(path, std::ios::binary);
	if(!in)
		return std::unexpected("missing " + path.string());
	const std::vector<char> bytes{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
	return I18n::MoCatalog::parse(std::as_bytes(std::span(bytes)));
}

long placeholders(const std::string& text)
{
	return std::ranges::count(text, '{');
}

} // namespace

TEST_CASE("Every string of the .pot is translated, placeholders kept", "[I18n][Translations]")
{
	const std::vector<PotEntry> pot = readPot();
	REQUIRE(pot.size() > 100);

	const std::string language = GENERATE("fr", "de", "es", "it");
	CAPTURE(language);
	const auto loaded = loadCatalog(language);
	REQUIRE(loaded.has_value());
	const I18n::MoCatalog& catalog = *loaded;

	for(const PotEntry& entry : pot)
	{
		CAPTURE(entry.msgid);
		if(entry.plural)
		{
			for(long n : {1L, 2L})
			{
				const std::string* translated = catalog.findPlural("", entry.msgid, n);
				REQUIRE(translated);
				CHECK(placeholders(*translated) == placeholders(entry.msgid));
			}
			continue;
		}
		const std::string* translated = catalog.find("", entry.msgid);
		REQUIRE(translated);
		CHECK(placeholders(*translated) == placeholders(entry.msgid));
		// the layout of multi-line messages relies on their line breaks
		CHECK(translated->ends_with('\n') == entry.msgid.ends_with('\n'));
	}
}

TEST_CASE("Shipped catalogs follow the plural rule of their language", "[I18n][Translations]")
{
	// French: 0 and 1 are singular; German, Spanish, Italian: only 1
	const auto loadedFr = loadCatalog("fr");
	REQUIRE(loadedFr.has_value());
	const I18n::MoCatalog& fr = *loadedFr;
	CHECK(*fr.findPlural("", "{} game available", 0) == "{} jeu disponible");
	CHECK(*fr.findPlural("", "{} game available", 1) == "{} jeu disponible");
	CHECK(*fr.findPlural("", "{} game available", 5) == "{} jeux disponibles");

	const auto loadedDe = loadCatalog("de");
	REQUIRE(loadedDe.has_value());
	const I18n::MoCatalog& de = *loadedDe;
	CHECK(*de.findPlural("", "{} game available", 0) == "{} Spiele verfügbar");
	CHECK(*de.findPlural("", "{} game available", 1) == "{} Spiel verfügbar");
	CHECK(*de.find("", "back") == "zurück");

	for(const std::string language : {"es", "it"})
	{
		CAPTURE(language);
		const auto catalog = loadCatalog(language);
		REQUIRE(catalog.has_value());
		CHECK(catalog->pluralIndex(0) == 1);
		CHECK(catalog->pluralIndex(1) == 0);
	}
}
