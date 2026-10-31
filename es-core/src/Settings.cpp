#include "Settings.h"
#include "utils/Paths.h"
#include "Log.h"
#include "pugixml/pugixml.hpp"
#include "platform.h"
#include <filesystem>
#include <vector>

std::unique_ptr<Settings> Settings::sInstance = nullptr;

// these values are NOT saved to es_settings.xml
// since they're set through command-line arguments, and not the in-program settings menu
std::vector<const char*> settings_dont_save = {
	"Debug", "DebugGrid", "DebugText", "ParseGamelistOnly", "ShowExit", "Windowed", "VSync", "IgnoreGamelist"};

Settings::Settings()
{
	setDefaults();
	loadFile();
}

Settings* Settings::getInstance()
{
	if(!sInstance)
		sInstance = std::unique_ptr<Settings>(new Settings());

	return sInstance.get();
}

void Settings::setDefaults()
{
	mBoolMap.clear();
	mIntMap.clear();

	mBoolMap["BackgroundJoystickInput"] = false;
	mBoolMap["GamepadSwapAB"] = false; // Nintendo style: A on the east face button of recognized gamepads
	mBoolMap["ParseGamelistOnly"] = false;
	mBoolMap["DrawFramerate"] = false;
	mBoolMap["ShowExit"] = true;
	mBoolMap["Windowed"] = false;

	mBoolMap["VSync"] = true;

	mBoolMap["EnableSounds"] = true;
	mBoolMap["ShowHelpPrompts"] = true;
	mBoolMap["ScrapeRatings"] = true;
	mBoolMap["IgnoreGamelist"] = false;
	mBoolMap["QuickSystemSelect"] = true;

	mBoolMap["Debug"] = false;
	mBoolMap["DebugGrid"] = false;
	mBoolMap["DebugText"] = false;

	mIntMap["ScreenSaverTime"] = 5 * 60 * 1000; // 5 minutes
	mIntMap["ScraperResizeWidth"] = 400;
	mIntMap["ScraperResizeHeight"] = 0;

	mStringMap["TransitionStyle"] = "fade";
	mStringMap["ThemeSet"] = "";
	mStringMap["ScreenSaverBehavior"] = "dim";
	mStringMap["Scraper"] = "TheGamesDB";
}

template <typename K, typename V> void saveMap(pugi::xml_document& doc, std::map<K, V>& map, const char* type)
{
	for(auto iter = map.begin(); iter != map.end(); iter++)
	{
		// key is on the "don't save" list, so don't save it
		if(std::find(settings_dont_save.begin(), settings_dont_save.end(), iter->first) != settings_dont_save.end())
			continue;

		pugi::xml_node node = doc.append_child(type);
		node.append_attribute("name").set_value(iter->first.c_str());
		node.append_attribute("value").set_value(iter->second);
	}
}

void Settings::saveFile()
{
	const std::string path = es::paths::userFile(es::paths::Kind::Config, "es_settings.cfg").string();

	pugi::xml_document doc;

	saveMap<std::string, bool>(doc, mBoolMap, "bool");
	saveMap<std::string, int>(doc, mIntMap, "int");
	saveMap<std::string, float>(doc, mFloatMap, "float");

	//saveMap<std::string, std::string>(doc, mStringMap, "string");
	for(auto iter = mStringMap.begin(); iter != mStringMap.end(); iter++)
	{
		pugi::xml_node node = doc.append_child("string");
		node.append_attribute("name").set_value(iter->first.c_str());
		node.append_attribute("value").set_value(iter->second.c_str());
	}

	doc.save_file(path.c_str());
}

void Settings::loadFile()
{
	const std::string path = es::paths::userFile(es::paths::Kind::Config, "es_settings.cfg").string();

	if(!std::filesystem::exists(path))
		return;

	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(path.c_str());
	if(!result)
	{
		LOG(LogError) << "Could not parse Settings file!\n   " << result.description();
		return;
	}

	for(pugi::xml_node node = doc.child("bool"); node; node = node.next_sibling("bool"))
		setBool(node.attribute("name").as_string(), node.attribute("value").as_bool());
	for(pugi::xml_node node = doc.child("int"); node; node = node.next_sibling("int"))
		setInt(node.attribute("name").as_string(), node.attribute("value").as_int());
	for(pugi::xml_node node = doc.child("float"); node; node = node.next_sibling("float"))
		setFloat(node.attribute("name").as_string(), node.attribute("value").as_float());
	for(pugi::xml_node node = doc.child("string"); node; node = node.next_sibling("string"))
		setString(node.attribute("name").as_string(), node.attribute("value").as_string());
}

//Print a warning message if the setting we're trying to get doesn't already exist in the map, then return the value in the map.
#define SETTINGS_GETSET(type, mapName, getMethodName, setMethodName)                                                   \
	type Settings::getMethodName(const std::string& name)                                                              \
	{                                                                                                                  \
		if(mapName.find(name) == mapName.end())                                                                        \
		{                                                                                                              \
			LOG(LogError) << "Tried to use unset setting " << name << "!";                                             \
		}                                                                                                              \
		return mapName[name];                                                                                          \
	}                                                                                                                  \
	void Settings::setMethodName(const std::string& name, type value)                                                  \
	{                                                                                                                  \
		mapName[name] = value;                                                                                         \
	}

SETTINGS_GETSET(bool, mBoolMap, getBool, setBool);
SETTINGS_GETSET(int, mIntMap, getInt, setInt);
SETTINGS_GETSET(float, mFloatMap, getFloat, setFloat);
SETTINGS_GETSET(const std::string&, mStringMap, getString, setString);