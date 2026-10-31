// Window owns the GUIs pushed on it, while GUIs still close themselves with "delete this".
// These tests run under the asan preset to catch double frees.
#include "Window.h"

#include <catch2/catch_test_macros.hpp>

namespace {

struct CountingGui : GuiComponent
{
	CountingGui(Window* window, int& destroyed) : GuiComponent(window), mDestroyed(destroyed) {}
	~CountingGui() override { mDestroyed++; }
	int& mDestroyed;
};

} // namespace

TEST_CASE("a GUI that deletes itself leaves the window stack", "[Window]")
{
	int destroyed = 0;
	Window window;
	window.pushGui(std::make_unique<CountingGui>(&window, destroyed));
	auto* top = new CountingGui(&window, destroyed);
	window.pushGui(std::unique_ptr<GuiComponent>(top));

	delete top;
	CHECK(destroyed == 1);
	REQUIRE(window.peekGui() != nullptr);
	CHECK(window.peekGui() != top);

	delete window.peekGui();
	CHECK(destroyed == 2);
	CHECK(window.peekGui() == nullptr);
}

TEST_CASE("destroying the window deletes every GUI once", "[Window]")
{
	int destroyed = 0;
	{
		Window window;
		for(int i = 0; i < 3; i++)
			window.pushGui(std::make_unique<CountingGui>(&window, destroyed));
	}
	CHECK(destroyed == 3);
}
