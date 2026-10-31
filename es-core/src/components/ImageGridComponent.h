#pragma once

#include "GuiComponent.h"
#include "components/IList.h"
#include "components/ImageComponent.h"
#include "Log.h"

struct ImageGridData
{
	std::shared_ptr<TextureResource> texture;
};

template <typename T> class ImageGridComponent : public IList<ImageGridData, T>
{
protected:
	using IList<ImageGridData, T>::mEntries;
	using IList<ImageGridData, T>::listUpdate;
	using IList<ImageGridData, T>::listInput;
	using IList<ImageGridData, T>::listRenderTitleOverlay;
	using IList<ImageGridData, T>::getTransform;
	using IList<ImageGridData, T>::mSize;
	using IList<ImageGridData, T>::mCursor;
	using typename IList<ImageGridData, T>::Entry;
	using IList<ImageGridData, T>::mWindow;

public:
	using IList<ImageGridData, T>::size;
	using IList<ImageGridData, T>::isScrolling;
	using IList<ImageGridData, T>::stopScrolling;

	ImageGridComponent(Window* window);

	void add(const std::string& name, const std::string& imagePath, const T& obj);

	void onSizeChanged() override;

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void render(const glm::mat4& parentTrans) override;

private:
	glm::vec2 getSquareSize(std::shared_ptr<TextureResource> tex = nullptr) const
	{
		glm::vec2 aspect(1, 1);

		if(tex)
		{
			const glm::ivec2& texSize = tex->getSize();

			if(texSize.x > texSize.y)
				aspect[0] = (float)texSize.x / texSize.y;
			else
				aspect[1] = (float)texSize.y / texSize.x;
		}

		return glm::vec2(156 * aspect.x, 156 * aspect.y);
	};

	glm::vec2 getMaxSquareSize() const
	{
		glm::vec2 squareSize(32, 32);

		// calc biggest square size
		for(auto it = mEntries.begin(); it != mEntries.end(); it++)
		{
			glm::vec2 chkSize = getSquareSize(it->data.texture);
			if(chkSize.x > squareSize.x)
				squareSize[0] = chkSize[0];
			if(chkSize.y > squareSize.y)
				squareSize[1] = chkSize[1];
		}

		return squareSize;
	};

	glm::ivec2 getGridSize() const
	{
		glm::vec2 squareSize = getMaxSquareSize();
		glm::ivec2 gridSize(mSize.x / (squareSize.x + getPadding().x), mSize.y / (squareSize.y + getPadding().y));
		return gridSize;
	};

	glm::vec2 getPadding() const { return glm::vec2(24, 24); }

	void buildImages();
	void updateImages();

	void onCursorChanged(const CursorState& state) override;

	bool mEntriesDirty;

	std::vector<std::unique_ptr<ImageComponent>> mImages;
};

template <typename T> ImageGridComponent<T>::ImageGridComponent(Window* window) : IList<ImageGridData, T>(window)
{
	mEntriesDirty = true;
}

template <typename T>
void ImageGridComponent<T>::add(const std::string& name, const std::string& imagePath, const T& obj)
{
	typename IList<ImageGridData, T>::Entry entry;
	entry.name = name;
	entry.object = obj;
	entry.data.texture = ResourceManager::getInstance()->fileExists(imagePath)
		? TextureResource::get(imagePath, false, true)
		: TextureResource::get(":/button.png");
	static_cast<IList<ImageGridData, T>*>(this)->add(entry);
	mEntriesDirty = true;
}

template <typename T> bool ImageGridComponent<T>::input(InputConfig* config, Input input)
{
	if(input.value != 0)
	{
		glm::ivec2 dir = glm::ivec2(0);
		if(config->isMappedTo("up", input))
			dir[1] = -1;
		else if(config->isMappedTo("down", input))
			dir[1] = 1;
		else if(config->isMappedTo("left", input))
			dir[0] = -1;
		else if(config->isMappedTo("right", input))
			dir[0] = 1;

		if(dir != glm::ivec2(0))
		{
			listInput(dir.x + dir.y * getGridSize().x);
			return true;
		}
	}
	else
	{
		if(config->isMappedTo("up", input) || config->isMappedTo("down", input) || config->isMappedTo("left", input) ||
			config->isMappedTo("right", input))
		{
			stopScrolling();
		}
	}

	return GuiComponent::input(config, input);
}

template <typename T> void ImageGridComponent<T>::update(int deltaTime)
{
	listUpdate(deltaTime);
}

template <typename T> void ImageGridComponent<T>::render(const glm::mat4& parentTrans)
{
	glm::mat4 trans = getTransform() * parentTrans;

	if(mEntriesDirty)
	{
		buildImages();
		updateImages();
		mEntriesDirty = false;
	}

	for(auto& image : mImages)
		image->render(trans);

	GuiComponent::renderChildren(trans);
}

template <typename T> void ImageGridComponent<T>::onCursorChanged(const CursorState& state)
{
	updateImages();
}

template <typename T> void ImageGridComponent<T>::onSizeChanged()
{
	buildImages();
	updateImages();
}

// create and position imagecomponents (mImages)
template <typename T> void ImageGridComponent<T>::buildImages()
{
	mImages.clear();

	glm::ivec2 gridSize = getGridSize();
	glm::vec2 squareSize = getMaxSquareSize();
	glm::vec2 padding = getPadding();

	// attempt to center within our size
	glm::vec2 totalSize(gridSize.x * (squareSize.x + padding.x), gridSize.y * (squareSize.y + padding.y));
	glm::vec2 offset(mSize.x - totalSize.x, mSize.y - totalSize.y);
	offset /= 2;

	for(int y = 0; y < gridSize.y; y++)
	{
		for(int x = 0; x < gridSize.x; x++)
		{
			mImages.push_back(std::make_unique<ImageComponent>(mWindow));
			ImageComponent& image = *mImages.back();

			image.setPosition(
				(squareSize.x + padding.x) * (x + 0.5f) + offset.x, (squareSize.y + padding.y) * (y + 0.5f) + offset.y);
			image.setOrigin(0.5f, 0.5f);
			image.setResize(squareSize.x, squareSize.y);
			image.setImage("");
		}
	}
}

template <typename T> void ImageGridComponent<T>::updateImages()
{
	if(mImages.empty())
		buildImages();

	glm::ivec2 gridSize = getGridSize();

	int cursorRow = mCursor / gridSize.x;

	int start = (cursorRow - (gridSize.y / 2)) * gridSize.x;

	//if we're at the end put the row as close as we can and no higher
	if(start + (gridSize.x * gridSize.y) >= (int)mEntries.size())
		start = gridSize.x * ((int)mEntries.size() / gridSize.x - gridSize.y + 1);

	if(start < 0)
		start = 0;

	unsigned int i = (unsigned int)start;
	for(unsigned int img = 0; img < mImages.size(); img++)
	{
		ImageComponent& image = *mImages.at(img);
		if(i >= (unsigned int)size())
		{
			image.setImage("");
			continue;
		}

		glm::vec2 squareSize = getSquareSize(mEntries.at(i).data.texture);
		if(i == mCursor)
		{
			image.setColorShift(0xFFFFFFFF);
			image.setResize(squareSize.x + getPadding().x * 0.95f, squareSize.y + getPadding().y * 0.95f);
		}
		else
		{
			image.setColorShift(0xAAAAAABB);
			image.setResize(squareSize.x, squareSize.y);
		}

		image.setImage(mEntries.at(i).data.texture);
		i++;
	}
}
