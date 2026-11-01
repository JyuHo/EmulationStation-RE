#include "ThemeData.h"
#include "theme/ThemeDiagnostics.h"
#include "Window.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

TEST_CASE("ThemeData loads a format 3 theme", "[ThemeData]")
{
	ThemeData theme;
	REQUIRE_NOTHROW(theme.loadFile(ES_TEST_DATA_DIR "/themes/minimal.xml"));

	const ThemeData::ThemeElement* text = theme.getElement("detailed", "md_description", "text");
	REQUIRE(text != nullptr);
	CHECK(text->get<unsigned int>("color") == 0x11223344u);
	CHECK(text->get<float>("fontSize") == 0.025f);

	const ThemeData::ThemeElement* image = theme.getElement("detailed", "md_image", "image");
	REQUIRE(image != nullptr);
	CHECK(image->get<glm::vec2>("pos").x == 0.1f);
	CHECK(image->get<glm::vec2>("maxSize").y == 0.5f);
}

TEST_CASE("ThemeData rejects invalid themes", "[ThemeData]")
{
	ThemeData theme;
	CHECK_THROWS(theme.loadFile(ES_TEST_DATA_DIR "/themes/no-version.xml"));
	CHECK_THROWS(theme.loadFile(ES_TEST_DATA_DIR "/themes/does-not-exist.xml"));
}

TEST_CASE("ThemeData resolves ./ paths and includes relative to the theme file", "[ThemeData]")
{
	const std::string dir = ES_TEST_DATA_DIR "/capture/themes/capture";
	ThemeData theme;
	REQUIRE_NOTHROW(theme.loadFile(dir + "/nes/theme.xml"));

	// "./../art/logo-nes.svg" in nes/theme.xml
	const ThemeData::ThemeElement* logo = theme.getElement("system", "logo", "image");
	REQUIRE(logo != nullptr);
	CHECK(logo->get<std::string>("path") == dir + "/nes/../art/logo-nes.svg");

	// "./art/background.png" in common.xml, included as "./../common.xml"
	const ThemeData::ThemeElement* background = theme.getElement("basic", "background", "image");
	REQUIRE(background != nullptr);
	CHECK(background->get<std::string>("path") == dir + "/nes/../art/background.png");
}

TEST_CASE("ThemeData reads the video element", "[ThemeData]")
{
	ThemeData theme;
	REQUIRE_NOTHROW(theme.loadFile(ES_TEST_DATA_DIR "/themes/video.xml"));

	const ThemeData::ThemeElement* video = theme.getElement("detailed", "md_video", "video");
	REQUIRE(video != nullptr);
	CHECK(video->get<glm::vec2>("pos") == glm::vec2(0.25f, 0.4f));
	CHECK(video->get<glm::vec2>("maxSize") == glm::vec2(0.45f, 0.4f));
	CHECK(video->get<glm::vec2>("origin") == glm::vec2(0.5f, 0.5f));
	CHECK(video->get<std::string>("default") == ES_TEST_DATA_DIR "/themes/default.mp4");
	CHECK(video->get<float>("delay") == 1.5f);
	CHECK(video->get<bool>("showSnapshotNoVideo"));
	CHECK(video->get<bool>("showSnapshotDelay"));
}

TEST_CASE("ThemeData extras are drawn as children of ThemeExtras", "[ThemeData]")
{
	// #75 dropped the addChild of setExtras: the extras of the system view (background,
	// caption of the capture theme) were created but never drawn, a black system view.
	// Plain components: a themed TextComponent needs a renderer for its font.
	const std::string dir = ES_TEST_DATA_DIR "/capture/themes/capture";
	ThemeData theme;
	REQUIRE_NOTHROW(theme.loadFile(dir + "/nes/theme.xml"));
	const ThemeData::ThemeElement* background = theme.getElement("system", "background", "image");
	REQUIRE(background != nullptr);
	CHECK(background->extra);

	Window window;
	std::vector<std::unique_ptr<GuiComponent>> extras;
	extras.push_back(std::make_unique<GuiComponent>(&window));
	extras.push_back(std::make_unique<GuiComponent>(&window));
	GuiComponent* first = extras.front().get();

	ThemeExtras holder(&window);
	holder.setExtras(std::move(extras));
	REQUIRE(holder.getChildCount() == 2);
	CHECK(holder.getChild(0) == first);

	// replacing the extras detaches the old ones
	holder.setExtras({});
	CHECK(holder.getChildCount() == 0);
}

TEST_CASE("ThemeData rejects an element of the wrong type", "[ThemeData]")
{
	ThemeData theme;
	REQUIRE_NOTHROW(theme.loadFile(ES_TEST_DATA_DIR "/themes/minimal.xml"));
	CHECK(theme.getElement("detailed", "md_description", "text") != nullptr);
	CHECK(theme.getElement("detailed", "md_description", "image") == nullptr);
	CHECK(theme.getElement("detailed", "md_description", "") != nullptr);
}

TEST_CASE("ThemeData ignores what it does not know, with a file:line warning", "[ThemeData]")
{
	// R6: an unknown property, an invalid value or an unknown element only drops that node
	const std::string file = ES_TEST_DATA_DIR "/themes/lenient.xml";
	ThemeData theme;
	es::theme::Diagnostics diag;
	REQUIRE(theme.loadFile(file, diag, 1920, 1080));

	const ThemeData::ThemeElement* text = theme.getElement("detailed", "md_description", "text");
	REQUIRE(text != nullptr);
	CHECK(text->get<unsigned int>("color") == 0x11223344u);
	CHECK_FALSE(text->has("sparkle"));
	const ThemeData::ThemeElement* image = theme.getElement("detailed", "md_image", "image");
	REQUIRE(image != nullptr);
	CHECK(image->get<glm::vec2>("pos") == glm::vec2(0.1f, 0.2f));
	CHECK_FALSE(image->has("color"));

	std::vector<int> lines;
	for(const auto& d : diag.all())
	{
		CHECK(d.severity == es::theme::Severity::Warning);
		CHECK(d.where.file.filename() == "lenient.xml");
		lines.push_back(d.where.line);
	}
	// sparkle (line 6), invalid color (line 10), unknown element hologram (line 12)
	CHECK(lines == std::vector<int>{6, 10, 12});
}

TEST_CASE("ThemeData reports an unusable main file without throwing", "[ThemeData]")
{
	es::theme::Diagnostics diag;
	ThemeData theme;
	CHECK_FALSE(theme.loadFile(ES_TEST_DATA_DIR "/themes/no-version.xml", diag, 1920, 1080));
	CHECK(diag.hasErrors());
	CHECK(theme.getElement("detailed", "md_description", "text") == nullptr);
}

TEST_CASE("ThemeSet looks for the theme file like Recalbox", "[ThemeData]")
{
	// docs/THEMES.md section 1: <set>/<system>/theme.xml, <set>/default/theme.xml, <set>/theme.xml
	namespace fs = std::filesystem;
	const fs::path set = fs::temp_directory_path() / "es-test-theme-lookup";
	fs::remove_all(set);
	fs::create_directories(set / "nes");
	fs::create_directories(set / "default");
	const ThemeSet themeSet{set};

	// nothing yet: the last candidate, which does not exist (SystemData then loads no theme)
	CHECK(themeSet.getThemePath("nes") == set / "theme.xml");
	CHECK_FALSE(fs::exists(themeSet.getThemePath("nes")));

	std::ofstream(set / "theme.xml") << "<theme/>";
	CHECK(themeSet.getThemePath("nes") == set / "theme.xml");
	std::ofstream(set / "default" / "theme.xml") << "<theme/>";
	CHECK(themeSet.getThemePath("nes") == set / "default" / "theme.xml");
	std::ofstream(set / "nes" / "theme.xml") << "<theme/>";
	CHECK(themeSet.getThemePath("nes") == set / "nes" / "theme.xml");
	CHECK(themeSet.getThemePath("snes") == set / "default" / "theme.xml");
	fs::remove_all(set);
}

TEST_CASE("ThemeData reads a format 4 theme with its system", "[ThemeData]")
{
	namespace fs = std::filesystem;
	const fs::path dir = ES_TEST_DATA_DIR "/themes/recalbox";
	ThemeData theme;
	es::theme::Diagnostics diag;
	// 1280 x 720: "hd" holds (docs/THEMES.md section 7)
	REQUIRE(theme.loadFile((dir / "theme.xml").string(), diag, 1280, 720, {"nes", "nes", "Nintendo"}));
	CHECK_FALSE(diag.hasErrors());

	// ${root}/systems/${system.name}.xml: included only because system.name is set
	const ThemeData::ThemeElement* logo = theme.getElement("system", "logo", "image");
	REQUIRE(logo != nullptr);
	CHECK(fs::equivalent(logo->get<std::string>("path"), dir / "art" / "nes.svg"));

	const ThemeData::ThemeElement* background = theme.getElement("detailed", "background", "image");
	REQUIRE(background != nullptr);
	CHECK(background->get<unsigned int>("color") == 0x2e447c80u); // ${PrimaryAlpha}
	CHECK(background->get<bool>("tile"));
	CHECK(background->get<std::string>("path") == (dir / "art" / "background.png").generic_string());

	// first option of "colorset" (blue), condition "hd" true, "tate" false
	const ThemeData::ThemeElement* description = theme.getElement("detailed", "md_description", "text");
	REQUIRE(description != nullptr);
	CHECK(description->get<unsigned int>("color") == 0x0000FFFFu);
	CHECK(description->get<float>("fontSize") == 0.02f);
	CHECK(theme.getElement("detailed", "e_hdOnly", "image") != nullptr);
	CHECK(theme.getElement("detailed", "e_tateOnly", "image") == nullptr);
}

TEST_CASE("ThemeData sets ${root} to the theme set and $system to the system folder", "[ThemeData]")
{
	// docs/THEMES.md sections 5 and 6: <set>/nes/theme.xml sees ${root} = <set>
	namespace fs = std::filesystem;
	const fs::path set = fs::temp_directory_path() / "es-test-theme-root";
	fs::remove_all(set);
	fs::create_directories(set / "nes");
	fs::create_directories(set / "default" / "art");
	std::ofstream(set / "default" / "art" / "logo.png") << "x";
	std::ofstream(set / "common.xml") << "<theme><view name=\"system\"><image name=\"logo\">"
										 "<path>./$system/art/logo.png</path></image></view></theme>";
	std::ofstream(set / "nes" / "theme.xml") << "<theme><formatVersion>4</formatVersion>"
												"<include>${root}/common.xml</include></theme>";

	ThemeData theme;
	es::theme::Diagnostics diag;
	REQUIRE(theme.loadFile((set / "nes" / "theme.xml").string(), diag, 1920, 1080, {"nes", "nes", ""}));
	CHECK(diag.all().empty());
	const ThemeData::ThemeElement* logo = theme.getElement("system", "logo", "image");
	REQUIRE(logo != nullptr);
	// nes/art/logo.png does not exist: $system falls back to default
	CHECK(logo->get<std::string>("path") == (set / "default" / "art" / "logo.png").generic_string());
	fs::remove_all(set);
}
