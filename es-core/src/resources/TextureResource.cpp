#include "resources/TextureResource.h"
#include <cassert>
#include "Log.h"
#include "ImageIO.h"
#include "Settings.h"
#include "renderer/Renderer.h"
#include "Util.h"
#include "resources/SVGResource.h"

std::map<TextureResource::TextureKeyType, std::weak_ptr<TextureResource>> TextureResource::sTextureMap;
std::list<std::weak_ptr<TextureResource>> TextureResource::sTextureList;

TextureResource::TextureResource(const std::string& path, bool tile) :
	mTextureSize(glm::ivec2(0)), mPath(path), mTile(tile), mTexture(0)
{}

TextureResource::~TextureResource()
{
	if(mWorker.joinable())
		mWorker.join();
	deinit();
}

void TextureResource::unload(std::shared_ptr<ResourceManager>& rm)
{
	deinit();
}

void TextureResource::reload(std::shared_ptr<ResourceManager>& rm)
{
	if(mPath.empty())
		return;

	// SVGs are rasterized synchronously (size-dependent).
	if(mPath.size() >= 4 && mPath.compare(mPath.size() - 4, 4, ".svg") == 0)
	{
		const ResourceData& data = rm->getFileData(mPath);
		initFromMemory((const char*)data.ptr.get(), data.length);
		return;
	}

	const ResourceData& data = rm->getFileData(mPath);
	std::vector<unsigned char> bytes(data.ptr.get(), data.ptr.get() + data.length);
	const std::string path = mPath;

	if(mWorker.joinable())
		mWorker.join();

	mWorker = std::jthread([this, bytes = std::move(bytes)]() {
		size_t w = 0, h = 0;
		std::vector<unsigned char> imageRGBA = ImageIO::loadFromMemoryRGBA32(bytes.data(), bytes.size(), w, h);
		if(imageRGBA.empty())
		{
			LOG(LogError) << "Could not initialize texture from memory, invalid data! (file path: " << mPath << ")";
			return;
		}
		std::lock_guard<std::mutex> lock(mPendingMutex);
		mPendingPixels = std::move(imageRGBA);
		mPendingW = w;
		mPendingH = h;
		mPendingReady = true;
	});
}

void TextureResource::initFromPixels(const unsigned char* dataRGBA, size_t width, size_t height)
{
	deinit();

	assert(width > 0 && height > 0);

	mTexture = Renderer::createTexture(Renderer::TextureFormat::RGBA8, Renderer::TextureFilter::LinearMinNearestMag,
		mTile, glm::ivec2(static_cast<int>(width), static_cast<int>(height)), dataRGBA);

	mTextureSize = {width, height};
}

void TextureResource::initFromMemory(const char* data, size_t length)
{
	size_t width, height;
	std::vector<unsigned char> imageRGBA =
		ImageIO::loadFromMemoryRGBA32((const unsigned char*)(data), length, width, height);

	if(imageRGBA.size() == 0)
	{
		LOG(LogError) << "Could not initialize texture from memory, invalid data!  (file path: " << mPath
					  << ", data ptr: " << (size_t)data << ", reported size: " << length << ")";
		return;
	}

	initFromPixels(imageRGBA.data(), width, height);
}

void TextureResource::deinit()
{
	if(mTexture != 0)
	{
		Renderer::destroyTexture(mTexture);
		mTexture = 0;
	}
}

const glm::ivec2& TextureResource::getSize() const
{
	return mTextureSize;
}

bool TextureResource::isTiled() const
{
	return mTile;
}

std::shared_ptr<TextureResource> TextureResource::get(const std::string& path, bool tile)
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
		if(!foundTexture->second.expired())
		{
			auto locked = foundTexture->second.lock();
			// LRU: move to back of the list
			for(auto it = sTextureList.begin(); it != sTextureList.end(); ++it)
			{
				if(it->expired() || it->lock() == locked)
				{
					if(!it->expired())
						sTextureList.splice(sTextureList.end(), sTextureList, it);
					break;
				}
			}
			return locked;
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
	else
	{
		// normal texture
		tex = std::shared_ptr<TextureResource>(new TextureResource(key.first, tile));
		sTextureMap[key] = std::weak_ptr<TextureResource>(tex);
		sTextureList.push_back(tex);
		rm->addReloadable(tex);
		tex->reload(ResourceManager::getInstance());
		evictIfNeeded();
		return tex;
	}
}

void TextureResource::processPending()
{
	auto it = sTextureList.begin();
	while(it != sTextureList.end())
	{
		if(it->expired())
		{
			it = sTextureList.erase(it);
			continue;
		}
		auto locked = it->lock();
		if(!locked)
		{
			++it;
			continue;
		}
		bool ready = false;
		{
			std::lock_guard<std::mutex> lock(locked->mPendingMutex);
			ready = locked->mPendingReady;
		}
		if(ready && !locked->isInitialized())
		{
			std::vector<unsigned char> pixels;
			size_t w = 0, h = 0;
			{
				std::lock_guard<std::mutex> lock(locked->mPendingMutex);
				pixels = std::move(locked->mPendingPixels);
				w = locked->mPendingW;
				h = locked->mPendingH;
				locked->mPendingReady = false;
			}
			if(!pixels.empty())
				locked->initFromPixels(pixels.data(), w, h);
		}
		++it;
	}
}

void TextureResource::evictIfNeeded()
{
	const int maxVramMb = Settings::getInstance() ? Settings::getInstance()->getInt("MaxVRAM") : 0;
	size_t budget = (maxVramMb > 0 ? (size_t)maxVramMb : 256) * 1024 * 1024;
	if(getTotalMemUsage() <= budget)
		return;

	// Evict from the front (least recently used) while over budget.
	auto it = sTextureList.begin();
	while(it != sTextureList.end() && getTotalMemUsage() > budget)
	{
		if(it->expired())
		{
			it = sTextureList.erase(it);
			continue;
		}
		auto locked = it->lock();
		// Don't evict textures with multiple references (in use).
		if(locked && locked.use_count() <= 2)
		{
			locked->deinit();
			it = sTextureList.erase(it);
		}
		else
		{
			++it;
		}
	}
}

bool TextureResource::isInitialized() const
{
	return mTexture != 0;
}

size_t TextureResource::getMemUsage() const
{
	if(!mTexture || mTextureSize.x == 0 || mTextureSize.y == 0)
		return 0;

	return mTextureSize.x * mTextureSize.y * 4;
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
