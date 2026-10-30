// Characterisation tests: they pin the current behaviour so that the Boost ->
// std migration (phase 3) can prove it changes nothing.
#include "Util.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

TEST_CASE("strToUpper", "[Util]")
{
	CHECK(strToUpper(std::string("Super Mario 64")) == "SUPER MARIO 64");
	CHECK(strToUpper("abc") == "ABC");
	std::string s = "mixed Case";
	strToUpper(s);
	CHECK(s == "MIXED CASE");
}

TEST_CASE("resolvePath expands ./ relative to a folder", "[Util]")
{
	CHECK(resolvePath("./nes/game.nes", "/roms", false).generic_string() == "/roms/nes/game.nes");
	CHECK(resolvePath("/abs/game.nes", "/roms", false).generic_string() == "/abs/game.nes");
	CHECK(resolvePath("", "/roms", false).generic_string() == "");
}

TEST_CASE("makeRelativePath and removeCommonPath", "[Util]")
{
	namespace sfs = std::filesystem;
	const sfs::path root = sfs::temp_directory_path() / "es-tests-util";
	sfs::remove_all(root);
	sfs::create_directories(root / "roms" / "nes");
	std::ofstream(root / "roms" / "nes" / "game.nes").put('x');

	const std::string rom = (root / "roms" / "nes" / "game.nes").string();
	const std::string roms = (root / "roms").string();

	CHECK(makeRelativePath(rom, roms, false).generic_string() == "./nes/game.nes");

	bool contains = false;
	CHECK(removeCommonPath(rom, roms, contains).generic_string() == "nes/game.nes");
	CHECK(contains);

	// a path outside the folder is returned unchanged
	CHECK(makeRelativePath(rom, (root / "other").string(), false).generic_string() == rom);

	sfs::remove_all(root);
}

TEST_CASE("string_to_ptime parses gamelist and scraper formats", "[Util]")
{
	CHECK(boost::posix_time::to_iso_string(string_to_ptime("20140115T000000")) == "20140115T000000");
	CHECK(boost::posix_time::to_iso_string(string_to_ptime("01/15/2014", "%m/%d/%Y")) == "20140115T000000");
	CHECK(string_to_ptime("garbage").is_not_a_date_time());
}
