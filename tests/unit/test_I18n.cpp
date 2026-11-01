#include "utils/I18n.h"
#include "utils/Paths.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cstdlib>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <unistd.h>

namespace fs = std::filesystem;

namespace {
// tests/data/i18n/fr.po, compiled to fr.mo (little-endian) and fr-be.mo (big-endian)
std::vector<char> readFixture(const std::string& name)
{
	std::ifstream in(fs::path(ES_TEST_DATA_DIR) / "i18n" / name, std::ios::binary);
	REQUIRE(in);
	return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::expected<I18n::MoCatalog, std::string> parseFixture(const std::string& name)
{
	const std::vector<char> bytes = readFixture(name);
	return I18n::MoCatalog::parse(std::as_bytes(std::span(bytes)));
}
} // namespace

TEST_CASE("MoCatalog reads msgid, msgctxt and plural entries", "[I18n]")
{
	const std::string name = GENERATE("fr.mo", "fr-be.mo");
	CAPTURE(name);
	const auto parsed = parseFixture(name);
	REQUIRE(parsed.has_value());
	const I18n::MoCatalog& catalog = *parsed;

	REQUIRE(catalog.find("", "GAME SETTINGS"));
	CHECK(*catalog.find("", "GAME SETTINGS") == "PARAMÈTRES DES JEUX");
	CHECK(*catalog.find("", "Back") == "Retour");
	CHECK(*catalog.find("button", "Back") == "Précédent");
	CHECK(catalog.find("menu", "Back") == nullptr);
	CHECK(catalog.find("", "Not translated yet") == nullptr); // empty msgstr
	CHECK(catalog.find("", "unknown") == nullptr);

	// French: plural=(n > 1), so 0 and 1 are singular
	CHECK(*catalog.findPlural("", "{} game", 0) == "{} jeu");
	CHECK(*catalog.findPlural("", "{} game", 1) == "{} jeu");
	CHECK(*catalog.findPlural("", "{} game", 2) == "{} jeux");
	CHECK(catalog.findPlural("", "Back", 2) == nullptr);
}

TEST_CASE("MoCatalog rejects truncated and corrupted files", "[I18n]")
{
	const std::vector<char> good = readFixture("fr.mo");

	SECTION("every truncation is an error, never a crash")
	{
		for(std::size_t size = 0; size < good.size(); size++)
		{
			CAPTURE(size);
			const auto catalog = I18n::MoCatalog::parse(std::as_bytes(std::span(good.data(), size)));
			CHECK_FALSE(catalog.has_value());
		}
	}
	SECTION("bad magic number")
	{
		std::vector<char> bad = good;
		bad[0] = 0;
		CHECK_FALSE(I18n::MoCatalog::parse(std::as_bytes(std::span(bad))).has_value());
	}
	SECTION("string count or offsets pointing outside the file")
	{
		const std::size_t field = GENERATE(8u, 12u, 16u); // count, originals table, translations table
		CAPTURE(field);
		std::vector<char> bad = good;
		bad[field + 3] = '\x7f';
		CHECK_FALSE(I18n::MoCatalog::parse(std::as_bytes(std::span(bad))).has_value());
	}
}

TEST_CASE("Plural-Forms expressions are evaluated like C", "[I18n]")
{
	// Russian: one, few, many
	constexpr std::string_view russian =
		"(n%10==1 && n%100!=11 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2)";
	CHECK(I18n::evaluatePlural(russian, 1) == 0u);
	CHECK(I18n::evaluatePlural(russian, 21) == 0u);
	CHECK(I18n::evaluatePlural(russian, 11) == 2u);
	CHECK(I18n::evaluatePlural(russian, 3) == 1u);
	CHECK(I18n::evaluatePlural(russian, 14) == 2u);
	CHECK(I18n::evaluatePlural(russian, 25) == 2u);

	CHECK(I18n::evaluatePlural("n != 1", 1) == 0u);
	CHECK(I18n::evaluatePlural("n != 1", 0) == 1u);
	CHECK(I18n::evaluatePlural("0", 5) == 0u);
	CHECK(I18n::evaluatePlural("!(n == 1)", 1) == 0u);

	CHECK_FALSE(I18n::evaluatePlural("n %", 1).has_value());
	CHECK_FALSE(I18n::evaluatePlural("n / 0", 1).has_value());
	CHECK_FALSE(I18n::evaluatePlural("(n == 1", 1).has_value());
	CHECK_FALSE(I18n::evaluatePlural("n ? 1", 1).has_value());
	CHECK_FALSE(I18n::evaluatePlural("x", 1).has_value());
	CHECK_FALSE(I18n::evaluatePlural(std::string(1000, '('), 1).has_value());
}

TEST_CASE("toUpperUtf8 upper-cases accented letters", "[I18n]")
{
	CHECK(I18n::toUpperUtf8("paramètres") == "PARAMÈTRES");
	CHECK(I18n::toUpperUtf8("éàçùœÿ") == "ÉÀÇÙŒŸ");
	CHECK(I18n::toUpperUtf8("straße") == "STRASSE");
	CHECK(I18n::toUpperUtf8("łódź") == "ŁÓDŹ");
	CHECK(I18n::toUpperUtf8("ελληνικά") == "ΕΛΛΗΝΙΚΆ");
	CHECK(I18n::toUpperUtf8("русский") == "РУССКИЙ");
	CHECK(I18n::toUpperUtf8("日本語 ok") == "日本語 OK");
	// invalid UTF-8 is copied, not dropped
	CHECK(I18n::toUpperUtf8("a\xff"
							"b") ==
		"A\xff"
		"B");
}

TEST_CASE("I18n::init loads the catalog of the Language setting", "[I18n]")
{
	const fs::path home = fs::temp_directory_path() / std::format("es-test-i18n-{}", ::getpid());
	const fs::path dir = home / "data/emulationstation/locale/fr/LC_MESSAGES";
	fs::create_directories(dir);
	fs::copy_file(fs::path(ES_TEST_DATA_DIR) / "i18n/fr.mo", dir / "emulationstation2.mo");
	es::paths::setHome(home);

	SECTION("fr_FR falls back to fr")
	{
		I18n::init("fr_FR");
		CHECK(_("Back") == "Retour");
		CHECK(_P("button", "Back") == "Précédent");
		CHECK(_N("{} game", "{} games", 2) == "{} jeux");
		CHECK(_U("GAME SETTINGS") == "PARAMÈTRES DES JEUX");
		CHECK(_U("Back") == "RETOUR");
		// two translations in one expression do not overwrite each other
		CHECK(_("Back") + " / " + _P("button", "Back") == "Retour / Précédent");
		CHECK(_("missing") == "missing");
	}
	SECTION("English and unknown languages keep the source strings")
	{
		const std::string language = GENERATE("en", "", "de_DE");
		I18n::init(language);
		CHECK(_("Back") == "Back");
		CHECK(_N("{} game", "{} games", 1) == "{} game");
		CHECK(_N("{} game", "{} games", 2) == "{} games");
	}

	I18n::shutdown();
	es::paths::clearHome();
	fs::remove_all(home);
}

TEST_CASE("I18n loads a catalog from ES_HOME and translates", "[I18n]")
{
	es::paths::setHome(fs::temp_directory_path() / "es-i18n-home");
	const auto home = fs::temp_directory_path() / "es-i18n-home";
	fs::remove_all(home);
	fs::create_directories(home / "data" / "emulationstation" / "locale" / "fr" / "LC_MESSAGES");
	// copy fixture .mo
	fs::copy_file(fs::path(ES_TEST_DATA_DIR) / "i18n" / "fr.mo",
		home / "data" / "emulationstation" / "locale" / "fr" / "LC_MESSAGES" / "emulationstation2.mo",
		fs::copy_options::overwrite_existing);

	es::paths::setHome(home);
	I18n::init("fr");
	CHECK(I18n::translate("GAME SETTINGS") == "PARAMÈTRES DES JEUX");
	I18n::shutdown();
	es::paths::clearHome();
	fs::remove_all(home);
}

TEST_CASE("I18n::init finds the catalogs compiled in the build tree", "[I18n]")
{
	// ./build/release/emulationstation, run without installing: the only catalogs are the ones
	// es-compile-mo writes to build/release/locale, next to the executable (es-tests lives there too)
	const auto exeDir = es::paths::executableDir();
	REQUIRE(exeDir);
	REQUIRE(fs::equivalent(*exeDir, fs::path(ES_LOCALE_BUILD_DIR).parent_path()));

	const fs::path root = fs::temp_directory_path() / std::format("es-test-i18n-build-{}", ::getpid());
	fs::create_directories(root / "home");
	fs::create_directories(root / "share");
	es::paths::setHome(root / "home");
	const char* previous = std::getenv("XDG_DATA_DIRS");
	const std::optional<std::string> saved = previous ? std::optional<std::string>(previous) : std::nullopt;
	setenv("XDG_DATA_DIRS", (root / "share").c_str(), 1);

	I18n::init("fr_FR.UTF-8");
	CHECK(_("Language") == "Langue");
	I18n::init("de");
	CHECK(_("Language") == "Sprache");
	I18n::init("en");
	CHECK(_("Language") == "Language");

	I18n::shutdown();
	if(saved)
		setenv("XDG_DATA_DIRS", saved->c_str(), 1);
	else
		unsetenv("XDG_DATA_DIRS");
	es::paths::clearHome();
	fs::remove_all(root);
}
