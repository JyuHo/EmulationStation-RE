#include "resources/LruCache.h"
#include "resources/TextureLoader.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace {
std::string fixture(const std::string& name)
{
	return (std::filesystem::path(ES_TEST_DATA_DIR) / "images" / name).string();
}

// Collects results until `count` arrived or 5 s passed.
std::vector<TextureLoader::Result> waitFor(TextureLoader& loader, std::size_t count)
{
	std::vector<TextureLoader::Result> results;
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	while(results.size() < count && std::chrono::steady_clock::now() < deadline)
	{
		for(auto& result : loader.takeFinished(count - results.size()))
			results.push_back(std::move(result));
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	return results;
}

} // namespace

TEST_CASE("TextureLoader decodes files on its threads", "[TextureLoader]")
{
	TextureLoader loader(2);
	const auto still = loader.enqueue(fixture("still.png"));
	const auto animated = loader.enqueue(fixture("animated.webp"));
	const auto missing = loader.enqueue(fixture("missing.png"));

	const auto results = waitFor(loader, 3);
	REQUIRE(results.size() == 3);
	for(const auto& result : results)
	{
		CAPTURE(result.path);
		if(result.ticket == still)
		{
			REQUIRE(result.image.has_value());
			CHECK(result.image->width == 4);
			CHECK_FALSE(result.image->isAnimated());
		}
		else if(result.ticket == animated)
		{
			REQUIRE(result.image.has_value());
			CHECK(result.image->frames.size() == 3);
		}
		else
		{
			CHECK(result.ticket == missing);
			CHECK_FALSE(result.image.has_value());
		}
	}
	CHECK(loader.idle());
}

TEST_CASE("TextureLoader drops cancelled requests", "[TextureLoader]")
{
	TextureLoader loader(1);
	std::vector<TextureLoader::Ticket> tickets;
	for(int i = 0; i < 50; i++)
		tickets.push_back(loader.enqueue(fixture("still.jpg")));
	// cancelled while queued, being decoded or already finished: never returned
	for(std::size_t i = 0; i + 1 < tickets.size(); i++)
		loader.cancel(tickets[i]);

	// idle() is false while a result waits to be taken: wait for the queue to drain
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	std::vector<TextureLoader::Result> results;
	while(!loader.idle() && std::chrono::steady_clock::now() < deadline)
	{
		for(auto& result : loader.takeFinished(50))
			results.push_back(std::move(result));
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	// the last request is the only one left (it is also decoded first: newest first)
	REQUIRE(results.size() <= 1);
	if(!results.empty())
		CHECK(results.front().ticket == tickets.back());
	CHECK(loader.idle());
}

TEST_CASE("TextureLoader stops with requests pending", "[TextureLoader]")
{
	auto loader = std::make_unique<TextureLoader>(2);
	for(int i = 0; i < 100; i++)
		(void)loader->enqueue(fixture("still.jpg"));
	loader.reset(); // joins the workers without decoding the whole queue
	SUCCEED();
}

namespace {
struct Blob
{
	std::size_t bytes;
};
} // namespace

TEST_CASE("LruCache releases the least recently used unshared objects", "[TextureLoader]")
{
	LruCache<int, Blob> cache;
	auto a = std::make_shared<Blob>(Blob{10});
	auto b = std::make_shared<Blob>(Blob{10});
	auto c = std::make_shared<Blob>(Blob{10});
	cache.touch(1, a);
	cache.touch(2, b);
	cache.touch(3, c);
	cache.touch(1, a); // 1 is now the most recent: 2 is the least recently used
	std::weak_ptr<Blob> weakB = b;
	std::weak_ptr<Blob> weakC = c;
	b.reset();
	c.reset();
	const auto sizeOf = [](const Blob& blob) {
		return blob.bytes;
	};

	SECTION("within budget: nothing released")
	{
		CHECK(cache.trim(30, 30, sizeOf) == 30);
		CHECK(cache.size() == 3);
	}
	SECTION("over budget: least recently used first")
	{
		CHECK(cache.trim(30, 25, sizeOf) == 20);
		CHECK(weakB.expired());
		CHECK_FALSE(weakC.expired());
		CHECK(cache.contains(1));
		CHECK_FALSE(cache.contains(2));
	}
	SECTION("an object in use is never released")
	{
		CHECK(cache.trim(30, 0, sizeOf) == 10);
		CHECK(cache.size() == 1);
		CHECK(cache.contains(1));
		CHECK(a.use_count() == 2);
	}
}
