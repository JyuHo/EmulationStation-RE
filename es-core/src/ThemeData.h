#pragma once

#include <iostream>
#include <sstream>
#include <memory>
#include <map>
#include <deque>
#include <string>
#include <filesystem>
#include <variant>
#include "math/Math.h"
#include "pugixml/pugixml.hpp"
#include "theme/ThemeLoader.h"
#include "GuiComponent.h"

template <typename T> class TextListComponent;

namespace es::theme {
class Diagnostics;
}

class Sound;
class ImageComponent;
class NinePatchComponent;
class TextComponent;
class Window;

namespace ThemeFlags {
enum PropertyFlags : unsigned int
{
	PATH = 1,
	POSITION = 2,
	SIZE = 4,
	ORIGIN = 8,
	COLOR = 16,
	FONT_PATH = 32,
	FONT_SIZE = 64,
	SOUND = 128,
	ALIGNMENT = 256,
	TEXT = 512,
	FORCE_UPPERCASE = 1024,
	LINE_SPACING = 2048,
	Z_INDEX = 4096,
	ROTATION = 8192,

	ALL = 0xFFFFFFFF
};
}

class ThemeException : public std::exception
{
public:
	std::string msg;

	const char* what() const throw() override { return msg.c_str(); }

	template <typename T> friend ThemeException& operator<<(ThemeException& e, T msg);

	inline void setFiles(const std::deque<std::filesystem::path>& deque)
	{
		*this << "from theme \"" << deque.front().string() << "\"\n";
		for(auto it = deque.begin() + 1; it != deque.end(); it++)
			*this << "  (from included file \"" << (*it).string() << "\")\n";
		*this << "    ";
	}
};

template <typename T> ThemeException& operator<<(ThemeException& e, T appendMsg)
{
	std::stringstream ss;
	ss << e.msg << appendMsg;
	e.msg = ss.str();
	return e;
}

class ThemeExtras : public GuiComponent
{
public:
	// host: the component whose children the extras become, so that they are drawn between its
	// other children by zIndex (gamelist views); nullptr: this component (system view)
	explicit ThemeExtras(Window* window, GuiComponent* host = nullptr) :
		GuiComponent(window), mHost(host ? host : this){};

	// replaces (and deletes) the previous extras, then sorts the host's children by zIndex
	void setExtras(std::vector<std::unique_ptr<GuiComponent>> extras);

private:
	GuiComponent* mHost;
	std::vector<std::unique_ptr<GuiComponent>> mExtras;
};

struct ThemeSet
{
	std::filesystem::path path;

	inline std::string getName() const { return path.stem().string(); }
	inline std::filesystem::path getThemePath(const std::string& system) const
	{
		const std::filesystem::path p1 = path / system / "theme.xml";
		if(std::filesystem::exists(p1)) return p1;
		const std::filesystem::path p2 = path / "default" / "theme.xml";
		if(std::filesystem::exists(p2)) return p2;
		return path / "theme.xml";
	}
};

// The system a theme is loaded for (docs/THEMES.md sections 5 and 6). Empty: no system.
struct ThemeSystemInfo
{
	std::string themeFolder; // $system: <theme> of es_systems.cfg ("nes")
	std::string name;		 // ${system.name} ("nes")
	std::string fullName;	 // ${system} ("Nintendo Entertainment System")
};

class ThemeData
{
public:
	class ThemeElement
	{
	public:
		bool extra;
		std::string type;

		std::map<std::string, std::variant<glm::vec2, std::string, unsigned int, float, bool>> properties;

		template <typename T> T get(const std::string& prop) const { return std::get<T>(properties.at(prop)); }

		inline bool has(const std::string& prop) const { return (properties.find(prop) != properties.end()); }
	};

private:
	class ThemeView
	{
	public:
		std::map<std::string, ThemeElement> elements;
		std::vector<std::string> orderedKeys;
	};

public:
	ThemeData();

	// Logs the diagnostics once. Throws ThemeException only when the main file is unusable
	// (missing, malformed, no version); every caller catches it (SystemData, getDefault).
	void loadFile(const std::string& path, const ThemeSystemInfo& system = {});
	// Same load, for --check-theme and tests: nothing is logged and nothing is thrown, the
	// diagnostics go to diag. Returns false when the main file is unusable (theme left empty).
	bool loadFile(const std::string& path, es::theme::Diagnostics& diag, int screenWidth, int screenHeight,
		const ThemeSystemInfo& system = {});

	enum ElementPropertyType
	{
		NORMALIZED_PAIR,
		PATH,
		STRING,
		COLOR,
		FLOAT,
		BOOLEAN
	};

	// If expectedType is an empty string, will do no type checking.
	const ThemeElement* getElement(
		const std::string& view, const std::string& element, const std::string& expectedType) const;

	static std::vector<std::unique_ptr<GuiComponent>> makeExtras(
		const std::shared_ptr<ThemeData>& theme, const std::string& view, Window* window);

	static const std::shared_ptr<ThemeData>& getDefault();

	// Phase 11e: theme options (subsets) and region support, read at load time.
	const std::vector<es::theme::ThemeSubset>& getSubsets() const { return mSubsets; }
	const std::map<std::string, std::string, std::less<>>& getSelectedOptions() const { return mSelectedOptions; }
	bool usesRegion() const { return mUsesRegion; }

	static std::map<std::string, ThemeSet> getThemeSets();
	static std::filesystem::path getThemeFromCurrentSet(const std::string& system);
	static std::string currentThemeSetName();

private:
	static std::map<std::string, std::map<std::string, ElementPropertyType>> sElementMap;

	std::deque<std::filesystem::path> mPaths;
	float mVersion;

	void parseIncludes(const pugi::xml_node& themeRoot);
	void parseViews(const pugi::xml_node& themeRoot);
	void parseView(const pugi::xml_node& viewNode, ThemeView& view);
	void parseElement(const pugi::xml_node& elementNode, const std::map<std::string, ElementPropertyType>& typeMap,
		ThemeElement& element);

	std::map<std::string, ThemeView> mViews;
	std::vector<es::theme::ThemeSubset> mSubsets;
	std::map<std::string, std::string, std::less<>> mSelectedOptions;
	bool mUsesRegion = false;
};
