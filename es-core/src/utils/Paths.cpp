#include "Paths.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

namespace {
fs::path gHomeOverride;

std::string envOrEmpty(const char* name)
{
	const char* v = std::getenv(name);
	return v ? std::string(v) : std::string();
}

bool isAbsoluteOrValid(const std::string& v)
{
	if (v.empty()) return false;
	if (v[0] != '/') return false;
	return true;
}

fs::path homeDir()
{
	if (!gHomeOverride.empty()) return gHomeOverride;
	std::string h = envOrEmpty("HOME");
	if (!isAbsoluteOrValid(h)) {
		// fallback via getpwuid
		// (kept simple: empty)
		h.clear();
	}
	if (h.empty()) {
		// last resort
		h = "/";
	}
	return fs::path(h);
}

fs::path xdgOr(const char* var, const fs::path& def)
{
	std::string v = envOrEmpty(var);
	if (isAbsoluteOrValid(v)) return fs::path(v);
	return def;
}

const char* subdir(es::paths::Kind k)
{
	switch (k) {
	case es::paths::Kind::Config: return "config";
	case es::paths::Kind::Data: return "data";
	case es::paths::Kind::State: return "state";
	case es::paths::Kind::Cache: return "cache";
	}
	return "data";
}

fs::path userBase(es::paths::Kind k)
{
	if (!gHomeOverride.empty()) {
		return gHomeOverride / subdir(k) / "emulationstation";
	}
	fs::path home = homeDir();
	fs::path base;
	switch (k) {
	case es::paths::Kind::Config: base = xdgOr("XDG_CONFIG_HOME", home / ".config"); break;
	case es::paths::Kind::Data: base = xdgOr("XDG_DATA_HOME", home / ".local/share"); break;
	case es::paths::Kind::State: base = xdgOr("XDG_STATE_HOME", home / ".local/state"); break;
	case es::paths::Kind::Cache: base = xdgOr("XDG_CACHE_HOME", home / ".cache"); break;
	}
	return base / "emulationstation";
}
} // namespace

namespace es::paths {

void setHome(const fs::path& home) { gHomeOverride = home; }
void clearHome() { gHomeOverride.clear(); }

fs::path userDir(es::paths::Kind kind)
{
	fs::path p = userBase(kind);
	std::error_code ec;
	fs::create_directories(p, ec);
	return p;
}

fs::path userFile(es::paths::Kind kind, std::string_view rel)
{
	return userDir(kind) / std::string(rel);
}

std::vector<fs::path> systemDirs(es::paths::Kind kind)
{
	std::vector<fs::path> out;
	// XDG config/data dirs
	if (kind == es::paths::Kind::Config) {
		std::string v = envOrEmpty("XDG_CONFIG_DIRS");
		if (v.empty()) v = "/etc/xdg";
		// split
		std::string cur;
		for (char c : v) {
			if (c == ':') { if (!cur.empty() && cur[0]=='/') out.push_back(fs::path(cur)/"emulationstation"); cur.clear(); }
			else cur.push_back(c);
		}
		if (!cur.empty() && cur[0]=='/') out.push_back(fs::path(cur)/"emulationstation");
	} else if (kind == es::paths::Kind::Data) {
		std::string v = envOrEmpty("XDG_DATA_DIRS");
		if (v.empty()) v = "/usr/local/share:/usr/share";
		std::string cur;
		for (char c : v) {
			if (c == ':') { if (!cur.empty() && cur[0]=='/') out.push_back(fs::path(cur)/"emulationstation"); cur.clear(); }
			else cur.push_back(c);
		}
		if (!cur.empty() && cur[0]=='/') out.push_back(fs::path(cur)/"emulationstation");
	}
	return out;
}

std::optional<fs::path> find(es::paths::Kind kind, std::string_view rel)
{
	// user first
	fs::path u = userBase(kind) / std::string(rel);
	std::error_code ec;
	if (fs::exists(u, ec)) return u;
	for (const auto& d : systemDirs(kind)) {
		fs::path p = d / std::string(rel);
		if (fs::exists(p, ec)) return p;
	}
	return std::nullopt;
}

fs::path expandUser(std::string_view path)
{
	std::string s(path);
	if (s == "~") {
		return homeDir();
	}
	if (s.rfind("~/", 0) == 0) {
		return homeDir() / s.substr(2);
	}
	return fs::path(s);
}

void migrateFromLegacy()
{
	fs::path home = homeDir();
	fs::path legacy = home / ".emulationstation";
	std::error_code ec;
	if (!fs::exists(legacy, ec)) return;
	// only if new dirs empty-ish; do copy
	// copy known files
	auto copyIf = [&](const char* name, es::paths::Kind k) {
		fs::path src = legacy / name;
		if (!fs::exists(src, ec)) return;
		fs::path dst = userFile(k, name);
		if (fs::exists(dst, ec)) return;
		fs::copy_file(src, dst, fs::copy_options::skip_existing, ec);
	};
	copyIf("es_settings.cfg", es::paths::Kind::Config);
	copyIf("es_input.cfg", es::paths::Kind::Config);
	copyIf("es_systems.cfg", es::paths::Kind::Config);
	copyIf("es_theme_default.xml", es::paths::Kind::Config);
	copyIf("gamecontrollerdb.txt", es::paths::Kind::Config);
	// log
	copyIf("es_log.txt", es::paths::Kind::State);
	// marker
	std::ofstream(legacy / "MIGRATED_TO_XDG.txt") << "Migrated to XDG dirs.\n";
}

} // namespace es::paths