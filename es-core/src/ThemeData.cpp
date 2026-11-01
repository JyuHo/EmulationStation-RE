#include <cstdio>
#include "ThemeData.h"

#include "theme/ThemeLoader.h"
#include "theme/ThemeSchema.h"
#include "theme/ThemeDiagnostics.h"
#include "theme/ThemeVariables.h"
#include "theme/ThemeCondition.h"

#include "utils/Paths.h"
#include "Log.h"
#include "Settings.h"

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
	env.screenWidth = 1920;
	env.screenHeight = 1080;
	env.systemName = "";
	ctx.conditions = es::theme::makeConditionResolver(env);

	auto loaded = es::theme::loadTheme(path, ctx, diag);
	diag.log();

	if(!loaded || loaded->formatVersion == 0.0f)
	{
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
			ThemeElement elem;
			elem.extra = rawElem.extra;
			elem.type = rawElem.type;
			for(const auto& rp : rawElem.properties)
			{
				if(spec)
				{
					const es::theme::PropertySpec* ps = es::theme::findProperty(*spec, rp.name);
					if(ps)
					{
						auto parsed = es::theme::parseProperty(ps->type, rp.value, rp.file);
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
}

const ThemeData::ThemeElement* ThemeData::getElement(const std::string& view, const std::string& element, const std::string& expectedType) const
{
	auto it = mViews.find(view);
	if(it == mViews.end()) return nullptr;
	auto eit = it->second.elements.find(element);
	if(eit == it->second.elements.end()) return nullptr;
	return &eit->second;
}

std::vector<std::unique_ptr<GuiComponent>> ThemeData::makeExtras(const std::shared_ptr<ThemeData>& theme, const std::string& view, Window* window)
{
	return {};
}

void ThemeExtras::setExtras(std::vector<std::unique_ptr<GuiComponent>> extras)
{
	mExtras = std::move(extras);
}

const std::shared_ptr<ThemeData>& ThemeData::getDefault()
{
	static std::shared_ptr<ThemeData> def = std::make_shared<ThemeData>();
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
