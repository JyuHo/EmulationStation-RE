#pragma once

#include "resources/ResourceManager.h"
#include "resources/LruCache.h"
#include "resources/TextureLoader.h"

#include <string>
#include <vector>
#include "ImageIO.h"
#include "math/Math.h"
#include "renderer/Renderer.h"

// A texture of the renderer (RGBA8), possibly animated (one texture per frame).
// Automatically recreates the texture with renderer deinit/reinit.
class TextureResource : public IReloadable
{
public:
	// dynamic: the file is read and decoded on a TextureLoader thread (game media, many
	// files, displayed while scrolling); the texture stays uninitialized (drawn as nothing)
	// until processPendingLoads() uploads it. Theme and interface images, few and needed
	// for the layout of the first frame, load synchronously.
	static std::shared_ptr<TextureResource> get(const std::string& path, bool tile = false, bool dynamic = false);

	virtual ~TextureResource();

	void unload(std::shared_ptr<ResourceManager>& rm) override;
	void reload(std::shared_ptr<ResourceManager>& rm) override;

	bool isInitialized() const;
	bool isLoading() const { return mPendingLoad != 0; } // asynchronous decoding in progress
	bool isTiled() const;
	const glm::ivec2& getSize() const;
	// 0 until initialized; changes when the renderer is reinitialized (game launch)
	Renderer::TextureHandle getTextureHandle() const { return mTexture; }

	bool isAnimated() const { return !mFrames.empty(); }
	// Texture of the frame shown timeMs after the animation started (it loops); the only
	// texture of a still image.
	Renderer::TextureHandle getTextureHandle(int timeMs) const;

	// Warning: will NOT correctly reinitialize when this texture is reloaded (e.g. ES starts/stops playing a game).
	virtual void initFromMemory(const char* file, size_t length);

	// Warning: will NOT correctly reinitialize when this texture is reloaded (e.g. ES starts/stops playing a game).
	void initFromPixels(const unsigned char* dataRGBA, size_t width, size_t height);

	size_t getMemUsage() const;		  // returns an approximation of the VRAM used by this texture (in bytes)
	static size_t getTotalMemUsage(); // returns an approximation of total VRAM used by textures (in bytes)

	// Main thread, once per frame: uploads the textures decoded by the TextureLoader since
	// the last call, then releases cached textures over the MaxVRAM budget.
	static void processPendingLoads();
	// Releases every texture kept only by the cache (before the renderer goes away).
	static void clearCache();

protected:
	TextureResource(const std::string& path, bool tile);
	void deinit();

	glm::ivec2 mTextureSize;
	const std::string mPath;
	const bool mTile;

private:
	void initFromImage(const ImageIO::Image& image);
	void loadAsync();
	void cancelLoad();
	static void trimCache();

	struct Frame
	{
		Renderer::TextureHandle texture;
		int endMs; // end of the frame since the start of the animation
	};

	Renderer::TextureHandle mTexture;
	std::vector<Frame> mFrames; // animation only; mFrames[0].texture == mTexture
	bool mDynamic = false;
	TextureLoader::Ticket mPendingLoad = 0;

	using TextureKeyType = std::pair<std::string, bool>;
	static std::map<TextureKeyType, std::weak_ptr<TextureResource>>
		sTextureMap; // map of textures, used to prevent duplicate textures

	static std::list<std::weak_ptr<TextureResource>>
		sTextureList; // list of all textures, used for memory approximations

	// recently used textures, kept alive within the MaxVRAM budget
	static LruCache<TextureKeyType, TextureResource> sCache;
	// textures waiting for the TextureLoader (each one removes itself when destroyed)
	static std::map<TextureLoader::Ticket, TextureResource*> sPendingLoads;
};
