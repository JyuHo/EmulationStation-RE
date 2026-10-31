#include "utils/Paths.h"

#include <catch2/catch_test_macros.hpp>

#include <sys/stat.h>

#include <cstdlib>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

namespace fs = std::filesystem;
using es::paths::Kind;

namespace {

// Sets environment variables for one test and restores them afterwards.
class EnvGuard
{
public:
	~EnvGuard()
	{
		for(auto it = mSaved.rbegin(); it != mSaved.rend(); ++it)
		{
			if(it->second)
				setenv(it->first.c_str(), it->second->c_str(), 1);
			else
				unsetenv(it->first.c_str());
		}
		es::paths::clearHome();
	}

	void set(const std::string& name, const std::optional<std::string>& value)
	{
		const char* old = std::getenv(name.c_str());
		mSaved.emplace_back(name, old ? std::optional<std::string>(old) : std::nullopt);
		if(value)
			setenv(name.c_str(), value->c_str(), 1);
		else
			unsetenv(name.c_str());
	}

private:
	std::vector<std::pair<std::string, std::optional<std::string>>> mSaved;
};

fs::path freshDir(const std::string& name)
{
	const fs::path dir = fs::temp_directory_path() / ("es-tests-paths-" + name);
	fs::remove_all(dir);
	fs::create_directories(dir);
	return dir;
}

void writeFile(const fs::path& path, const std::string& content)
{
	fs::create_directories(path.parent_path());
	std::ofstream(path) << content;
}

std::string readFile(const fs::path& path)
{
	std::ifstream in(path);
	std::stringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

void clearXdg(EnvGuard& env)
{
	for(const char* name :
		{"XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_STATE_HOME", "XDG_CACHE_HOME", "XDG_CONFIG_DIRS", "XDG_DATA_DIRS"})
		env.set(name, std::nullopt);
}

} // namespace

TEST_CASE("XDG defaults are derived from HOME", "[Paths]")
{
	EnvGuard env;
	clearXdg(env);
	const fs::path home = freshDir("defaults");
	env.set("HOME", home.string());

	CHECK(es::paths::homeDir() == home);
	CHECK(es::paths::userDir(Kind::Config) == home / ".config/emulationstation");
	CHECK(es::paths::userDir(Kind::Data) == home / ".local/share/emulationstation");
	CHECK(es::paths::userDir(Kind::State) == home / ".local/state/emulationstation");
	CHECK(es::paths::userDir(Kind::Cache) == home / ".cache/emulationstation");

	struct stat st
	{
	};
	REQUIRE(stat((home / ".config/emulationstation").c_str(), &st) == 0);
	CHECK((st.st_mode & 0777) == 0700);

	fs::remove_all(home);
}

TEST_CASE("absolute XDG variables win, empty and relative ones are ignored", "[Paths]")
{
	EnvGuard env;
	clearXdg(env);
	const fs::path home = freshDir("vars");
	env.set("HOME", home.string());
	env.set("XDG_CONFIG_HOME", (home / "cfg").string());
	env.set("XDG_DATA_HOME", "relative/data");
	env.set("XDG_STATE_HOME", "");

	CHECK(es::paths::userDir(Kind::Config) == home / "cfg/emulationstation");
	CHECK(es::paths::userDir(Kind::Data) == home / ".local/share/emulationstation");
	CHECK(es::paths::userDir(Kind::State) == home / ".local/state/emulationstation");
	CHECK(es::paths::userFile(Kind::Config, "es_settings.cfg") == home / "cfg/emulationstation/es_settings.cfg");

	fs::remove_all(home);
}

TEST_CASE("portable mode puts every user directory under one folder", "[Paths]")
{
	EnvGuard env;
	clearXdg(env);
	const fs::path home = freshDir("portable-home");
	const fs::path portable = freshDir("portable");
	env.set("HOME", home.string());
	env.set("XDG_CONFIG_HOME", (home / "cfg").string());

	es::paths::setHome(portable);
	CHECK(es::paths::hasHomeOverride());
	CHECK(es::paths::userDir(Kind::Config) == portable / "config/emulationstation");
	CHECK(es::paths::userDir(Kind::Data) == portable / "data/emulationstation");
	CHECK(es::paths::userDir(Kind::State) == portable / "state/emulationstation");
	CHECK(es::paths::userDir(Kind::Cache) == portable / "cache/emulationstation");
	// "~" in es_systems.cfg still means the real home
	CHECK(es::paths::expandUser("~/roms") == home / "roms");

	fs::remove_all(home);
	fs::remove_all(portable);
}

TEST_CASE("system directories follow XDG_*_DIRS and skip relative entries", "[Paths]")
{
	EnvGuard env;
	clearXdg(env);
	env.set("XDG_DATA_DIRS", "/one:relative::/two");
	env.set("XDG_CONFIG_DIRS", "/etc/a");

	const auto data = es::paths::systemDirs(Kind::Data);
	REQUIRE(data.size() >= 2);
	CHECK(data[0] == "/one/emulationstation");
	CHECK(data[1] == "/two/emulationstation");

	const auto config = es::paths::systemDirs(Kind::Config);
	REQUIRE(!config.empty());
	CHECK(config[0] == "/etc/a/emulationstation");

	CHECK(es::paths::systemDirs(Kind::State).empty());
	CHECK(es::paths::systemDirs(Kind::Cache).empty());
}

TEST_CASE("find prefers the user directory over system directories", "[Paths]")
{
	EnvGuard env;
	clearXdg(env);
	const fs::path root = freshDir("find");
	env.set("HOME", (root / "home").string());
	env.set("XDG_DATA_DIRS", (root / "sys").string());

	writeFile(root / "sys/emulationstation/themes/a/theme.xml", "system");
	REQUIRE(es::paths::find(Kind::Data, "themes/a/theme.xml") == root / "sys/emulationstation/themes/a/theme.xml");

	writeFile(root / "home/.local/share/emulationstation/themes/a/theme.xml", "user");
	CHECK(es::paths::find(Kind::Data, "themes/a/theme.xml") ==
		root / "home/.local/share/emulationstation/themes/a/theme.xml");
	CHECK(!es::paths::find(Kind::Data, "missing.xml").has_value());

	fs::remove_all(root);
}

TEST_CASE("expandUser", "[Paths]")
{
	EnvGuard env;
	env.set("HOME", "/home/tester");
	CHECK(es::paths::expandUser("~") == "/home/tester");
	CHECK(es::paths::expandUser("~/roms/nes") == "/home/tester/roms/nes");
	CHECK(es::paths::expandUser("/abs/path") == "/abs/path");
	CHECK(es::paths::expandUser("~other/x") == "~other/x");
	CHECK(es::paths::expandUser("./rel") == "./rel");
}

TEST_CASE("legacy ~/.emulationstation is copied once, without overwriting", "[Paths]")
{
	EnvGuard env;
	clearXdg(env);
	const fs::path home = freshDir("migration");
	env.set("HOME", home.string());
	const fs::path legacy = home / ".emulationstation";
	writeFile(legacy / "es_systems.cfg", "legacy systems");
	writeFile(legacy / "es_settings.cfg", "legacy settings");
	writeFile(legacy / "themes/simple/theme.xml", "theme");
	writeFile(legacy / "gamelists/nes/gamelist.xml", "gamelist");
	writeFile(legacy / "downloaded_images/nes/game-image.png", "png");
	writeFile(legacy / "es_log.txt", "old log");
	writeFile(home / ".config/emulationstation/es_settings.cfg", "new settings");

	const auto copied = es::paths::migrateFromLegacy();
	CHECK(copied.size() == 4);

	const fs::path config = home / ".config/emulationstation";
	const fs::path data = home / ".local/share/emulationstation";
	CHECK(readFile(config / "es_systems.cfg") == "legacy systems");
	CHECK(readFile(config / "es_settings.cfg") == "new settings");
	CHECK(readFile(data / "themes/simple/theme.xml") == "theme");
	CHECK(readFile(data / "gamelists/nes/gamelist.xml") == "gamelist");
	CHECK(readFile(data / "downloaded_images/nes/game-image.png") == "png");
	CHECK(!fs::exists(home / ".local/state/emulationstation/es_log.txt"));

	// the old folder is kept, with an explanation
	CHECK(readFile(legacy / "es_systems.cfg") == "legacy systems");
	CHECK(fs::exists(legacy / "MIGRATED_TO_XDG.txt"));

	// second run: nothing to do
	writeFile(legacy / "es_input.cfg", "late input");
	CHECK(es::paths::migrateFromLegacy().empty());
	CHECK(!fs::exists(config / "es_input.cfg"));

	fs::remove_all(home);
}

TEST_CASE("migration is skipped in portable mode", "[Paths]")
{
	EnvGuard env;
	clearXdg(env);
	const fs::path home = freshDir("migration-portable");
	const fs::path portable = freshDir("migration-portable-dir");
	env.set("HOME", home.string());
	writeFile(home / ".emulationstation/es_systems.cfg", "legacy");

	es::paths::setHome(portable);
	CHECK(es::paths::migrateFromLegacy().empty());
	CHECK(!fs::exists(portable / "config/emulationstation/es_systems.cfg"));
	CHECK(!fs::exists(home / ".emulationstation/MIGRATED_TO_XDG.txt"));

	fs::remove_all(home);
	fs::remove_all(portable);
}
