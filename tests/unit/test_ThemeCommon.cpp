// Phase 11d: properties common to the theme elements (docs/THEMES.md sections 10 and 11, rows
// "11d"): zIndex and the default drawing order, rotation, rotationOrigin, disabled.

#include "GuiComponent.h"
#include "ThemeData.h"
#include "Window.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

namespace {

const std::string kTheme = ES_TEST_DATA_DIR "/themes/recalbox/common-properties.xml";

std::shared_ptr<ThemeData> load()
{
	auto theme = std::make_shared<ThemeData>();
	theme->loadFile(kTheme, ThemeSystemInfo{"nes", "nes", "Nintendo"});
	return theme;
}

// transformed position of a local point
glm::vec2 apply(GuiComponent& c, glm::vec2 local)
{
	const glm::vec4 p = c.getTransform() * glm::vec4(local, 0.0f, 1.0f);
	return {p.x, p.y};
}

} // namespace

TEST_CASE("Common properties: zIndex and its default", "[phase11][11d][ThemeCommon]")
{
	Window window;
	const auto theme = load();

	GuiComponent withValue(&window);
	withValue.setDefaultZIndex(40);
	withValue.applyTheme(theme, "detailed", "md_title", ThemeFlags::ALL);
	CHECK(withValue.getZIndex() == 45.0f);

	// an element without zIndex takes the default of the component
	GuiComponent withDefault(&window);
	withDefault.setDefaultZIndex(30);
	withDefault.applyTheme(theme, "detailed", "e_plain", ThemeFlags::ALL);
	CHECK(withDefault.getZIndex() == 30.0f);

	// a component that the theme does not mention keeps its default: a view element absent
	// from a format 3 theme (md_rating of the capture theme) stays at its place
	GuiComponent absent(&window);
	absent.setDefaultZIndex(40);
	absent.applyTheme(theme, "detailed", "md_rating", ThemeFlags::ALL);
	CHECK(absent.getZIndex() == 40.0f);

	// without the Z_INDEX flag the theme value is not used (Recalbox Themable.cpp:73-74)
	GuiComponent notThemed(&window);
	notThemed.setDefaultZIndex(20);
	notThemed.applyTheme(theme, "detailed", "md_title", ThemeFlags::POSITION);
	CHECK(notThemed.getZIndex() == 20.0f);
}

TEST_CASE("Common properties: children sorted by zIndex, stable", "[phase11][11d][ThemeCommon]")
{
	Window window;
	GuiComponent parent(&window);
	GuiComponent a(&window), b(&window), c(&window), d(&window);
	a.setZIndex(10);
	b.setZIndex(0);
	c.setZIndex(10);
	d.setZIndex(0);
	for(GuiComponent* child : {&a, &b, &c, &d})
		parent.addChild(child);

	parent.sortChildren();
	REQUIRE(parent.getChildCount() == 4);
	// equal values keep their order: format 3 themes, without zIndex, keep the current order
	CHECK(parent.getChild(0) == &b);
	CHECK(parent.getChild(1) == &d);
	CHECK(parent.getChild(2) == &a);
	CHECK(parent.getChild(3) == &c);
}

TEST_CASE("Common properties: extras interleave with the host's children", "[phase11][11d][ThemeCommon]")
{
	Window window;
	GuiComponent view(&window);
	GuiComponent list(&window);
	list.setZIndex(20);
	view.addChild(&list);

	auto extras = ThemeData::makeExtras(load(), "detailed", &window);
	REQUIRE(extras.size() == 3);
	GuiComponent* front = extras[0].get();
	GuiComponent* hidden = extras[1].get();
	GuiComponent* plain = extras[2].get();
	CHECK(front->getZIndex() == 5.0f);
	CHECK(plain->getZIndex() == 10.0f); // default of the extras

	ThemeExtras holder(&window, &view);
	holder.setExtras(std::move(extras));
	CHECK(holder.getChildCount() == 0);
	REQUIRE(view.getChildCount() == 4);
	CHECK(view.getChild(0) == front);
	CHECK(view.getChild(1) == hidden);
	CHECK(view.getChild(2) == plain);
	CHECK(view.getChild(3) == &list);

	holder.setExtras({});
	REQUIRE(view.getChildCount() == 1);
	CHECK(view.getChild(0) == &list);
}

TEST_CASE("Common properties: disabled", "[phase11][11d][ThemeCommon]")
{
	Window window;
	const auto theme = load();

	GuiComponent hidden(&window);
	hidden.applyTheme(theme, "detailed", "e_hidden", ThemeFlags::ALL);
	CHECK(hidden.isThemeDisabled());

	GuiComponent shown(&window);
	shown.setThemeDisabled(true);
	shown.applyTheme(theme, "detailed", "e_plain", ThemeFlags::ALL);
	CHECK_FALSE(shown.isThemeDisabled());

	// read with the position (Recalbox Themable.cpp:76-77)
	GuiComponent noPosition(&window);
	noPosition.applyTheme(theme, "detailed", "e_hidden", ThemeFlags::SIZE);
	CHECK_FALSE(noPosition.isThemeDisabled());
}

TEST_CASE("Common properties: rotation around rotationOrigin", "[phase11][11d][ThemeCommon]")
{
	Window window;
	const auto theme = load();

	GuiComponent c(&window);
	c.setPosition(100.0f, 50.0f);
	c.setSize(40.0f, 20.0f);
	c.applyTheme(theme, "detailed", "e_front", ThemeFlags::ROTATION);
	CHECK(c.getRotation() == 90.0f);
	CHECK(c.getRotationOrigin() == glm::vec2(0.5f, 0.5f));

	// 90 degrees clockwise around the center (20, 10): the top-left corner goes to (30, -10)
	const glm::vec2 corner = apply(c, {0.0f, 0.0f});
	CHECK(corner.x == Catch::Approx(130.0f));
	CHECK(corner.y == Catch::Approx(40.0f).margin(1e-4));
	// the center does not move
	const glm::vec2 center = apply(c, {20.0f, 10.0f});
	CHECK(center.x == Catch::Approx(120.0f));
	CHECK(center.y == Catch::Approx(60.0f));

	// no rotation in the element: back to 0, origin (0, 0) (Recalbox Themable.cpp:66-70)
	c.applyTheme(theme, "detailed", "e_plain", ThemeFlags::ROTATION);
	CHECK(c.getRotation() == 0.0f);
	CHECK(c.getRotationOrigin() == glm::vec2(0.0f, 0.0f));
	const glm::vec2 same = apply(c, {0.0f, 0.0f});
	CHECK(same.x == 100.0f);
	CHECK(same.y == 50.0f);
}
