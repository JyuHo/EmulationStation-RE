#include "GuiComponent.h"
#include <algorithm>
#include <memory>
#include <cassert>
#include <utility>
#include "Window.h"
#include "Log.h"
#include "renderer/Renderer.h"
#include "animations/AnimationController.h"
#include "ThemeData.h"

GuiComponent::GuiComponent(Window* window) :
	mOpacity(255),
	mWindow(window),
	mParent(nullptr),
	mPosition(glm::vec3(0.0f)),
	mSize(glm::vec2(0.0f)),
	mTransform(glm::mat4(1.0f))
{}

GuiComponent::~GuiComponent()
{
	mWindow->removeGui(this);

	cancelAllAnimations();

	if(mParent)
		mParent->removeChild(this);

	for(unsigned int i = 0; i < getChildCount(); i++)
		getChild(i)->setParent(nullptr);
}

bool GuiComponent::input(InputConfig* config, Input input)
{
	for(unsigned int i = 0; i < getChildCount(); i++)
	{
		if(getChild(i)->input(config, input))
			return true;
	}

	return false;
}

void GuiComponent::updateSelf(int deltaTime)
{
	for(unsigned char i = 0; i < MAX_ANIMATIONS; i++)
		advanceAnimation(i, deltaTime);
}

void GuiComponent::updateChildren(int deltaTime)
{
	for(unsigned int i = 0; i < getChildCount(); i++)
	{
		getChild(i)->update(deltaTime);
	}
}

void GuiComponent::update(int deltaTime)
{
	updateSelf(deltaTime);
	updateChildren(deltaTime);
}

void GuiComponent::render(const glm::mat4& parentTrans)
{
	glm::mat4 trans = parentTrans * getTransform();
	renderChildren(trans);
}

void GuiComponent::renderChildren(const glm::mat4& transform) const
{
	for(unsigned int i = 0; i < getChildCount(); i++)
	{
		if(!getChild(i)->isThemeDisabled())
			getChild(i)->render(transform);
	}
}

void GuiComponent::sortChildren()
{
	std::ranges::stable_sort(mChildren, {}, &GuiComponent::getZIndex);
}

glm::vec3 GuiComponent::getPosition() const
{
	return mPosition;
}

void GuiComponent::setPosition(const glm::vec3& offset)
{
	mPosition = offset;
	onPositionChanged();
}

void GuiComponent::setPosition(float x, float y, float z)
{
	mPosition = {x, y, z};
	onPositionChanged();
}

glm::vec2 GuiComponent::getSize() const
{
	return mSize;
}

void GuiComponent::setSize(const glm::vec2& size)
{
	mSize = size;
	onSizeChanged();
}

void GuiComponent::setSize(float w, float h)
{
	mSize = {w, h};
	onSizeChanged();
}

//Children stuff.
void GuiComponent::addChild(GuiComponent* cmp)
{
	mChildren.push_back(cmp);

	if(cmp->getParent())
		cmp->getParent()->removeChild(cmp);

	cmp->setParent(this);
}

void GuiComponent::removeChild(GuiComponent* cmp)
{
	if(!cmp->getParent())
		return;

	if(cmp->getParent() != this)
	{
		LOG(LogError) << "Tried to remove child from incorrect parent!";
	}

	cmp->setParent(nullptr);

	for(auto i = mChildren.begin(); i != mChildren.end(); i++)
	{
		if(*i == cmp)
		{
			mChildren.erase(i);
			return;
		}
	}
}

void GuiComponent::clearChildren()
{
	mChildren.clear();
}

unsigned int GuiComponent::getChildCount() const
{
	return mChildren.size();
}

GuiComponent* GuiComponent::getChild(unsigned int i) const
{
	return mChildren.at(i);
}

void GuiComponent::setParent(GuiComponent* parent)
{
	mParent = parent;
}

GuiComponent* GuiComponent::getParent() const
{
	return mParent;
}

void GuiComponent::onShow()
{
	for(GuiComponent* child : mChildren)
		child->onShow();
}

void GuiComponent::onHide()
{
	for(GuiComponent* child : mChildren)
		child->onHide();
}

unsigned char GuiComponent::getOpacity() const
{
	return mOpacity;
}

void GuiComponent::setOpacity(unsigned char opacity)
{
	mOpacity = opacity;
	for(auto it = mChildren.begin(); it != mChildren.end(); it++)
	{
		(*it)->setOpacity(opacity);
	}
}

const glm::mat4& GuiComponent::getTransform()
{
	mTransform = glm::mat4(1.0f);
	mTransform = glm::translate(mTransform, mPosition);
	if(mRotation != 0.0f)
	{
		// the position is the origin point: the pivot is rotationOrigin, relative to it
		const glm::vec2 pivot = (mRotationOrigin - getOrigin()) * mSize;
		mTransform = glm::translate(mTransform, glm::vec3(pivot, 0.0f));
		mTransform = glm::rotate(mTransform, glm::radians(mRotation), glm::vec3(0.0f, 0.0f, 1.0f));
		mTransform = glm::translate(mTransform, glm::vec3(-pivot, 0.0f));
	}
	return mTransform;
}

void GuiComponent::setValue(const std::string& value) {}

std::string GuiComponent::getValue() const
{
	return "";
}

void GuiComponent::textInput(const char* text)
{
	for(auto iter = mChildren.begin(); iter != mChildren.end(); iter++)
	{
		(*iter)->textInput(text);
	}
}

void GuiComponent::setAnimation(
	std::unique_ptr<Animation> anim, int delay, std::function<void()> finishedCallback, bool reverse, unsigned char slot)
{
	assert(slot < MAX_ANIMATIONS);

	// the old controller runs its finished callback when destroyed, after the new one is in place
	std::unique_ptr<AnimationController> oldAnim = std::exchange(mAnimationMap[slot],
		std::make_unique<AnimationController>(std::move(anim), delay, std::move(finishedCallback), reverse));
}

// The slot is emptied before the controller is destroyed: its finished callback may start a new
// animation in the same slot, or delete this component.
bool GuiComponent::stopAnimation(unsigned char slot)
{
	assert(slot < MAX_ANIMATIONS);
	std::unique_ptr<AnimationController> anim = std::move(mAnimationMap[slot]);
	return anim != nullptr;
}

bool GuiComponent::cancelAnimation(unsigned char slot)
{
	assert(slot < MAX_ANIMATIONS);
	std::unique_ptr<AnimationController> anim = std::move(mAnimationMap[slot]);
	if(!anim)
		return false;
	anim->removeFinishedCallback();
	return true;
}

bool GuiComponent::finishAnimation(unsigned char slot)
{
	assert(slot < MAX_ANIMATIONS);
	std::unique_ptr<AnimationController> anim = std::move(mAnimationMap[slot]);
	if(!anim)
		return false;

	// skip to animation's end; destroying anim then calls finishedCallback
	[[maybe_unused]] const bool done = anim->update(anim->getAnimation()->getDuration() - anim->getTime());
	assert(done);
	return true;
}

bool GuiComponent::advanceAnimation(unsigned char slot, unsigned int time)
{
	assert(slot < MAX_ANIMATIONS);
	if(!mAnimationMap[slot])
		return false;

	if(mAnimationMap[slot]->update(time))
	{
		std::unique_ptr<AnimationController> done = std::move(mAnimationMap[slot]);
	}
	return true;
}

void GuiComponent::stopAllAnimations()
{
	for(unsigned char i = 0; i < MAX_ANIMATIONS; i++)
		stopAnimation(i);
}

void GuiComponent::cancelAllAnimations()
{
	for(unsigned char i = 0; i < MAX_ANIMATIONS; i++)
		cancelAnimation(i);
}

bool GuiComponent::isAnimationPlaying(unsigned char slot) const
{
	return mAnimationMap[slot] != nullptr;
}

bool GuiComponent::isAnimationReversed(unsigned char slot) const
{
	assert(mAnimationMap[slot] != NULL);
	return mAnimationMap[slot]->isReversed();
}

int GuiComponent::getAnimationTime(unsigned char slot) const
{
	assert(mAnimationMap[slot] != NULL);
	return mAnimationMap[slot]->getTime();
}

void GuiComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view,
	const std::string& element, unsigned int properties)
{
	glm::vec2 scale = getParent() ? getParent()->getSize()
								  : glm::vec2((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "");
	if(!elem)
		return;

	using namespace ThemeFlags;
	if(properties & POSITION && elem->has("pos"))
	{
		glm::vec2 denormalized = elem->get<glm::vec2>("pos") * (scale);
		setPosition(glm::vec3(denormalized.x, denormalized.y, 0));
	}

	if(properties & ThemeFlags::SIZE && elem->has("size"))
		setSize(elem->get<glm::vec2>("size") * (scale));

	applyCommonTheme(theme, view, element, properties);
}

void GuiComponent::applyCommonTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view,
	const std::string& element, unsigned int properties)
{
	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "");
	if(!elem)
		return;

	// docs/THEMES.md section 11 (Recalbox components/base/Themable.cpp:66-77)
	using namespace ThemeFlags;
	if(properties & ROTATION)
	{
		setRotation(elem->has("rotation") ? elem->get<float>("rotation") : 0.0f);
		setRotationOrigin(elem->has("rotationOrigin") ? elem->get<glm::vec2>("rotationOrigin") : glm::vec2(0.0f));
	}
	setZIndex(properties & Z_INDEX && elem->has("zIndex") ? elem->get<float>("zIndex") : mDefaultZIndex);
	if(properties & POSITION)
		setThemeDisabled(elem->has("disabled") && elem->get<bool>("disabled"));
}

void GuiComponent::updateHelpPrompts()
{
	if(getParent())
	{
		getParent()->updateHelpPrompts();
		return;
	}

	std::vector<HelpPrompt> prompts = getHelpPrompts();

	if(mWindow->peekGui() == this)
		mWindow->setHelpPrompts(prompts, getHelpStyle());
}

HelpStyle GuiComponent::getHelpStyle()
{
	return HelpStyle();
}
