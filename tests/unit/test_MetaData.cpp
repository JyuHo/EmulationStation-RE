// Pins the gamelist.xml metadata round trip: reading then writing must not
// change anything (see the user-data rule in PROMPT.md).
#include "MetaData.h"

#include <catch2/catch_test_macros.hpp>
#include "pugixml/pugixml.hpp"

#include <sstream>

namespace {
std::string toString(const pugi::xml_node& node)
{
	std::ostringstream out;
	node.print(out, "", pugi::format_raw);
	return out.str();
}
} // namespace

TEST_CASE("Game metadata defaults", "[MetaData]")
{
	MetaDataList md(GAME_METADATA);
	CHECK(md.get("name").empty());
	CHECK(md.get("developer") == "unknown");
	CHECK(md.getInt("playcount") == 0);
	CHECK(md.getFloat("rating") == 0.0f);
}

TEST_CASE("Game metadata XML round trip", "[MetaData]")
{
	const char* xml = "<game>"
					  "<name>Super Test Bros.</name>"
					  "<desc>A game.</desc>"
					  "<rating>0.8</rating>"
					  "<releasedate>19850913T000000</releasedate>"
					  "<developer>Nintendo</developer>"
					  "<players>2</players>"
					  "<playcount>3</playcount>"
					  "</game>";

	pugi::xml_document in;
	REQUIRE(in.load(xml));

	const MetaDataList md = MetaDataList::createFromXML(GAME_METADATA, in.child("game"), "/roms/nes");
	CHECK(md.get("name") == "Super Test Bros.");
	CHECK(md.get("releasedate") == "19850913T000000");
	CHECK(md.getInt("players") == 2);
	CHECK(md.getInt("playcount") == 3);

	pugi::xml_document out;
	pugi::xml_node game = out.append_child("game");
	md.appendToXML(game, true, "/roms/nes");

	CHECK(toString(game) == xml);
}

TEST_CASE("Relative image paths are resolved against the system folder", "[MetaData]")
{
	pugi::xml_document in;
	REQUIRE(in.load("<game><name>x</name><image>./media/x.png</image></game>"));

	const MetaDataList md = MetaDataList::createFromXML(GAME_METADATA, in.child("game"), "/roms/nes");
	CHECK(md.get("image") == "/roms/nes/media/x.png");
}
