// Numbers in configuration, theme and gamelist files always use '.', whatever the user's locale.
#include "MetaData.h"
#include "Util.h"

#include <catch2/catch_test_macros.hpp>
#include "pugixml/pugixml.hpp"

#include <clocale>
#include <cstdlib>
#include <string>

namespace {

// Switches the process to a French locale for one test, then back to "C".
class FrenchLocale
{
public:
	FrenchLocale()
	{
		for(const char* name : {"fr_FR.UTF-8", "fr_FR.utf8", "fr_FR"})
		{
			if(std::setlocale(LC_ALL, name) != nullptr)
			{
				mName = name;
				break;
			}
		}
	}
	~FrenchLocale() { std::setlocale(LC_ALL, "C"); }

	bool available() const { return !mName.empty(); }
	const std::string& name() const { return mName; }

private:
	std::string mName;
};

} // namespace

TEST_CASE("parseFloat and parseInt behave like atof/atoi without the locale", "[Locale]")
{
	CHECK(parseFloat("0.5") == 0.5f);
	CHECK(parseFloat(" 0.25") == 0.25f);
	CHECK(parseFloat("+1.5") == 1.5f);
	CHECK(parseFloat("-2") == -2.0f);
	CHECK(parseFloat("0.75 trailing") == 0.75f);
	CHECK(parseFloat("") == 0.0f);
	CHECK(parseFloat("abc") == 0.0f);
	CHECK(parseFloat("abc", 1.0f) == 1.0f);

	CHECK(parseInt("42") == 42);
	CHECK(parseInt(" 7x") == 7);
	CHECK(parseInt("+3") == 3);
	CHECK(parseInt("") == 0);
	CHECK(parseInt("x", -1) == -1);
}

TEST_CASE("a French locale keeps '.' as the decimal separator", "[Locale]")
{
	FrenchLocale french;
	if(!french.available())
		SKIP("fr_FR.UTF-8 is not generated on this system");

	// what goes wrong without applyUserLocale(): the C library expects a comma
	REQUIRE(std::string(std::localeconv()->decimal_point) == ",");
	CHECK(std::atof("0.5") == 0.0);

	setenv("LC_ALL", french.name().c_str(), 1);
	applyUserLocale();
	unsetenv("LC_ALL");

	CHECK(std::string(std::localeconv()->decimal_point) == ".");
	CHECK(std::atof("0.5") == 0.5);

	// pugixml formats and parses numbers with the C library (settings, themes)
	pugi::xml_document doc;
	pugi::xml_node node = doc.append_child("float");
	node.text().set(0.5f);
	CHECK(std::string(node.text().as_string()) == "0.5");
	CHECK(node.text().as_float() == 0.5f);

	// gamelist ratings
	MetaDataList md(GAME_METADATA);
	md.set("rating", "0.8");
	CHECK(md.getFloat("rating") == 0.8f);

	// the rest of the locale still follows the user
	CHECK(std::string(std::setlocale(LC_CTYPE, nullptr)) == french.name());
}
