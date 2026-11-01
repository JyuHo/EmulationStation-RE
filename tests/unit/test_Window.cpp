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

namespace {

// Closes itself, then keeps using its members (as GuiMsgBox::deleteMeAndCall does)
struct SelfClosingGui : CountingGui
{
	using CountingGui::CountingGui;
	int closeAndRead()
	{
		mWindow->closeGui(this);
		return ++mUses;
	}
	int mUses = 0;
};

// Opens or closes another GUI from its destructor
struct ChainGui : CountingGui
{
	ChainGui(Window* window, int& destroyed, GuiComponent* toClose, int* pushedDestroyed) :
		CountingGui(window, destroyed), mToClose(toClose), mPushedDestroyed(pushedDestroyed)
	{}
	~ChainGui() override
	{
		if(mToClose)
			mWindow->closeGui(mToClose);
		if(mPushedDestroyed)
			mWindow->pushGui(std::make_unique<CountingGui>(mWindow, *mPushedDestroyed));
	}
	GuiComponent* mToClose;
	int* mPushedDestroyed;
};

} // namespace

TEST_CASE("closeGui unlinks at once and deletes at the next update", "[Window][11f3]")
{
	int destroyed = 0;
	Window window;
	window.pushGui(std::make_unique<CountingGui>(&window, destroyed));
	GuiComponent* bottom = window.peekGui();
	auto top = std::make_unique<SelfClosingGui>(&window, destroyed);
	SelfClosingGui* raw = top.get();
	window.pushGui(std::move(top));

	CHECK(raw->closeAndRead() == 1); // still alive after closeGui (asan checks the read)
	CHECK(window.peekGui() == bottom);
	CHECK(destroyed == 0);

	window.closeGui(raw); // not on the stack any more: nothing
	window.update(0);
	CHECK(destroyed == 1);
	window.update(0);
	CHECK(destroyed == 1);
}

TEST_CASE("closeGui of a GUI below the top keeps the order of the others", "[Window][11f3]")
{
	int destroyed = 0;
	Window window;
	std::vector<GuiComponent*> guis;
	for(int i = 0; i < 3; i++)
	{
		window.pushGui(std::make_unique<CountingGui>(&window, destroyed));
		guis.push_back(window.peekGui());
	}
	window.closeGui(guis[1]); // as GuiScraperMulti, closed from the message box above it
	CHECK(window.peekGui() == guis[2]);
	window.update(0);
	CHECK(destroyed == 1);
	window.closeGui(guis[2]);
	CHECK(window.peekGui() == guis[0]);
	window.update(0);
	CHECK(destroyed == 2);
}

TEST_CASE("a closed GUI whose destructor closes or opens another one", "[Window][11f3]")
{
	int destroyed = 0;
	int pushedDestroyed = 0;
	{
		Window window;
		window.pushGui(std::make_unique<CountingGui>(&window, destroyed));
		GuiComponent* other = window.peekGui();
		window.pushGui(std::make_unique<ChainGui>(&window, destroyed, other, &pushedDestroyed));
		window.closeGui(window.peekGui());
		window.update(0); // ChainGui destroyed: closes "other", pushes a new GUI
		CHECK(destroyed == 1);
		REQUIRE(window.peekGui() != nullptr);
		CHECK(window.peekGui() != other);
		window.update(0);
		CHECK(destroyed == 2);
		CHECK(pushedDestroyed == 0);
	}
	CHECK(pushedDestroyed == 1);
}

TEST_CASE("destroying the window deletes the closed GUIs not yet deleted", "[Window][11f3]")
{
	int destroyed = 0;
	{
		Window window;
		window.pushGui(std::make_unique<CountingGui>(&window, destroyed));
		window.pushGui(std::make_unique<CountingGui>(&window, destroyed));
		window.closeGui(window.peekGui());
		CHECK(destroyed == 0);
	}
	CHECK(destroyed == 2);
}
