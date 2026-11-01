#pragma once

#include "InputConfig.h"
#include <functional>
#include <memory>
#include "math/Math.h"
#include "HelpStyle.h"

class Window;
class Animation;
class AnimationController;
class ThemeData;
class Font;

// button, translated action
using HelpPrompt = std::pair<const char*, std::string>;

class GuiComponent
{
public:
	GuiComponent(Window* window);
	virtual ~GuiComponent();

	virtual void textInput(const char* text);

	//Called when input is received.
	//Return true if the input is consumed, false if it should continue to be passed to other children.
	virtual bool input(InputConfig* config, Input input);

	//Called when time passes.  Default implementation calls updateSelf(deltaTime) and updateChildren(deltaTime) - so you should probably call GuiComponent::update(deltaTime) at some point (or at least updateSelf so animations work).
	virtual void update(int deltaTime);

	//Called when it's time to render.  By default, just calls renderChildren(parentTrans * getTransform()).
	//You probably want to override this like so:
	//1. Calculate the new transform that your control will draw at with glm::mat4 t = parentTrans * getTransform().
	//2. Set the renderer to use that new transform as the model matrix - Renderer::setMatrix(t);
	//3. Draw your component.
	//4. Tell your children to render, based on your component's transform - renderChildren(t).
	virtual void render(const glm::mat4& parentTrans);

	glm::vec3 getPosition() const;
	void setPosition(const glm::vec3& offset);
	void setPosition(float x, float y, float z = 0.0f);
	virtual void onPositionChanged() {};

	glm::vec2 getSize() const;
	void setSize(const glm::vec2& size);
	void setSize(float w, float h);
	virtual void onSizeChanged() {};

	void setParent(GuiComponent* parent);
	GuiComponent* getParent() const;

	void addChild(GuiComponent* cmp);
	void removeChild(GuiComponent* cmp);
	void clearChildren();
	unsigned int getChildCount() const;
	// stable: children with the same zIndex keep their order
	void sortChildren();
	GuiComponent* getChild(unsigned int i) const;

	// animation will be automatically deleted when it completes or is stopped.
	bool isAnimationPlaying(unsigned char slot) const;
	bool isAnimationReversed(unsigned char slot) const;
	int getAnimationTime(unsigned char slot) const;
	void setAnimation(std::unique_ptr<Animation> animation, int delay = 0, std::function<void()> finishedCallback = nullptr,
		bool reverse = false, unsigned char slot = 0);
	bool stopAnimation(unsigned char slot);
	bool cancelAnimation(unsigned char
			slot); // Like stopAnimation, but doesn't call finishedCallback - only removes the animation, leaving things in their current state.  Returns true if successful (an animation was in this slot).
	bool finishAnimation(unsigned char
			slot); // Calls update(1.f) and finishedCallback, then deletes the animation - basically skips to the end.  Returns true if successful (an animation was in this slot).
	bool advanceAnimation(
		unsigned char slot, unsigned int time); // Returns true if successful (an animation was in this slot).
	void stopAllAnimations();
	void cancelAllAnimations();

	virtual unsigned char getOpacity() const;
	virtual void setOpacity(unsigned char opacity);

	const glm::mat4& getTransform();

	// Theme properties common to the elements (docs/THEMES.md section 11, rows "11d").
	// zIndex: drawing order among the children of the parent, after sortChildren().
	float getZIndex() const { return mZIndex; }
	void setZIndex(float zIndex) { mZIndex = zIndex; }
	// value given to zIndex when the theme element has none (default order of each view); also
	// the zIndex itself, for a component that its theme does not mention
	float getDefaultZIndex() const { return mDefaultZIndex; }
	void setDefaultZIndex(float zIndex) { mDefaultZIndex = mZIndex = zIndex; }
	// rotation in degrees, clockwise, around rotationOrigin (fractions of the size)
	float getRotation() const { return mRotation; }
	void setRotation(float degrees) { mRotation = degrees; }
	glm::vec2 getRotationOrigin() const { return mRotationOrigin; }
	void setRotationOrigin(const glm::vec2& origin) { mRotationOrigin = origin; }
	// a theme-disabled component is not drawn by its parent
	bool isThemeDisabled() const { return mThemeDisabled; }
	void setThemeDisabled(bool disabled) { mThemeDisabled = disabled; }
	// point of the component that its position refers to, in fractions of its size
	virtual glm::vec2 getOrigin() const { return {0.0f, 0.0f}; }

	virtual std::string getValue() const;
	virtual void setValue(const std::string& value);

	virtual void onFocusGained() {};
	virtual void onFocusLost() {};

	// The component's view becomes the shown one, or stops being (another view, a game
	// launched, sleep). Forwarded to the children.
	virtual void onShow();
	virtual void onHide();

	// Default implementation just handles <pos> and <size> tags as normalized float pairs.
	// You probably want to keep this behavior for any derived classes as well as add your own.
	virtual void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view,
		const std::string& element, unsigned int properties);

	// Returns a list of help prompts.
	virtual std::vector<HelpPrompt> getHelpPrompts() { return std::vector<HelpPrompt>(); };

	// Called whenever help prompts change.
	void updateHelpPrompts();

	virtual HelpStyle getHelpStyle();

protected:
	void renderChildren(const glm::mat4& transform) const;
	// zIndex, rotation, rotationOrigin and disabled of the theme element, for the
	// components whose applyTheme does not call GuiComponent::applyTheme
	void applyCommonTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view,
		const std::string& element, unsigned int properties);
	void updateSelf(int deltaTime);		// updates animations
	void updateChildren(int deltaTime); // updates animations

	unsigned char mOpacity;
	Window* mWindow;

	GuiComponent* mParent;
	std::vector<GuiComponent*> mChildren;

	glm::vec3 mPosition;
	glm::vec2 mSize;

	float mZIndex = 0.0f;
	float mDefaultZIndex = 0.0f;
	float mRotation = 0.0f;
	glm::vec2 mRotationOrigin{0.5f, 0.5f};
	bool mThemeDisabled = false;

public:
	const static unsigned char MAX_ANIMATIONS = 4;

private:
	glm::mat4 mTransform; //Don't access this directly! Use getTransform()!
	std::unique_ptr<AnimationController> mAnimationMap[MAX_ANIMATIONS];
};
