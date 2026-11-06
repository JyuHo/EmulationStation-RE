#pragma once

#include <string>
#include "components/ImageComponent.h"
#include "components/TextComponent.h"

class Window;
class TextureResource;

#define DEFAULT_SPLASH_IMAGE ":/logo.png"
#define OLD_SPLASH_LAYOUT false

class Splash
{
public:
	Splash(Window* window, const std::string image, bool fullScreenBackGround = true, IBindable* bindable = nullptr);
	~Splash();

	void update(std::string text, float percent = -1);
	void render(float opacity, bool swapBuffers = true);

private:
	ImageComponent  mBackground;
	TextComponent   mText;
	float			mPercent;

	ImageComponent  mInactiveProgressbar;
	ImageComponent  mActiveProgressbar;

	unsigned int	mBackgroundColor;
	float			mRoundCorners;

	std::shared_ptr<TextureResource> mTexture;

	std::vector<GuiComponent*> mExtras;
} ;
