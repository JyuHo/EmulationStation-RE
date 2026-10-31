#pragma once

#include "resources/TextureResource.h"

struct NSVGimage;

class SVGResource : public TextureResource
{
public:
	~SVGResource() override;

	void unload(std::shared_ptr<ResourceManager>& rm) override;

	void initFromMemory(const char* image, size_t length) override;

	void rasterizeAt(size_t width, size_t height);
	glm::vec2 getSourceImageSize() const;

protected:
	friend TextureResource;
	SVGResource(const std::string& path, bool tile);
	void deinitSVG();

	NSVGimage* mSVGImage;
	size_t mLastWidth;
	size_t mLastHeight;
};
