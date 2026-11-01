#include <cstdio>
#include "ThemeData.h"
#include "components/ImageComponent.h"
#include "components/TextComponent.h"

#include "theme/ThemeLoader.h"
#include "theme/ThemeSchema.h"
#include "theme/ThemeDiagnostics.h"
#include "theme/ThemeVariables.h"
#include "theme/ThemeCondition.h"

#include "utils/Paths.h"
#include "Log.h"
#include "Settings.h"
#include "renderer/Renderer.h"
#include "resources/ResourceManager.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace fs = std::filesystem;

ThemeData::ThemeData() : mVersion(0) {}

void ThemeData::loadFile(const std::string& path, const ThemeSystemInfo& system)
{
	es::theme::Diagnostics diag;
	// conditions such as "hd" depend on the real screen; 1920 x 1080 before Renderer::init
	const int width = Renderer::getScreenWidth() > 0 ? static_cast<int>(Renderer::getScreenWidth()) : 1920;
	const int height = Renderer::getScreenHeight() > 0 ? static_cast<int>(Renderer::getScreenHeight()) : 1080;
	const bool loaded = loadFile(path, diag, width, height, system);
	diag.log();
	if(!loaded)
	{
		ThemeException e;
		e.msg = "could not load theme";
		throw e;
	}
}

bool ThemeData::loadFile(const std::string& path, es::theme::Diagnostics& diag, int screenWidth, int screenHeight,
	const ThemeSystemInfo& system)
{
	mViews.clear();
	mPaths.clear();
	mPaths.push_back(path);

	// build loader context (docs/THEMES.md sections 5 and 6)
	es::theme::LoaderContext ctx;
	ctx.legacy.system = system.themeFolder;
	// "fr_FR" -> $language "fr", $country "FR"
	const std::string locale = Settings::getInstance()->getString("Language");
	const size_t underscore = locale.find('_');
	ctx.legacy.language = locale.substr(0, underscore);
	ctx.legacy.country = underscore == std::string::npos ? std::string() : locale.substr(underscore + 1);
	ctx.region = "eu";
	// ${root} is the root of the theme set (Recalbox ThemeManager.cpp:270): the folder above
	// <set>/<system>/theme.xml or <set>/default/theme.xml, else the folder of the file
	fs::path root = fs::path(path).parent_path();
	const std::string folder = root.filename().string();
	if(folder == "default" || (!system.themeFolder.empty() && folder == system.themeFolder))
		root = root.parent_path();
	ctx.variables.set("root", root.generic_string(), diag, {path, 0});
	if(!system.name.empty())
		ctx.variables.set("system.name", system.name, diag, {path, 0});
	if(!system.fullName.empty())
		ctx.variables.set("system", system.fullName, diag, {path, 0});
	es::theme::ConditionEnvironment env;
	env.screenWidth = screenWidth;
	env.screenHeight = screenHeight;
	env.systemName = system.name;
	ctx.conditions = es::theme::makeConditionResolver(env);

	auto loaded = es::theme::loadTheme(path, ctx, diag);

	if(loaded && loaded->formatVersion == 0.0f)
		diag.error({path, 0}, "theme has no <formatVersion>, theme ignored");
	if(!loaded || loaded->formatVersion == 0.0f)
	{
		mVersion = 0;
		return false;
	}

	mVersion = loaded->formatVersion;
	mPaths.clear();
	for(const auto& f : loaded->files) mPaths.push_back(f);

	// convert views
	for(const auto& [viewName, rawView] : loaded->views)
	{
		ThemeView view;
		for(const auto& rawElem : rawView.elements)
		{
			const es::theme::ElementSpec* spec = es::theme::findElement(rawElem.type);
			if(!spec)
				diag.warn(rawElem.where, "unknown element type \"" + rawElem.type + "\", element ignored");
			ThemeElement elem;
			elem.extra = rawElem.extra;
			elem.type = rawElem.type;
			for(const auto& rp : rawElem.properties)
			{
				if(spec)
				{
					const es::theme::PropertySpec* ps = es::theme::findProperty(*spec, rp.name);
					if(!ps)
						diag.warn(rp.where,
							"unknown property \"" + rp.name + "\" for element of type \"" + rawElem.type +
								"\", property ignored");
					if(ps)
					{
						auto parsed = es::theme::parseProperty(ps->type, rp.value, rp.file);
						if(!parsed.has_value())
							diag.warn(rp.where,
								"property \"" + rp.name + "\": " + parsed.error() + " (\"" + rp.value +
									"\"), property ignored");
						else if(ps->type == es::theme::PropertyType::Path)
						{
							const std::string& resolved = std::get<std::string>(*parsed);
							if(!resolved.empty() && !ResourceManager::getInstance()->fileExists(resolved))
								diag.warn(rp.where,
									"could not find file \"" + rp.value + "\" (resolved to \"" + resolved + "\")");
						}
						if(parsed.has_value())
						{
							// same alternatives, same order (ThemeSchema.h)
							std::visit([&](const auto& v) { elem.properties[rp.name] = v; }, *parsed);
						}
					}
				}
			}
			view.elements[rawElem.name] = elem;
			view.orderedKeys.push_back(rawElem.name);
		}
		mViews[viewName] = std::move(view);
	}
	return true;
}

const ThemeData::ThemeElement* ThemeData::getElement(const std::string& view, const std::string& element, const std::string& expectedType) const
{
	auto it = mViews.find(view);
	if(it == mViews.end()) return nullptr;
	auto eit = it->second.elements.find(element);
	if(eit == it->second.elements.end()) return nullptr;
	if(!expectedType.empty() && eit->second.type != expectedType)
	{
		Log::write(LogWarning, "requested mismatched theme type for [{}.{}] - expected \"{}\", got \"{}\"", view,
			element, expectedType, eit->second.type);
		return nullptr;
	}
	return &eit->second;
}

std::vector<std::unique_ptr<GuiComponent>> ThemeData::makeExtras(const std::shared_ptr<ThemeData>& theme, const std::string& view, Window* window)
{
	std::vector<std::unique_ptr<GuiComponent>> comps;
	if(!theme) return comps;

	auto vit = theme->mViews.find(view);
	if(vit == theme->mViews.end()) return comps;

	for(const auto& key : vit->second.orderedKeys)
	{
		auto eit = vit->second.elements.find(key);
		if(eit == vit->second.elements.end()) continue;
		const ThemeElement& elem = eit->second;
		if(!elem.extra) continue;

		std::unique_ptr<GuiComponent> comp;
		if(elem.type == "image")
			comp = std::make_unique<ImageComponent>(window);
		else if(elem.type == "text")
			comp = std::make_unique<TextComponent>(window);
		else
			continue;

		comp->applyTheme(theme, view, key, ThemeFlags::ALL);
		comps.push_back(std::move(comp));
	}

	return comps;
}

void ThemeExtras::setExtras(std::vector<std::unique_ptr<GuiComponent>> extras)
{
	mExtras = std::move(extras); // deletes the old extras, which detach themselves from this parent
	for(auto& extra : mExtras)
		addChild(extra.get()); // without this the extras are never drawn (system view background)
}

const std::shared_ptr<ThemeData>& ThemeData::getDefault()
{
	static std::shared_ptr<ThemeData> def = nullptr;
	if(def == nullptr)
	{
		def = std::make_shared<ThemeData>();
		if(const auto found = es::paths::find(es::paths::Kind::Config, "es_theme_default.xml"))
		{
			try
			{
				def->loadFile(found->string());
			}
			catch(ThemeException& e)
			{
				Log::write(LogError, "{}", e.what());
				def = std::make_shared<ThemeData>(); // reset to empty
			}
		}
	}
	return def;
}

std::map<std::string, ThemeSet> ThemeData::getThemeSets()
{
	std::map<std::string, ThemeSet> sets;
	std::vector<fs::path> paths = es::paths::systemDirs(es::paths::Kind::Data);
	std::reverse(paths.begin(), paths.end());
	paths.push_back(es::paths::userDir(es::paths::Kind::Data));
	fs::directory_iterator end;
	for(auto& path : paths)
	{
		path /= "themes";
		std::error_code ec;
		if(!fs::is_directory(path, ec)) continue;
		for(fs::directory_iterator it(path, ec); it != end; it.increment(ec))
		{
			if(fs::is_directory(*it))
			{
				ThemeSet set = {*it};
				sets[set.getName()] = set;
			}
		}
	}
	return sets;
}

fs::path ThemeData::getThemeFromCurrentSet(const std::string& system)
{
	auto themeSets = ThemeData::getThemeSets();
	if(themeSets.empty()) return "";
	auto set = themeSets.find(Settings::getInstance()->getString("ThemeSet"));
	if(set == themeSets.end())
	{
		set = themeSets.begin();
		Settings::getInstance()->setString("ThemeSet", set->first);
	}
	return set->second.getThemePath(system);
}
