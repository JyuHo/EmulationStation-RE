#include "resources/TextureResource.h"
#include <cassert>
#include <span>
#include "Log.h"
#include "ImageIO.h"
#include "Settings.h"
#include "renderer/Renderer.h"
#include "Util.h"
#include "resources/SVGResource.h"

std::map<TextureResource::TextureKeyType, std::weak_ptr<TextureResource>> TextureResource::sTextureMap;
std::list<std::weak_ptr<TextureResource>> TextureResource::sTextureList;
LruCache<TextureResource::TextureKeyType, TextureResource> TextureResource::sCache;
std::map<TextureLoader::Ticket, TextureResource*> TextureResource::sPendingLoads;

namespace {
// Uploads per frame: a 1 000-game list scrolled quickly must not stall a frame on GL uploads.
constexpr std::size_t kMaxUploadsPerFrame = 4;
constexpr int kDefaultMaxVRAM = 256; // MB
} // namespace

TextureResource::TextureResource(const std::string& path, bool tile) :
	mTextureSize(glm::ivec2(0)), mPath(path), mTile(tile), mTexture(0)
{}

TextureResource::~TextureResource()
{
	cancelLoad();
	deinit();
}

void TextureResource::unload(std::shared_ptr<ResourceManager>& rm)
{
	cancelLoad();
	deinit();
}

void TextureResource::reload(std::shared_ptr<ResourceManager>& rm)
{
	if(mPath.empty())
		return;

	if(mDynamic)
	{
		loadAsync();
		return;
	}

	const ResourceData& data = rm->getFileData(mPath);
	initFromMemory((const char*)data.ptr.get(), data.length);
}

void TextureResource::loadAsync()
{
	cancelLoad();
	mPendingLoad = TextureLoader::instance().enqueue(mPath);
	sPendingLoads[mPendingLoad] = this; // erased by cancelLoad(), called by the destructor
}

void TextureResource::cancelLoad()
{
	if(mPendingLoad == 0)
		return;
	TextureLoader::instance().cancel(mPendingLoad);
	sPendingLoads.erase(mPendingLoad);
	mPendingLoad = 0;
}

void TextureResource::initFromPixels(const unsigned char* dataRGBA, size_t width, size_t height)
{
	deinit();

	assert(width > 0 && height > 0);

	mTexture = Renderer::createTexture(Renderer::TextureFormat::RGBA8, Renderer::TextureFilter::LinearMinNearestMag,
		mTile, glm::ivec2(static_cast<int>(width), static_cast<int>(height)), dataRGBA);

	mTextureSize = {width, height};
}

void TextureResource::initFromImage(const ImageIO::Image& image)
{
	if(!image.isAnimated())
	{
		initFromPixels(image.rgba.data(), image.width, image.height);
		return;
	}

	deinit();
	const glm::ivec2 size(image.width, image.height);
	int endMs = 0;
	for(const ImageIO::Frame& frame : image.frames)
	{
		endMs += frame.durationMs;
		mFrames.push_back({Renderer::createTexture(Renderer::TextureFormat::RGBA8,
							   Renderer::TextureFilter::LinearMinNearestMag, mTile, size, frame.rgba.data()),
			endMs});
	}
	mTexture = mFrames.front().texture;
	mTextureSize = size;
}

void TextureResource::initFromMemory(const char* data, size_t length)
{
	auto image = ImageIO::load(std::as_bytes(std::span(data, length)));
	if(!image)
	{
		LOG(LogError) << "Could not initialize texture from memory, invalid data!  (file path: " << mPath
					  << ", data ptr: " << (size_t)data << ", reported size: " << length << "): " << image.error();
		return;
	}

	initFromImage(*image);
}

void TextureResource::deinit()
{
	if(!mFrames.empty())
	{
		for(const Frame& frame : mFrames)
			Renderer::destroyTexture(frame.texture);
		mFrames.clear();
		mTexture = 0;
	}
	if(mTexture != 0)
	{
		Renderer::destroyTexture(mTexture);
		mTexture = 0;
	}
}

Renderer::TextureHandle TextureResource::getTextureHandle(int timeMs) const
{
	if(mFrames.empty())
		return mTexture;

	const int position = std::max(timeMs, 0) % mFrames.back().endMs;
	for(const Frame& frame : mFrames)
		if(position < frame.endMs)
			return frame.texture;
	return mTexture;
}

const glm::ivec2& TextureResource::getSize() const
{
	return mTextureSize;
}

bool TextureResource::isTiled() const
{
	return mTile;
}

std::shared_ptr<TextureResource> TextureResource::get(const std::string& path, bool tile, bool dynamic)
{
	std::shared_ptr<ResourceManager>& rm = ResourceManager::getInstance();

	const std::string canonicalPath = getCanonicalPath(path);

	if(canonicalPath.empty())
	{
		std::shared_ptr<TextureResource> tex(new TextureResource("", tile));
		rm->addReloadable(tex); //make sure we get properly deinitialized even though we do nothing on reinitialization
		return tex;
	}

	TextureKeyType key(canonicalPath, tile);
	auto foundTexture = sTextureMap.find(key);
	if(foundTexture != sTextureMap.end())
	{
		if(std::shared_ptr<TextureResource> tex = foundTexture->second.lock())
		{
			sCache.touch(key, tex);
			return tex;
		}
	}

	// need to create it
	std::shared_ptr<TextureResource> tex;

	// is it an SVG?
	if(key.first.substr(key.first.size() - 4, std::string::npos) == ".svg")
	{
		// probably
		// don't add it to our map because 2 svgs might be rasterized at different sizes
		tex = std::shared_ptr<SVGResource>(new SVGResource(key.first, tile));
		sTextureList.push_back(tex); // add it to our list though
		rm->addReloadable(tex);
		tex->reload(rm);
		return tex;
	}

	// normal texture; embedded resources (":/…") are small and always synchronous
	tex = std::shared_ptr<TextureResource>(new TextureResource(key.first, tile));
	tex->mDynamic = dynamic && !key.first.starts_with(":/");
	sTextureMap[key] = std::weak_ptr<TextureResource>(tex);
	sTextureList.push_back(tex);
	sCache.touch(key, tex);
	rm->addReloadable(tex);
	tex->reload(rm);
	if(!tex->isLoading())
		trimCache();
	return tex;
}

void TextureResource::processPendingLoads()
{
	if(sPendingLoads.empty())
		return;

	bool uploaded = false;
	for(TextureLoader::Result& result : TextureLoader::instance().takeFinished(kMaxUploadsPerFrame))
	{
		const auto pending = sPendingLoads.find(result.ticket);
		if(pending == sPendingLoads.end())
			continue; // cancelled after it finished
		TextureResource* tex = pending->second;
		sPendingLoads.erase(pending);

		tex->mPendingLoad = 0;
		if(!result.image)
		{
			LOG(LogError) << "Could not load image \"" << result.path << "\": " << result.image.error();
			continue;
		}
		tex->initFromImage(*result.image);
		uploaded = true;
	}

	if(uploaded)
		trimCache();
}

void TextureResource::trimCache()
{
	const int maxVRAM = Settings::getInstance()->getInt("MaxVRAM");
	const size_t budget = size_t(maxVRAM > 0 ? maxVRAM : kDefaultMaxVRAM) * 1024 * 1024;
	const size_t used = getTotalMemUsage();
	if(used > budget)
		sCache.trim(used, budget, [](const TextureResource& tex) { return tex.getMemUsage(); });
}

void TextureResource::clearCache()
{
	sCache.clear();
}

bool TextureResource::isInitialized() const
{
	return mTexture != 0;
}

size_t TextureResource::getMemUsage() const
{
	if(!mTexture || mTextureSize.x == 0 || mTextureSize.y == 0)
		return 0;

	return size_t(mTextureSize.x) * mTextureSize.y * 4 * std::max<size_t>(mFrames.size(), 1);
}

size_t TextureResource::getTotalMemUsage()
{
	size_t total = 0;

	auto it = sTextureList.begin();
	while(it != sTextureList.end())
	{
		if((*it).expired())
		{
			// remove expired textures from the list
			it = sTextureList.erase(it);
			continue;
		}

		total += (*it).lock()->getMemUsage();
		it++;
	}

	return total;
}
