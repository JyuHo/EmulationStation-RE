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

void ThemeData::loadFile(const std::string& path)
{
	mViews.clear();
	mPaths.clear();
	mPaths.push_back(path);

	// build loader context
	es::theme::LoaderContext ctx;
	ctx.legacy.system = "";
	ctx.legacy.language = Settings::getInstance()->getString("Language");
	ctx.region = "eu";
	es::theme::Diagnostics diag;
	ctx.variables.set("root", fs::path(path).parent_path().generic_string(), diag, {path, 0});
	es::theme::ConditionEnvironment env;
	// conditions such as "hd" depend on the real screen; 1920 x 1080 before Renderer::init
	env.screenWidth = Renderer::getScreenWidth() > 0 ? static_cast<int>(Renderer::getScreenWidth()) : 1920;
	env.screenHeight = Renderer::getScreenHeight() > 0 ? static_cast<int>(Renderer::getScreenHeight()) : 1080;
	env.systemName = "";
	ctx.conditions = es::theme::makeConditionResolver(env);

	auto loaded = es::theme::loadTheme(path, ctx, diag);

	if(!loaded || loaded->formatVersion == 0.0f)
	{
		diag.log();
		ThemeException e;
		e.msg = "could not load theme";
		throw e;
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
							// convert PropertyValue to variant used by ThemeElement
							std::visit([&](auto&& v) {
								using T = std::decay_t<decltype(v)>;
								if constexpr (std::is_same_v<T, glm::vec2>)
									elem.properties[rp.name] = v;
								else if constexpr (std::is_same_v<T, std::string>)
									elem.properties[rp.name] = v;
								else if constexpr (std::is_same_v<T, unsigned int>)
									elem.properties[rp.name] = v;
								else if constexpr (std::is_same_v<T, float>)
									elem.properties[rp.name] = v;
								else if constexpr (std::is_same_v<T, bool>)
									elem.properties[rp.name] = v;
								else if constexpr (std::is_same_v<T, es::theme::PathList>)
									elem.properties[rp.name] = v.paths.empty() ? std::string() : v.paths.front();
							}, *parsed);
						}
					}
				}
			}
			view.elements[rawElem.name] = elem;
			view.orderedKeys.push_back(rawElem.name);
		}
		mViews[viewName] = std::move(view);
	}
	diag.log();
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
