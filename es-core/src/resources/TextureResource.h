#pragma once

#include "resources/ResourceManager.h"

#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "math/Math.h"
#include "renderer/Renderer.h"

// A texture of the renderer (RGBA8).
// Automatically recreates the texture with renderer deinit/reinit.
class TextureResource : public IReloadable
{
public:
	static std::shared_ptr<TextureResource> get(const std::string& path, bool tile = false);

	virtual ~TextureResource();

	void unload(std::shared_ptr<ResourceManager>& rm) override;
	void reload(std::shared_ptr<ResourceManager>& rm) override;

	bool isInitialized() const;
	bool isTiled() const;
	const glm::ivec2& getSize() const;
	// 0 until initialized; changes when the renderer is reinitialized (game launch)
	Renderer::TextureHandle getTextureHandle() const { return mTexture; }

	// Warning: will NOT correctly reinitialize when this texture is reloaded (e.g. ES starts/stops playing a game).
	virtual void initFromMemory(const char* file, size_t length);

	// Warning: will NOT correctly reinitialize when this texture is reloaded (e.g. ES starts/stops playing a game).
	void initFromPixels(const unsigned char* dataRGBA, size_t width, size_t height);

	size_t getMemUsage() const;		  // returns an approximation of the VRAM used by this texture (in bytes)
	static size_t getTotalMemUsage(); // returns an approximation of total VRAM used by textures (in bytes)
	static void evictIfNeeded(); // evict least-recently-used textures over the MaxVRAM budget
	static void processPending(); // upload textures decoded off-thread (call on main thread)

protected:
	TextureResource(const std::string& path, bool tile);
	void deinit();

	glm::ivec2 mTextureSize;
	const std::string mPath;
	const bool mTile;

private:
	Renderer::TextureHandle mTexture;

	// Async loading state
	std::vector<unsigned char> mPendingPixels;
	size_t mPendingW = 0;
	size_t mPendingH = 0;
	bool mPendingReady = false;
	std::mutex mPendingMutex;
	std::jthread mWorker;

	using TextureKeyType = std::pair<std::string, bool>;
	static std::map<TextureKeyType, std::weak_ptr<TextureResource>>
		sTextureMap; // map of textures, used to prevent duplicate textures

	static std::list<std::weak_ptr<TextureResource>>
		sTextureList; // list of all textures, used for memory approximations
};
