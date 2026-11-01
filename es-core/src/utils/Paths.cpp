#include "utils/Paths.h"

#include "Paths.config.h"

#include <pwd.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace es::paths {

namespace {

constexpr std::string_view kAppDir = "emulationstation";
constexpr std::string_view kLegacyDir = ".emulationstation";
constexpr std::string_view kMigrationMarker = "MIGRATED_TO_XDG.txt";

fs::path gHomeOverride;

// The XDG specification says relative values must be ignored.
std::optional<fs::path> absoluteEnv(const char* name)
{
	const char* value = std::getenv(name);
	if(value == nullptr || value[0] != '/')
		return std::nullopt;
	return fs::path(value);
}

std::vector<fs::path> splitDirList(const char* name, std::string_view fallback)
{
	const char* env = std::getenv(name);
	std::string_view list = (env != nullptr && env[0] != '\0') ? std::string_view(env) : fallback;

	std::vector<fs::path> dirs;
	while(!list.empty())
	{
		const size_t colon = list.find(':');
		const std::string_view entry = list.substr(0, colon);
		if(!entry.empty() && entry.front() == '/')
			dirs.emplace_back(fs::path(entry) / kAppDir);
		if(colon == std::string_view::npos)
			break;
		list.remove_prefix(colon + 1);
	}
	return dirs;
}

std::string_view portableSubdir(Kind kind)
{
	switch(kind)
	{
	case Kind::Config: return "config";
	case Kind::Data: return "data";
	case Kind::State: return "state";
	case Kind::Cache: return "cache";
	}
	return "data";
}

fs::path userBase(Kind kind)
{
	if(!gHomeOverride.empty())
		return gHomeOverride / portableSubdir(kind) / kAppDir;

	const fs::path home = homeDir();
	switch(kind)
	{
	case Kind::Config: return absoluteEnv("XDG_CONFIG_HOME").value_or(home / ".config") / kAppDir;
	case Kind::Data: return absoluteEnv("XDG_DATA_HOME").value_or(home / ".local/share") / kAppDir;
	case Kind::State: return absoluteEnv("XDG_STATE_HOME").value_or(home / ".local/state") / kAppDir;
	case Kind::Cache: return absoluteEnv("XDG_CACHE_HOME").value_or(home / ".cache") / kAppDir;
	}
	return home / kAppDir;
}

void appendUnique(std::vector<fs::path>& dirs, const fs::path& dir)
{
	if(std::find(dirs.begin(), dirs.end(), dir) == dirs.end())
		dirs.push_back(dir);
}

std::optional<fs::path> executableShareDir()
{
	const auto exeDir = executableDir();
	if(!exeDir)
		return std::nullopt;
	return (*exeDir / ".." / "share" / kAppDir).lexically_normal();
}

void createPrivateDirectories(const fs::path& dir)
{
	std::error_code ec;
	if(fs::is_directory(dir, ec))
		return;
	createPrivateDirectories(dir.parent_path());
	if(fs::create_directory(dir, ec))
		fs::permissions(dir, fs::perms::owner_all, fs::perm_options::replace, ec);
}

void copyMissing(const fs::path& from, const fs::path& to, std::vector<fs::path>& copied)
{
	std::error_code ec;
	if(fs::is_regular_file(from, ec))
	{
		if(fs::exists(to, ec))
			return;
		createPrivateDirectories(to.parent_path());
		if(fs::copy_file(from, to, fs::copy_options::skip_existing, ec))
			copied.push_back(to);
		return;
	}

	if(!fs::is_directory(from, ec))
		return;
	for(const auto& entry : fs::recursive_directory_iterator(from, fs::directory_options::skip_permission_denied, ec))
	{
		if(!entry.is_regular_file(ec))
			continue;
		const fs::path target = to / fs::relative(entry.path(), from, ec);
		if(ec || fs::exists(target, ec))
			continue;
		createPrivateDirectories(target.parent_path());
		if(fs::copy_file(entry.path(), target, fs::copy_options::skip_existing, ec))
			copied.push_back(target);
	}
}

} // namespace

std::optional<fs::path> executableDir()
{
	std::error_code ec;
	const fs::path exe = fs::read_symlink("/proc/self/exe", ec);
	if(ec)
		return std::nullopt;
	return exe.parent_path();
}

fs::path homeDir()
{
	if(auto home = absoluteEnv("HOME"))
		return *home;
	if(const passwd* pw = getpwuid(getuid()); pw != nullptr && pw->pw_dir != nullptr)
		return fs::path(pw->pw_dir);
	return fs::path("/");
}

fs::path userDir(Kind kind)
{
	fs::path dir = userBase(kind);
	createPrivateDirectories(dir);
	return dir;
}

fs::path userFile(Kind kind, std::string_view rel)
{
	return userDir(kind) / rel;
}

std::vector<fs::path> systemDirs(Kind kind)
{
	std::vector<fs::path> dirs;
	switch(kind)
	{
	case Kind::Config:
		dirs = splitDirList("XDG_CONFIG_DIRS", "/etc/xdg");
		appendUnique(dirs, ES_INSTALL_SYSCONFDIR);
		break;
	case Kind::Data:
		dirs = splitDirList("XDG_DATA_DIRS", "/usr/local/share:/usr/share");
		appendUnique(dirs, ES_INSTALL_DATADIR);
		if(auto shareDir = executableShareDir())
			appendUnique(dirs, *shareDir);
		break;
	case Kind::State:
	case Kind::Cache: break;
	}
	return dirs;
}

std::optional<fs::path> find(Kind kind, std::string_view rel)
{
	std::error_code ec;
	if(fs::path user = userBase(kind) / rel; fs::exists(user, ec))
		return user;
	for(const auto& dir : systemDirs(kind))
	{
		if(fs::path candidate = dir / rel; fs::exists(candidate, ec))
			return candidate;
	}
	return std::nullopt;
}

fs::path expandUser(std::string_view path)
{
	if(path == "~")
		return homeDir();
	if(path.starts_with("~/"))
		return homeDir() / path.substr(2);
	return fs::path(path);
}

void setHome(const fs::path& home)
{
	gHomeOverride = fs::absolute(home);
}

void clearHome()
{
	gHomeOverride.clear();
}

bool hasHomeOverride()
{
	return !gHomeOverride.empty();
}

std::vector<fs::path> migrateFromLegacy()
{
	std::vector<fs::path> copied;
	if(hasHomeOverride())
		return copied;

	const fs::path legacy = homeDir() / kLegacyDir;
	std::error_code ec;
	if(!fs::is_directory(legacy, ec) || fs::exists(legacy / kMigrationMarker, ec))
		return copied;

	for(std::string_view name : {"es_settings.cfg", "es_input.cfg", "es_systems.cfg", "es_theme_default.xml"})
		copyMissing(legacy / name, userBase(Kind::Config) / name, copied);
	for(std::string_view name : {"themes", "gamelists", "downloaded_images"})
		copyMissing(legacy / name, userBase(Kind::Data) / name, copied);

	std::ofstream marker(legacy / kMigrationMarker);
	marker << "EmulationStation-RE copied this folder to the XDG directories:\n"
		   << "  configuration: " << userBase(Kind::Config).string() << "\n"
		   << "  data (themes, gamelists, downloaded images): " << userBase(Kind::Data).string() << "\n"
		   << "  log: " << userBase(Kind::State).string() << "\n"
		   << "This folder was left untouched. gamelist.xml files may still point to images in it.\n";
	return copied;
}

} // namespace es::paths
