#include "components/AnimatedImageComponent.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <span>
#include <vector>

#include "ImageIO.h"
#include "Log.h"

AnimatedImageComponent::AnimatedImageComponent(Window* window) : GuiComponent(window), mEnabled(false) {}

void AnimatedImageComponent::load(const AnimationDef* def)
{
	mFrames.clear();

	assert(def->frameCount >= 1);

	for(size_t i = 0; i < def->frameCount; i++)
	{
		if(def->frames[i].path != nullptr && !ResourceManager::getInstance()->fileExists(def->frames[i].path))
		{
			LOG(LogError) << "Missing animation frame " << i << " (\"" << def->frames[i].path << "\")";
			continue;
		}

		auto img = std::make_unique<ImageComponent>(mWindow);
		img->setResize(mSize.x, mSize.y);
		img->setImage(std::string(def->frames[i].path), false);

		mFrames.emplace_back(std::move(img), def->frames[i].time);
	}

	mLoop = def->loop;

	mCurrentFrame = 0;
	mFrameAccumulator = 0;
	mEnabled = true;
}

void AnimatedImageComponent::reset()
{
	mCurrentFrame = 0;
	mFrameAccumulator = 0;
}

void AnimatedImageComponent::onSizeChanged()
{
	for(auto it = mFrames.begin(); it != mFrames.end(); it++)
	{
		it->first->setResize(mSize.x, mSize.y);
	}
}

void AnimatedImageComponent::update(int deltaTime)
{
	if(!mEnabled || mFrames.size() == 0)
		return;

	mFrameAccumulator += deltaTime;

	while(mFrames.at(mCurrentFrame).second <= mFrameAccumulator)
	{
		mCurrentFrame++;

		if(mCurrentFrame == mFrames.size())
		{
			if(mLoop)
			{
				// restart
				mCurrentFrame = 0;
			}
			else
			{
				// done, stop at last frame
				mCurrentFrame--;
				mEnabled = false;
				break;
			}
		}

		mFrameAccumulator -= mFrames.at(mCurrentFrame).second;
	}
}

void AnimatedImageComponent::render(const glm::mat4& trans)
{
	if(mFrames.size())
		mFrames.at(mCurrentFrame).first->render(getTransform() * trans);
}

void AnimatedImageComponent::loadAnimated(const std::string& path)
{
	mFrames.clear();

	std::ifstream in(path, std::ios::binary);
	if(!in)
	{
		LOG(LogError) << "Missing animated image \"" << path << "\"";
		return;
	}
	std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

	std::vector<std::byte> b(bytes.size());
	for(size_t i = 0; i < bytes.size(); i++)
		b[i] = static_cast<std::byte>(bytes[i]);
	const auto img = ImageIO::load(std::span<const std::byte>(b));
	if(!img.has_value() || img->frames.empty())
	{
		LOG(LogError) << "Could not decode animated image \"" << path << "\"";
		return;
	}

	for(const auto& f : img->frames)
	{
		auto comp = std::make_unique<ImageComponent>(mWindow);
		comp->setResize(mSize.x, mSize.y);
		comp->setImageFromPixels(f.rgba.data(), img->width, img->height, false);
		mFrames.emplace_back(std::move(comp), f.durationMs > 0 ? f.durationMs : 100);
	}

	mLoop = true;
	mCurrentFrame = 0;
	mFrameAccumulator = 0;
	mEnabled = true;
}
