#include "ThemeData.h"
#include "utils/Paths.h"
#include <algorithm>
#include "Renderer.h"
#include "resources/Font.h"
#include "Sound.h"
#include "resources/TextureResource.h"
#include "Log.h"
#include "Settings.h"
#include "Util.h"
#include "pugixml/pugixml.hpp"

#include "components/ImageComponent.h"
#include "components/TextComponent.h"

using ElementMapType = std::map<std::string, ThemeData::ElementPropertyType>;

std::map<std::string, ElementMapType> ThemeData::sElementMap = {
	{"image",
	 {{"pos", NORMALIZED_PAIR},
	  {"size", NORMALIZED_PAIR},
	  {"maxSize", NORMALIZED_PAIR},
	  {"origin", NORMALIZED_PAIR},
	  {"path", PATH},
	  {"tile", BOOLEAN},
	  {"color", COLOR}}},
	{"text",
	 {{"pos", NORMALIZED_PAIR},
	  {"size", NORMALIZED_PAIR},
	  {"text", STRING},
	  {"color", COLOR},
	  {"fontPath", PATH},
	  {"fontSize", FLOAT},
	  {"alignment", STRING},
	  {"forceUppercase", BOOLEAN},
	  {"lineSpacing", FLOAT}}},
	{"textlist",
	 {{"pos", NORMALIZED_PAIR},
	  {"size", NORMALIZED_PAIR},
	  {"selectorColor", COLOR},
	  {"selectedColor", COLOR},
	  {"primaryColor", COLOR},
	  {"secondaryColor", COLOR},
	  {"fontPath", PATH},
	  {"fontSize", FLOAT},
	  {"scrollSound", PATH},
	  {"alignment", STRING},
	  {"horizontalMargin", FLOAT},
	  {"forceUppercase", BOOLEAN},
	  {"lineSpacing", FLOAT}}},
	{"container", {{"pos", NORMALIZED_PAIR}, {"size", NORMALIZED_PAIR}}},
	{"ninepatch", {{"pos", NORMALIZED_PAIR}, {"size", NORMALIZED_PAIR}, {"path", PATH}}},
	{"datetime",
	 {{"pos", NORMALIZED_PAIR},
	  {"size", NORMALIZED_PAIR},
	  {"color", COLOR},
	  {"fontPath", PATH},
	  {"fontSize", FLOAT},
	  {"forceUppercase", BOOLEAN}}},
	{"rating", {{"pos", NORMALIZED_PAIR}, {"size", NORMALIZED_PAIR}, {"filledPath", PATH}, {"unfilledPath", PATH}}},
	{"sound", {{"path", PATH}}},
	{"helpsystem",
	 {{"pos", NORMALIZED_PAIR},
	  {"textColor", COLOR},
	  {"iconColor", COLOR},
	  {"fontPath", PATH},
	  {"fontSize", FLOAT}}}};

namespace fs = std::filesystem;

#define MINIMUM_THEME_FORMAT_VERSION 3
#define CURRENT_THEME_FORMAT_VERSION 3

// helper
unsigned int getHexColor(const char* str)
{
	ThemeException error;
	if(!str)
		throw error << "Empty color";

	size_t len = strlen(str);
	if(len != 6 && len != 8)
		throw error << "Invalid color (bad length, \"" << str << "\" - must be 6 or 8)";

	unsigned int val;
	std::stringstream ss;
	ss << str;
	ss >> std::hex >> val;

	if(len == 6)
		val = (val << 8) | 0xFF;

	return val;
}

// helper
std::string resolvePath(const char* in, const fs::path& relative)
{
	if(!in || in[0] == '\0')
		return in;

	fs::path relPath = relative.parent_path();

	std::filesystem::path path(in);

	// we use filesystem here instead of just string checks because
	// some directories could theoretically start with ~ or .
	if(*path.begin() == "~")
	{
		path = es::paths::expandUser(in);
	}
	else if(*path.begin() == ".")
	{
		// std::filesystem's operator/ replaces the left side when the right side is absolute, so
		// append the elements after "." one by one ("./art/x.png" must not become "/art/x.png")
		fs::path resolved = relPath;
		for(auto it = std::next(path.begin()); it != path.end(); ++it)
			resolved /= *it;
		path = resolved;
	}

	return path.generic_string();
}

ThemeData::ThemeData()
{
	mVersion = 0;
}

void ThemeData::loadFile(const std::string& path)
{
	mPaths.emplace_back(path);

	ThemeException error;
	error.setFiles(mPaths);

	if(!fs::exists(path))
		throw error << "File does not exist!";

	mVersion = 0;
	mViews.clear();

	pugi::xml_document doc;
	pugi::xml_parse_result res = doc.load_file(path.c_str());
	if(!res)
		throw error << "XML parsing error: \n    " << res.description();

	pugi::xml_node root = doc.child("theme");
	if(!root)
		throw error << "Missing <theme> tag!";

	// parse version
	mVersion = root.child("formatVersion").text().as_float(-404);
	if(mVersion == -404)
		throw error << "<formatVersion> tag missing!\n   It's either out of date or you need to add <formatVersion>"
					<< CURRENT_THEME_FORMAT_VERSION << "</formatVersion> inside your <theme> tag.";

	if(mVersion < MINIMUM_THEME_FORMAT_VERSION)
		throw error << "Theme uses format version " << mVersion << ". Minimum supported version is "
					<< MINIMUM_THEME_FORMAT_VERSION << ".";

	parseIncludes(root);
	parseViews(root);
}

void ThemeData::parseIncludes(const pugi::xml_node& root)
{
	ThemeException error;
	error.setFiles(mPaths);

	for(pugi::xml_node node = root.child("include"); node; node = node.next_sibling("include"))
	{
		const char* relPath = node.text().get();
		std::string path = resolvePath(relPath, mPaths.back());
		if(!ResourceManager::getInstance()->fileExists(path))
			throw error << "Included file \"" << relPath << "\" not found! (resolved to \"" << path << "\")";

		error << "    from included file \"" << relPath << "\":\n    ";

		mPaths.emplace_back(path);

		pugi::xml_document includeDoc;
		pugi::xml_parse_result result = includeDoc.load_file(path.c_str());
		if(!result)
			throw error << "Error parsing file: \n    " << result.description();

		pugi::xml_node root = includeDoc.child("theme");
		if(!root)
			throw error << "Missing <theme> tag!";

		parseIncludes(root);
		parseViews(root);

		mPaths.pop_back();
	}
}

void ThemeData::parseViews(const pugi::xml_node& root)
{
	ThemeException error;
	error.setFiles(mPaths);

	// parse views
	for(pugi::xml_node node = root.child("view"); node; node = node.next_sibling("view"))
	{
		if(!node.attribute("name"))
			throw error << "View missing \"name\" attribute!";

		const char* delim = " \t\r\n,";
		const std::string nameAttr = node.attribute("name").as_string();
		size_t prevOff = nameAttr.find_first_not_of(delim, 0);
		size_t off = nameAttr.find_first_of(delim, prevOff);
		std::string viewKey;
		while(off != std::string::npos || prevOff != std::string::npos)
		{
			viewKey = nameAttr.substr(prevOff, off - prevOff);
			prevOff = nameAttr.find_first_not_of(delim, off);
			off = nameAttr.find_first_of(delim, prevOff);

			ThemeView& view = mViews.insert(std::pair<std::string, ThemeView>(viewKey, ThemeView())).first->second;
			parseView(node, view);
		}
	}
}

void ThemeData::parseView(const pugi::xml_node& root, ThemeView& view)
{
	ThemeException error;
	error.setFiles(mPaths);

	for(pugi::xml_node node = root.first_child(); node; node = node.next_sibling())
	{
		if(!node.attribute("name"))
			throw error << "Element of type \"" << node.name() << "\" missing \"name\" attribute!";

		auto elemTypeIt = sElementMap.find(node.name());
		if(elemTypeIt == sElementMap.end())
			throw error << "Unknown element of type \"" << node.name() << "\"!";

		const char* delim = " \t\r\n,";
		const std::string nameAttr = node.attribute("name").as_string();
		size_t prevOff = nameAttr.find_first_not_of(delim, 0);
		size_t off = nameAttr.find_first_of(delim, prevOff);
		while(off != std::string::npos || prevOff != std::string::npos)
		{
			std::string elemKey = nameAttr.substr(prevOff, off - prevOff);
			prevOff = nameAttr.find_first_not_of(delim, off);
			off = nameAttr.find_first_of(delim, prevOff);

			parseElement(node, elemTypeIt->second,
				view.elements.insert(std::pair<std::string, ThemeElement>(elemKey, ThemeElement())).first->second);

			if(std::find(view.orderedKeys.begin(), view.orderedKeys.end(), elemKey) == view.orderedKeys.end())
				view.orderedKeys.push_back(elemKey);
		}
	}
}

void ThemeData::parseElement(
	const pugi::xml_node& root, const std::map<std::string, ElementPropertyType>& typeMap, ThemeElement& element)
{
	ThemeException error;
	error.setFiles(mPaths);

	element.type = root.name();
	element.extra = root.attribute("extra").as_bool(false);

	for(pugi::xml_node node = root.first_child(); node; node = node.next_sibling())
	{
		auto typeIt = typeMap.find(node.name());
		if(typeIt == typeMap.end())
			throw error << "Unknown property type \"" << node.name() << "\" (for element of type " << root.name()
						<< ").";

		switch(typeIt->second)
		{
		case NORMALIZED_PAIR: {
			std::string str = std::string(node.text().as_string());

			size_t divider = str.find(' ');
			if(divider == std::string::npos)
				throw error << "invalid normalized pair (property \"" << node.name() << "\", value \"" << str.c_str()
							<< "\")";

			std::string first = str.substr(0, divider);
			std::string second = str.substr(divider, std::string::npos);

			Eigen::Vector2f val(parseFloat(first), parseFloat(second));

			element.properties[node.name()] = val;
			break;
		}
		case STRING: element.properties[node.name()] = std::string(node.text().as_string()); break;
		case PATH: {
			std::string path = resolvePath(node.text().as_string(), mPaths.back().string());
			if(!ResourceManager::getInstance()->fileExists(path))
			{
				std::stringstream ss;
				ss << "  Warning " << error.msg; // "from theme yadda yadda, included file yadda yadda
				ss << "could not find file \"" << node.text().get() << "\" ";
				if(node.text().get() != path)
					ss << "(which resolved to \"" << path << "\") ";
				LOG(LogWarning) << ss.str();
			}
			element.properties[node.name()] = path;
			break;
		}
		case COLOR: element.properties[node.name()] = getHexColor(node.text().as_string()); break;
		case FLOAT: element.properties[node.name()] = node.text().as_float(); break;
		case BOOLEAN: element.properties[node.name()] = node.text().as_bool(); break;
		default:
			throw error << "Unknown ElementPropertyType for \"" << root.attribute("name").as_string() << "\", property "
						<< node.name();
		}
	}
}

const ThemeData::ThemeElement* ThemeData::getElement(
	const std::string& view, const std::string& element, const std::string& expectedType) const
{
	auto viewIt = mViews.find(view);
	if(viewIt == mViews.end())
		return nullptr; // not found

	auto elemIt = viewIt->second.elements.find(element);
	if(elemIt == viewIt->second.elements.end())
		return nullptr;

	if(elemIt->second.type != expectedType && !expectedType.empty())
	{
		LOG(LogWarning) << " requested mismatched theme type for [" << view << "." << element << "] - expected \""
						<< expectedType << "\", got \"" << elemIt->second.type << "\"";
		return nullptr;
	}

	return &elemIt->second;
}

const std::shared_ptr<ThemeData>& ThemeData::getDefault()
{
	static std::shared_ptr<ThemeData> theme = nullptr;
	if(theme == nullptr)
	{
		theme = std::make_shared<ThemeData>();

		const auto found = es::paths::find(es::paths::Kind::Config, "es_theme_default.xml");
		if(found)
		{
			const std::string path = found->string();
			try
			{
				theme->loadFile(path);
			}
			catch(ThemeException& e)
			{
				LOG(LogError) << e.what();
				theme = std::make_shared<ThemeData>(); //reset to empty
			}
		}
	}

	return theme;
}

std::vector<std::unique_ptr<GuiComponent>> ThemeData::makeExtras(
	const std::shared_ptr<ThemeData>& theme, const std::string& view, Window* window)
{
	std::vector<std::unique_ptr<GuiComponent>> comps;

	auto viewIt = theme->mViews.find(view);
	if(viewIt == theme->mViews.end())
		return comps;

	for(auto it = viewIt->second.orderedKeys.begin(); it != viewIt->second.orderedKeys.end(); it++)
	{
		ThemeElement& elem = viewIt->second.elements.at(*it);
		if(elem.extra)
		{
			std::unique_ptr<GuiComponent> comp;
			const std::string& t = elem.type;
			if(t == "image")
				comp = std::make_unique<ImageComponent>(window);
			else if(t == "text")
				comp = std::make_unique<TextComponent>(window);
			else
				continue;

			comp->applyTheme(theme, view, *it, ThemeFlags::ALL);
			comps.push_back(std::move(comp));
		}
	}

	return comps;
}

void ThemeExtras::setExtras(std::vector<std::unique_ptr<GuiComponent>> extras)
{
	mExtras = std::move(extras); // deletes the old extras, which detach themselves from this parent
	for(auto& extra : mExtras)
		addChild(extra.get());
}

std::map<std::string, ThemeSet> ThemeData::getThemeSets()
{
	std::map<std::string, ThemeSet> sets;

	// lowest priority first: a set in a later directory replaces one with the same name
	std::vector<fs::path> paths = es::paths::systemDirs(es::paths::Kind::Data);
	std::reverse(paths.begin(), paths.end());
	paths.push_back(es::paths::userDir(es::paths::Kind::Data));

	fs::directory_iterator end;

	for(auto& path : paths)
	{
		path /= "themes";
		std::error_code ec;
		if(!fs::is_directory(path, ec))
			continue;

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
	if(themeSets.empty())
	{
		// no theme sets available
		return "";
	}

	auto set = themeSets.find(Settings::getInstance()->getString("ThemeSet"));
	if(set == themeSets.end())
	{
		// currently selected theme set is missing, so just pick the first available set
		set = themeSets.begin();
		Settings::getInstance()->setString("ThemeSet", set->first);
	}

	return set->second.getThemePath(system);
}
