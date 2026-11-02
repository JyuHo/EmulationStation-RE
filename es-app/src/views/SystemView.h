#pragma once

#include "GuiComponent.h"
#include "components/ImageComponent.h"
#include "components/TextComponent.h"
#include "components/ScrollableContainer.h"
#include "components/IList.h"
#include "resources/TextureResource.h"
#include "views/SystemCarousel.h"

class SystemData;
class AnimatedImageComponent;

struct SystemViewData
{
	std::shared_ptr<GuiComponent> logo;
	std::shared_ptr<GuiComponent> logoSelected; // ES layout only
	std::shared_ptr<GuiComponent> logoText;		// themed carousel: name over a "default" logo, or alone
	std::shared_ptr<ThemeExtras> backgroundExtras;
};

class SystemView : public IList<SystemViewData, SystemData*>
{
public:
	SystemView(Window* window);

	void goToSystem(SystemData* system, bool animate);

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void render(const glm::mat4& parentTrans) override;

	std::vector<HelpPrompt> getHelpPrompts() override;
	HelpStyle getHelpStyle() override;

protected:
	void onCursorChanged(const CursorState& state) override;

private:
	inline glm::vec2 logoSize() const { return glm::vec2(mSize.x * 0.25f, mSize.y * 0.155f); }

	void populate();
	// themed system view (main theme with a carousel or a systemInfo, docs/THEMES.md PR 11g)
	void populateThemed(const std::shared_ptr<ThemeData>& mainTheme);
	void makeThemedLogo(SystemData* system, SystemViewData& data);
	void renderLegacy(const glm::mat4& trans);
	void renderThemed(const glm::mat4& trans);
	void renderExtras(const glm::mat4& trans, float lower, float upper);
	void renderCarousel(const glm::mat4& trans);
	void renderTextList(const glm::mat4& trans);
	void renderInfoBar(const glm::mat4& trans);
	bool isVertical() const;

	TextComponent mSystemInfo;

	bool mThemed = false;
	es::carousel::Carousel mCarousel;
	std::shared_ptr<Font> mCarouselFontLarge;
	std::shared_ptr<Font> mCarouselFontMedium;
	unsigned int mInfoBackground = 0xDDDDDDD8;

	// unit is list index
	float mCamOffset;
	float mExtrasCamOffset;
	float mExtrasFadeOpacity;
};
