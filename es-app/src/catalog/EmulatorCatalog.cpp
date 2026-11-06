#include "catalog/EmulatorCatalog.h"

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include <algorithm>
#include <sys/utsname.h>

namespace catalog
{
	namespace
	{
		std::string str(const rapidjson::Value& obj, const char* name)
		{
			if (obj.IsObject())
			{
				auto it = obj.FindMember(name);
				if (it != obj.MemberEnd() && it->value.IsString())
					return std::string(it->value.GetString(), it->value.GetStringLength());
			}
			return "";
		}

		uint64_t uint(const rapidjson::Value& obj, const char* name)
		{
			auto it = obj.FindMember(name);
			if (it != obj.MemberEnd() && it->value.IsUint64())
				return it->value.GetUint64();
			return 0;
		}

		std::vector<std::string> strings(const rapidjson::Value& obj, const char* name)
		{
			std::vector<std::string> ret;
			auto it = obj.FindMember(name);
			if (it != obj.MemberEnd() && it->value.IsArray())
				for (auto& v : it->value.GetArray())
					if (v.IsString())
						ret.push_back(v.GetString());
			return ret;
		}

		bool isHttps(const std::string& url) { return url.rfind("https://", 0) == 0; }

		// A logo may be relative to catalog.json, like the theme screenshots of the theme catalog
		std::string resolveUrl(const std::string& url, const std::string& base)
		{
			if (url.empty() || url.find("://") != std::string::npos)
				return isHttps(url) ? url : "";

			auto slash = base.rfind('/');
			if (slash == std::string::npos || url.find("..") != std::string::npos)
				return "";

			return base.substr(0, slash + 1) + url;
		}

		std::optional<Build> parseBuild(const rapidjson::Value& b)
		{
			if (!b.IsObject())
				return std::nullopt;

			Build build;
			auto type = str(b, "type");
			if (type == "flatpak")
			{
				build.type = BuildType::Flatpak;
				build.ref = str(b, "ref");
				build.remote = str(b, "remote");
				if (build.remote.empty())
					build.remote = "flathub";
				if (!isSafeId(build.ref) || !isSafeId(build.remote))
					return std::nullopt;
				return build;
			}

			if (type == "appimage")
				build.type = BuildType::AppImage;
			else if (type == "archive")
				build.type = BuildType::Archive;
			else
				return std::nullopt; // a type added after this version of ES

			build.url = str(b, "url");
			build.version = str(b, "version");
			build.sha256 = str(b, "sha256");
			build.size = uint(b, "size");
			build.exec = str(b, "exec");

			// Never install something that cannot be checked
			if (!isHttps(build.url) || build.sha256.size() != 64)
				return std::nullopt;
			if (build.type == BuildType::Archive && build.exec.empty())
				return std::nullopt;

			return build;
		}

		void writeStrings(rapidjson::PrettyWriter<rapidjson::StringBuffer>& w, const char* key, const std::vector<std::string>& values)
		{
			w.Key(key);
			w.StartArray();
			for (auto& v : values)
				w.String(v.c_str());
			w.EndArray();
		}

		void parseItems(const rapidjson::Value& doc, const char* key, std::map<std::string, InstalledItem>& out)
		{
			auto it = doc.FindMember(key);
			if (it == doc.MemberEnd() || !it->value.IsObject())
				return;

			for (auto& m : it->value.GetObject())
			{
				if (!m.value.IsObject() || !isSafeId(m.name.GetString()))
					continue;

				InstalledItem item;
				item.type = str(m.value, "type");
				item.version = str(m.value, "version");
				item.path = str(m.value, "path");
				item.ref = str(m.value, "ref");
				item.command = strings(m.value, "command");
				out[m.name.GetString()] = item;
			}
		}

		void writeItems(rapidjson::PrettyWriter<rapidjson::StringBuffer>& w, const char* key, const std::map<std::string, InstalledItem>& items)
		{
			w.Key(key);
			w.StartObject();
			for (auto& [id, item] : items)
			{
				w.Key(id.c_str());
				w.StartObject();
				w.Key("type"); w.String(item.type.c_str());
				w.Key("version"); w.String(item.version.c_str());
				if (!item.path.empty()) { w.Key("path"); w.String(item.path.c_str()); }
				if (!item.ref.empty()) { w.Key("ref"); w.String(item.ref.c_str()); }
				if (!item.command.empty()) writeStrings(w, "command", item.command);
				w.EndObject();
			}
			w.EndObject();
		}
	}

	bool isSafeId(const std::string& id)
	{
		if (id.empty() || id.size() > 128 || id[0] == '.' || id[0] == '-')
			return false;

		return std::all_of(id.begin(), id.end(), [](char c)
		{
			return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
		});
	}

	std::string typeName(BuildType type)
	{
		switch (type)
		{
		case BuildType::AppImage: return "appimage";
		case BuildType::Archive: return "archive";
		case BuildType::Flatpak: return "flatpak";
		}
		return "";
	}

	std::string hostArchitecture()
	{
		struct utsname name;
		if (uname(&name) != 0)
			return "";

		std::string arch = name.machine;
		if (arch == "arm64")
			return "aarch64";
		if (arch == "amd64")
			return "x86_64";
		return arch;
	}

	const Emulator* Catalog::findEmulator(const std::string& id) const
	{
		for (auto& e : emulators)
			if (e.id == id)
				return &e;
		return nullptr;
	}

	const Core* Catalog::findCore(const std::string& id) const
	{
		for (auto& c : cores)
			if (c.id == id)
				return &c;
		return nullptr;
	}

	std::optional<Catalog> parseCatalog(const std::string& json, const std::string& arch, const std::string& catalogUrl, std::string* error)
	{
		auto fail = [error](const std::string& why) -> std::optional<Catalog>
		{
			if (error)
				*error = why;
			return std::nullopt;
		};

		rapidjson::Document doc;
		doc.Parse(json.c_str(), json.size());
		if (doc.HasParseError() || !doc.IsObject())
			return fail("not a JSON object");

		auto format = doc.FindMember("format");
		if (format == doc.MemberEnd() || !format->value.IsInt())
			return fail("no format");
		if (format->value.GetInt() != FORMAT)
			return fail("unsupported catalog format " + std::to_string(format->value.GetInt()));

		Catalog catalog;

		auto standalone = doc.FindMember("standalone");
		if (standalone != doc.MemberEnd() && standalone->value.IsArray())
		{
			for (auto& e : standalone->value.GetArray())
			{
				if (!e.IsObject())
					continue;

				Emulator emu;
				emu.id = str(e, "id");
				emu.name = str(e, "name");
				if (!isSafeId(emu.id) || emu.name.empty() || catalog.findEmulator(emu.id))
					continue;

				emu.description = str(e, "description");
				emu.homepage = str(e, "homepage");
				emu.license = str(e, "license");
				emu.logo = resolveUrl(str(e, "logo"), catalogUrl);
				emu.systems = strings(e, "systems");
				emu.binaries = strings(e, "binaries");
				emu.binaries.erase(std::remove_if(emu.binaries.begin(), emu.binaries.end(), [](const std::string& b) { return !isSafeId(b); }), emu.binaries.end());

				auto builds = e.FindMember("builds");
				if (builds != e.MemberEnd() && builds->value.IsObject())
				{
					auto forArch = builds->value.FindMember(arch.c_str());
					if (forArch != builds->value.MemberEnd() && forArch->value.IsArray())
						for (auto& b : forArch->value.GetArray())
							if (auto build = parseBuild(b))
								emu.builds.push_back(*build);
				}

				if (!emu.builds.empty())
					catalog.emulators.push_back(std::move(emu));
			}
		}

		auto cores = doc.FindMember("cores");
		if (cores != doc.MemberEnd() && cores->value.IsObject())
		{
			auto info = str(cores->value, "info");
			if (isHttps(info))
				catalog.coreInfoUrl = info;

			auto items = cores->value.FindMember("items");
			if (items != cores->value.MemberEnd() && items->value.IsArray())
			{
				for (auto& c : items->value.GetArray())
				{
					if (!c.IsObject())
						continue;

					Core core;
					core.id = str(c, "id");
					core.name = str(c, "name");
					if (!isSafeId(core.id) || core.name.empty() || catalog.findCore(core.id))
						continue;

					core.description = str(c, "description");
					core.systems = strings(c, "systems");

					auto builds = c.FindMember("builds");
					if (builds == c.MemberEnd() || !builds->value.IsObject())
						continue;

					auto forArch = builds->value.FindMember(arch.c_str());
					if (forArch == builds->value.MemberEnd() || !forArch->value.IsObject())
						continue;

					core.url = str(forArch->value, "url");
					core.version = str(forArch->value, "version");
					if (isHttps(core.url))
						catalog.cores.push_back(std::move(core));
				}
			}
		}

		auto byName = [](const auto& a, const auto& b) { return a.name < b.name; };
		std::stable_sort(catalog.emulators.begin(), catalog.emulators.end(), byName);
		std::stable_sort(catalog.cores.begin(), catalog.cores.end(), byName);

		return catalog;
	}

	const Build* preferredBuild(const Emulator& emulator, bool flatpakAvailable)
	{
		for (auto& b : emulator.builds)
			if (b.type != BuildType::Flatpak || flatpakAvailable)
				return &b;
		return nullptr;
	}

	State emulatorState(const Emulator& emulator, const Installed& installed, bool flatpakAvailable, bool foundInPath)
	{
		auto it = installed.emulators.find(emulator.id);
		if (it == installed.emulators.end())
			return foundInPath ? State::System : State::Available;

		// Flatpak updates its own applications
		if (it->second.type == "flatpak")
			return State::Installed;

		auto build = preferredBuild(emulator, flatpakAvailable);
		if (build && typeName(build->type) == it->second.type && build->version != it->second.version)
			return State::UpdateAvailable;

		return State::Installed;
	}

	State coreState(const Core& core, const Installed& installed, bool foundInSystem)
	{
		auto it = installed.cores.find(core.id);
		if (it == installed.cores.end())
			return foundInSystem ? State::System : State::Available;

		return it->second.version != core.version ? State::UpdateAvailable : State::Installed;
	}

	Installed Installed::parse(const std::string& json)
	{
		Installed ret;

		rapidjson::Document doc;
		doc.Parse(json.c_str(), json.size());
		if (doc.HasParseError() || !doc.IsObject())
			return ret;

		parseItems(doc, "emulators", ret.emulators);
		parseItems(doc, "cores", ret.cores);
		return ret;
	}

	std::string Installed::toJson() const
	{
		rapidjson::StringBuffer buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> w(buffer);
		w.SetIndent(' ', 2);

		w.StartObject();
		w.Key("format"); w.Int(FORMAT);
		writeItems(w, "emulators", emulators);
		writeItems(w, "cores", cores);
		w.EndObject();

		return std::string(buffer.GetString(), buffer.GetSize()) + "\n";
	}
}
