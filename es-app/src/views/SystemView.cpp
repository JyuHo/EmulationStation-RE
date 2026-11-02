#include "views/SystemView.h"
#include "utils/I18n.h"

#include <format>
#include <memory>
#include "SystemData.h"
#include "renderer/Renderer.h"
#include "Log.h"
#include "Window.h"
#include "views/ViewController.h"
#include "animations/LambdaAnimation.h"
#include "SystemData.h"
#include "Settings.h"
#include "Util.h"
#include "resources/ResourceManager.h"

#include <filesystem>
#include <limits>

#define SELECTED_SCALE 1.5f
#define LOGO_PADDING ((logoSize().x * (SELECTED_SCALE - 1) / 2) + (mSize.x * 0.06f))
#define BAND_HEIGHT (logoSize().y * SELECTED_SCALE)

SystemView::SystemView(Window* window) :
	IList<SystemViewData, SystemData*>(window, LIST_SCROLL_STYLE_SLOW, LIST_ALWAYS_LOOP),
	mSystemInfo(window, "SYSTEM INFO", Font::get(FONT_SIZE_SMALL), 0x33333300, ALIGN_CENTER)
{
	mCamOffset = 0;
	mExtrasCamOffset = 0;
	mExtrasFadeOpacity = 0.0f;

	setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	mSystemInfo.setSize(mSize.x, mSystemInfo.getSize().y * 1.333f);
	mSystemInfo.setPosition(0, (mSize.y + BAND_HEIGHT) / 2);

	populate();
}

void SystemView::populate()
{
	mEntries.clear();

	// a main theme with a carousel or a systemInfo: Recalbox's system view (PR 11g)
	const auto mainTheme = std::const_pointer_cast<ThemeData>(mWindow->getMainTheme());
	mThemed = es::carousel::isThemed(mainTheme.get());
	if(mThemed)
		populateThemed(mainTheme);

	for(auto it = SystemData::sSystemVector.begin(); it != SystemData::sSystemVector.end(); it++)
	{
		const std::shared_ptr<ThemeData>& theme = (*it)->getTheme();

		Entry e;
		e.name = (*it)->getName();
		e.object = *it;

		// make logo
		if(mThemed)
			makeThemedLogo(*it, e.data);
		else if(theme->getElement("system", "logo", "image"))
		{
			auto logo = std::make_shared<ImageComponent>(mWindow);
			logo->setMaxSize(glm::vec2(logoSize().x, logoSize().y));
			logo->applyTheme((*it)->getTheme(), "system", "logo", ThemeFlags::PATH);
			logo->setPosition((logoSize().x - logo->getSize().x) / 2, (logoSize().y - logo->getSize().y) / 2); // center
			e.data.logo = logo;

			auto logoSelected = std::make_shared<ImageComponent>(mWindow);
			logoSelected->setMaxSize(glm::vec2(logoSize().x * SELECTED_SCALE, logoSize().y * SELECTED_SCALE * 0.70f));
			logoSelected->applyTheme((*it)->getTheme(), "system", "logo", ThemeFlags::PATH);
			logoSelected->setPosition((logoSize().x - logoSelected->getSize().x) / 2,
				(logoSize().y - logoSelected->getSize().y) / 2); // center
			e.data.logoSelected = logoSelected;
		}
		else
		{
			// no logo in theme; use text
			auto text = std::make_shared<TextComponent>(mWindow, (*it)->getName(), Font::get(FONT_SIZE_LARGE), 0x000000FF, ALIGN_CENTER);
			text->setSize(logoSize());
			e.data.logo = text;

			auto textSelected = std::make_shared<TextComponent>(mWindow, (*it)->getName(),
				Font::get((int)(FONT_SIZE_LARGE * SELECTED_SCALE)), 0x000000FF, ALIGN_CENTER);
			textSelected->setSize(logoSize());
			e.data.logoSelected = textSelected;
		}

		// make background extras
		e.data.backgroundExtras = std::make_shared<ThemeExtras>(mWindow);
		e.data.backgroundExtras->setExtras(ThemeData::makeExtras((*it)->getTheme(), "system", mWindow));

		this->add(e);
	}
}

namespace {

// theme font of the carousel (fontPath, fontSize as a fraction of the screen height), else the
// default one at `defaultSize` (Recalbox FontManager::FromTheme); a missing file: default path
std::shared_ptr<Font> carouselFont(const es::carousel::Carousel& c, int defaultSize)
{
	if(!c.fontPath && !c.fontSize)
		return Font::get(defaultSize);
	const int size = c.fontSize ? (int)(*c.fontSize * (float)Renderer::getScreenHeight()) : defaultSize;
	std::string path = Font::getDefaultPath();
	if(c.fontPath && ResourceManager::getInstance()->fileExists(*c.fontPath))
		path = *c.fontPath;
	return Font::get(std::max(1, size), path);
}

Alignment textAlignment(es::carousel::HAlign h)
{
	switch(h)
	{
		case es::carousel::HAlign::Left: return ALIGN_LEFT;
		case es::carousel::HAlign::Right: return ALIGN_RIGHT;
		default: return ALIGN_CENTER;
	}
}

} // namespace

void SystemView::populateThemed(const std::shared_ptr<ThemeData>& mainTheme)
{
	// carousel element, values of Recalbox for what it does not give
	mCarousel = es::carousel::fromElement(
		mainTheme->getElement("system", "systemcarousel", "carousel"), glm::vec2(mSize.x, mSize.y));
	mCarouselFontLarge = carouselFont(mCarousel, FONT_SIZE_LARGE);
	mCarouselFontMedium = carouselFont(mCarousel, FONT_SIZE_MEDIUM);

	// information bar: Recalbox defaults (SystemView.cpp:1017-1026), then the systemInfo text
	mSystemInfo.setFont(Font::get(FONT_SIZE_SMALL));
	mSystemInfo.setSize(mSize.x, mSystemInfo.getFont()->getHeight(1.0f) * 2.2f);
	mSystemInfo.setPosition(0, mCarousel.pos.y + mCarousel.size.y - 0.2f);
	mSystemInfo.setAlignment(ALIGN_CENTER);
	mSystemInfo.setColor(0x000000FF);
	mSystemInfo.setDefaultZIndex(50);
	mInfoBackground = 0xDDDDDDD8;
	if(mainTheme->getElement("system", "systemInfo", "text"))
		mSystemInfo.applyTheme(mainTheme, "system", "systemInfo", ThemeFlags::ALL);
}

void SystemView::makeThemedLogo(SystemData* system, SystemViewData& data)
{
	// Recalbox AssignSystemLogo (SystemView.cpp:59-165), without its built-in logos and genres
	data.logo.reset();
	data.logoSelected.reset();
	data.logoText.reset();
	if(mCarousel.textOnly)
		return; // drawn by renderTextList

	const glm::vec2 full = mCarousel.logoSize * mCarousel.logoScale;
	const glm::vec2 origin = es::carousel::logoOrigin(mCarousel);
	const Alignment align = textAlignment(mCarousel.hAlign);
	const std::shared_ptr<ThemeData>& theme = system->getTheme();
	const ThemeData::ThemeElement* elem = theme->getElement("system", "logo", "image");
	const std::string path = elem && elem->has("path") ? elem->get<std::string>("path") : std::string();

	if(!path.empty() && ResourceManager::getInstance()->fileExists(path))
	{
		auto logo = std::make_shared<ImageComponent>(mWindow);
		logo->setMaxSize(full);
		logo->applyTheme(theme, "system", "logo", ThemeFlags::PATH | ThemeFlags::COLOR);
		logo->setOrigin(origin);
		data.logo = logo;

		// a "default" logo: the system name over it (SystemView.cpp:67, 98-105)
		const std::filesystem::path p(path);
		std::string stem = p.stem().string();
		std::ranges::transform(stem, stem.begin(), [](unsigned char ch) { return (char)std::tolower(ch); });
		std::string lowerPath = p.generic_string();
		std::ranges::transform(lowerPath, lowerPath.begin(), [](unsigned char ch) { return (char)std::tolower(ch); });
		if(stem == "default" || lowerPath.find("/default/") != std::string::npos)
		{
			auto text = std::make_shared<TextComponent>(
				mWindow, system->getName(), Font::get(FONT_SIZE_MEDIUM), mCarousel.fontColor ^ 0x80808000, align);
			text->setSize(full);
			data.logoText = text;
		}
	}
	else
	{
		// no logo: the full name in the carousel font, black (SystemView.cpp:125-131)
		auto text = std::make_shared<TextComponent>(mWindow, system->getFullName(), mCarouselFontLarge, 0x000000FF, align);
		text->setSize(full);
		data.logoText = text;
	}
}

bool SystemView::isVertical() const
{
	// the type chooses the keys, text list included (SystemView.cpp:260-286)
	return mThemed && mCarousel.type != es::carousel::Type::Horizontal;
}

void SystemView::goToSystem(SystemData* system, bool animate)
{
	setCursor(system);

	if(!animate)
		finishAnimation(0);
}

bool SystemView::input(InputConfig* config, Input input)
{
	if(input.value != 0)
	{
		if(config->getDeviceId() == DEVICE_KEYBOARD && input.value && input.id == SDLK_R &&
			SDL_GetModState() & SDL_KMOD_LCTRL && Settings::getInstance()->getBool("Debug"))
		{
			LOG(LogInfo) << " Reloading SystemList view";

			// reload themes
			for(auto it = mEntries.begin(); it != mEntries.end(); it++)
				it->object->loadTheme();

			populate();
			updateHelpPrompts();
			return true;
		}
		if(config->isMappedTo(isVertical() ? "up" : "left", input))
		{
			listInput(-1);
			return true;
		}
		if(config->isMappedTo(isVertical() ? "down" : "right", input))
		{
			listInput(1);
			return true;
		}
		if(config->isMappedTo("a", input))
		{
			stopScrolling();
			ViewController::get()->goToGameList(getSelected());
			return true;
		}
	}
	else
	{
		if(isVertical() ? (config->isMappedTo("up", input) || config->isMappedTo("down", input))
						: (config->isMappedTo("left", input) || config->isMappedTo("right", input)))
			listInput(0);
	}

	return GuiComponent::input(config, input);
}

void SystemView::update(int deltaTime)
{
	listUpdate(deltaTime);
	GuiComponent::update(deltaTime);
}

void SystemView::onCursorChanged(const CursorState& state)
{
	// update help style
	updateHelpPrompts();

	float startPos = mCamOffset;

	float posMax = (float)mEntries.size();
	float target = (float)mCursor;

	// what's the shortest way to get to our target?
	// it's one of these...

	float endPos = target; // directly
	float dist = abs(endPos - startPos);

	if(abs(target + posMax - startPos) < dist)
		endPos = target + posMax; // loop around the end (0 -> max)
	if(abs(target - posMax - startPos) < dist)
		endPos = target - posMax; // loop around the start (max - 1 -> -1)

	// animate mSystemInfo's opacity (fade out, wait, fade back in)

	cancelAnimation(1);
	cancelAnimation(2);

	const float infoStartOpacity = mSystemInfo.getOpacity() / 255.f;

	auto infoFadeOut = std::make_unique<LambdaAnimation>(
		[infoStartOpacity, this](
			float t) { mSystemInfo.setOpacity((unsigned char)(lerp<float>(infoStartOpacity, 0.f, t) * 255)); },
		(int)(infoStartOpacity * 150));

	unsigned int gameCount = getSelected()->getGameCount();

	// also change the text after we've fully faded out
	setAnimation(
		std::move(infoFadeOut), 0,
		[this, gameCount] {
			std::stringstream ss;

			// only display a game count if there are at least 2 games
			if(gameCount > 1)
				// TRANSLATORS: shown only from 2 games on
				ss << I18n::toUpperUtf8(std::vformat(
					_N("{} game available", "{} games available", gameCount), std::make_format_args(gameCount)));

			mSystemInfo.setText(ss.str());
		},
		false, 1);

	// only display a game count if there are at least 2 games
	if(gameCount > 1)
	{
		auto infoFadeIn = std::make_unique<LambdaAnimation>(
			[this](float t) { mSystemInfo.setOpacity((unsigned char)(lerp<float>(0.f, 1.f, t) * 255)); }, 300);

		// wait 600ms to fade in
		setAnimation(std::move(infoFadeIn), 2000, nullptr, false, 2);
	}

	// no need to animate transition, we're not going anywhere (probably mEntries.size() == 1)
	if(endPos == mCamOffset && endPos == mExtrasCamOffset)
		return;

	std::unique_ptr<Animation> anim;
	// the main theme's defaultTransition, else the setting (PR 11g)
	const std::string transition = es::carousel::transitionStyle(
		mWindow->getMainTheme().get(), Settings::getInstance()->getString("TransitionStyle"));
	if(transition == "instant")
	{
		// the logos still move, the extras jump (SystemView.cpp:504-516)
		anim = std::make_unique<LambdaAnimation>(
			[startPos, endPos, posMax, this](float t) {
				t -= 1;
				float f = lerp<float>(startPos, endPos, t * t * t + 1);
				if(f < 0)
					f += posMax;
				if(f >= posMax)
					f -= posMax;

				this->mCamOffset = f;
				this->mExtrasCamOffset = endPos;
			},
			500);
	}
	else if(transition == "fade")
	{
		float startExtrasFade = mExtrasFadeOpacity;
		anim = std::make_unique<LambdaAnimation>(
			[startExtrasFade, startPos, endPos, posMax, this](float t) {
				t -= 1;
				float f = lerp<float>(startPos, endPos, t * t * t + 1);
				if(f < 0)
					f += posMax;
				if(f >= posMax)
					f -= posMax;

				this->mCamOffset = f;

				t += 1;
				if(t < 0.3f)
					this->mExtrasFadeOpacity = lerp<float>(0.0f, 1.0f, t / 0.3f + startExtrasFade);
				else if(t < 0.7f)
					this->mExtrasFadeOpacity = 1.0f;
				else
					this->mExtrasFadeOpacity = lerp<float>(1.0f, 0.0f, (t - 0.7f) / 0.3f);

				if(t > 0.5f)
					this->mExtrasCamOffset = endPos;
			},
			500);
	}
	else
	{ // slide
		anim = std::make_unique<LambdaAnimation>(
			[startPos, endPos, posMax, this](float t) {
				t -= 1;
				float f = lerp<float>(startPos, endPos, t * t * t + 1);
				if(f < 0)
					f += posMax;
				if(f >= posMax)
					f -= posMax;

				this->mCamOffset = f;
				this->mExtrasCamOffset = f;
			},
			500);
	}

	setAnimation(std::move(anim), 0, nullptr, false, 0);
}

void SystemView::render(const glm::mat4& parentTrans)
{
	if(size() == 0)
		return;

	glm::mat4 trans = getTransform() * parentTrans;
	if(mThemed)
		renderThemed(trans);
	else
		renderLegacy(trans);
}

void SystemView::renderLegacy(const glm::mat4& trans)
{

	// draw the list elements (titles, backgrounds, logos)
	const float logoSizeX = logoSize().x + LOGO_PADDING;

	int logoCount = (int)(mSize.x / logoSizeX) + 2; // how many logos we need to draw
	int center = (int)(mCamOffset);

	if(mEntries.size() == 1)
		logoCount = 1;

	// draw background extras
	glm::mat4 extrasTrans = trans;
	int extrasCenter = (int)mExtrasCamOffset;
	for(int i = extrasCenter - 1; i < extrasCenter + 2; i++)
	{
		int index = i;
		while(index < 0)
			index += mEntries.size();
		while(index >= (int)mEntries.size())
			index -= mEntries.size();

		setTranslation(extrasTrans, getTranslation(trans) + glm::vec3((i - mExtrasCamOffset) * mSize.x, 0, 0));

		glm::ivec2 clipRect = glm::ivec2((int)((i - mExtrasCamOffset) * mSize.x), 0);
		Renderer::pushClipRect(clipRect, glm::ivec2(mSize));
		mEntries.at(index).data.backgroundExtras->render(extrasTrans);
		Renderer::popClipRect();
	}

	// fade extras if necessary
	if(mExtrasFadeOpacity)
	{
		Renderer::setMatrix(trans);
		Renderer::drawRect(0.0f, 0.0f, mSize.x, mSize.y, 0x00000000 | (unsigned char)(mExtrasFadeOpacity * 255));
	}

	// draw logos
	float xOff = (mSize.x - logoSize().x) / 2 - (mCamOffset * logoSizeX);
	float yOff = (mSize.y - logoSize().y) / 2;

	// background behind the logos
	Renderer::setMatrix(trans);
	Renderer::drawRect(0.f, (mSize.y - BAND_HEIGHT) / 2, mSize.x, BAND_HEIGHT, 0xFFFFFFD8);

	glm::mat4 logoTrans = trans;
	for(int i = center - logoCount / 2; i < center + logoCount / 2 + 1; i++)
	{
		int index = i;
		while(index < 0)
			index += mEntries.size();
		while(index >= (int)mEntries.size())
			index -= mEntries.size();

		setTranslation(logoTrans, getTranslation(trans) + glm::vec3(i * logoSizeX + xOff, yOff, 0));

		if(index == mCursor) //scale our selection up
		{
			// selected
			const std::shared_ptr<GuiComponent>& comp = mEntries.at(index).data.logoSelected;
			comp->setOpacity(0xFF);
			comp->render(logoTrans);
		}
		else
		{
			// not selected
			const std::shared_ptr<GuiComponent>& comp = mEntries.at(index).data.logo;
			comp->setOpacity(0x80);
			comp->render(logoTrans);
		}
	}

	Renderer::setMatrix(trans);
	Renderer::drawRect(mSystemInfo.getPosition().x, mSystemInfo.getPosition().y - 1, mSize.x, mSystemInfo.getSize().y,
		0xDDDDDD00 | (unsigned char)(mSystemInfo.getOpacity() / 255.f * 0xD8));
	mSystemInfo.render(trans);
}

void SystemView::renderThemed(const glm::mat4& trans)
{
	// extras below, between and above the carousel and the information bar, by zIndex
	// (Recalbox SystemView::Render, SystemView.cpp:527-545)
	constexpr float kLowest = std::numeric_limits<float>::lowest();
	constexpr float kHighest = std::numeric_limits<float>::max();
	const float infoZ = mSystemInfo.getZIndex();
	const float low = std::min(mCarousel.zIndex, infoZ);
	const float high = std::max(mCarousel.zIndex, infoZ);
	const bool carouselAbove = mCarousel.zIndex > infoZ;

	renderExtras(trans, kLowest, low);
	if(carouselAbove)
		renderInfoBar(trans);
	else
		renderCarousel(trans);
	renderExtras(trans, low, high);
	if(carouselAbove)
		renderCarousel(trans);
	else
		renderInfoBar(trans);
	renderExtras(trans, high, kHighest);

	if(mExtrasFadeOpacity)
	{
		Renderer::setMatrix(trans);
		Renderer::drawRect(0.0f, 0.0f, mSize.x, mSize.y, 0x00000000 | (unsigned char)(mExtrasFadeOpacity * 255));
	}
}

void SystemView::renderExtras(const glm::mat4& trans, float lower, float upper)
{
	// the current system and its neighbours, sliding along the carousel's direction
	const int center = (int)mExtrasCamOffset;
	const bool horizontal = mCarousel.type == es::carousel::Type::Horizontal;
	for(int i = center - 1; i <= center + 1; i++)
	{
		const int index = es::carousel::wrapIndex(i, (int)mEntries.size());
		const float shift = (float)i - mExtrasCamOffset;
		const glm::vec3 move = horizontal ? glm::vec3(shift * mSize.x, 0, 0) : glm::vec3(0, shift * mSize.y, 0);
		glm::mat4 extrasTrans = trans;
		setTranslation(extrasTrans, getTranslation(trans) + move);

		Renderer::pushClipRect(glm::ivec2(getTranslation(extrasTrans)), glm::ivec2(mSize));
		const ThemeExtras& extras = *mEntries.at(index).data.backgroundExtras;
		for(unsigned int c = 0; c < extras.getChildCount(); c++)
		{
			GuiComponent* extra = extras.getChild(c);
			if(!extra->isThemeDisabled() && extra->getZIndex() >= lower && extra->getZIndex() < upper)
				extra->render(extrasTrans);
		}
		Renderer::popClipRect();
	}
}

void SystemView::renderCarousel(const glm::mat4& trans)
{
	if(mCarousel.textOnly)
	{
		renderTextList(trans);
		return;
	}

	// background box behind the logos, logos clipped to it (SystemView.cpp:636-787)
	glm::mat4 carouselTrans = trans;
	setTranslation(carouselTrans,
		getTranslation(trans) + glm::vec3(mCarousel.pos - mCarousel.origin * mCarousel.size, 0.0f));
	Renderer::pushClipRect(glm::ivec2(getTranslation(carouselTrans)), glm::ivec2(mCarousel.size));
	Renderer::setMatrix(carouselTrans);
	Renderer::drawRect(0.0f, 0.0f, mCarousel.size.x, mCarousel.size.y, mCarousel.color);

	const glm::vec2 origin = es::carousel::logoOrigin(mCarousel);
	const glm::ivec2 range = es::carousel::visibleRange(mCarousel, mCamOffset, (int)mEntries.size());
	for(int i = range.x; i <= range.y; i++)
	{
		const SystemViewData& data = mEntries.at(es::carousel::wrapIndex(i, (int)mEntries.size())).data;
		const float distance = (float)i - mCamOffset;
		const float scale = es::carousel::logoScaleFactor(mCarousel, distance);
		const unsigned char opacity = es::carousel::logoOpacity(distance);
		const float rotation = es::carousel::logoRotationAt(mCarousel, distance);

		// the logo's origin point in its slot, scaled around it
		const glm::vec2 point = es::carousel::slotPosition(mCarousel, mCamOffset, i) + mCarousel.logoSize * origin;
		glm::mat4 logoTrans = glm::translate(carouselTrans, glm::vec3(point, 0.0f));
		logoTrans = glm::scale(logoTrans, glm::vec3(scale, scale, 1.0f));

		for(const std::shared_ptr<GuiComponent>& comp : {data.logo, data.logoText})
		{
			if(!comp)
				continue;
			comp->setRotation(rotation);
			comp->setRotationOrigin(mCarousel.logoRotationOrigin);
			comp->setOpacity(opacity);
			// a text has no origin: its top left corner goes origin x size back
			const glm::vec2 back = comp == data.logoText ? origin * comp->getSize() : glm::vec2(0.0f);
			comp->render(glm::translate(logoTrans, glm::vec3(-back, 0.0f)));
		}
	}
	Renderer::popClipRect();
}

void SystemView::renderTextList(const glm::mat4& trans)
{
	// system names as a list (SystemView.cpp:789-885), without the scrolling of a long selected
	// name nor the scroll bar
	glm::mat4 listTrans = trans;
	setTranslation(listTrans, getTranslation(trans) + glm::vec3(mCarousel.pos - mCarousel.origin * mCarousel.size, 0.0f));
	Renderer::setMatrix(listTrans);
	Renderer::drawRect(0.0f, 0.0f, mCarousel.size.x, mCarousel.size.y, mCarousel.color);

	const int count = (int)mEntries.size();
	const es::carousel::TextListLayout l =
		es::carousel::textListLayout(mCarousel, mCarouselFontLarge->getHeight(1.0f), mCursor, count);
	if(l.rowHeight <= 0.0f)
		return;

	Renderer::pushClipRect(glm::ivec2(getTranslation(listTrans)), glm::ivec2(mCarousel.size));
	Renderer::setMatrix(listTrans);
	for(int i = l.firstRow; i < l.firstRow + l.rowCount; i++)
	{
		const int index = es::carousel::wrapIndex(i, count);
		if(index == mCursor)
			continue;
		Renderer::drawRect(
			0.0f, ((float)i - l.startOffset) * l.rowHeight, mCarousel.size.x, l.rowHeight, es::carousel::textListRowTint(index));
	}
	Renderer::drawRect(mCarousel.selectorOffsetX, l.selectorY, mCarousel.size.x, l.selectorHeight, mCarousel.selectorColor);

	const float width = std::max(0.0f, mCarousel.size.x - mCarousel.horizontalMargin * 2.0f);
	const Alignment align = textAlignment(mCarousel.hAlign);
	for(int i = l.firstRow; i < l.firstRow + l.rowCount; i++)
	{
		const int index = es::carousel::wrapIndex(i, count);
		std::string name = mEntries.at(index).object->getFullName();
		if(mCarousel.forceUppercase)
			name = I18n::toUpperUtf8(name);
		const float y = ((float)i - l.startOffset) * l.rowHeight + (l.rowHeight - mCarouselFontLarge->getHeight(1.0f)) / 2;
		auto cache = mCarouselFontLarge->buildTextCache(name,
			glm::vec2(mCarousel.horizontalMargin + mCarousel.textOffsetX, y),
			es::carousel::textListColor(mCarousel, index, mCursor), width, align);
		Renderer::setMatrix(listTrans);
		mCarouselFontLarge->renderTextCache(cache.get());
	}
	Renderer::popClipRect();
}

void SystemView::renderInfoBar(const glm::mat4& trans)
{
	Renderer::setMatrix(trans);
	Renderer::drawRect(mSystemInfo.getPosition().x, mSystemInfo.getPosition().y, mSystemInfo.getSize().x,
		mSystemInfo.getSize().y, (mInfoBackground & 0xFFFFFF00) |
			(unsigned char)((mInfoBackground & 0xFF) * (mSystemInfo.getOpacity() / 255.f)));
	mSystemInfo.render(trans);
}

std::vector<HelpPrompt> SystemView::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.emplace_back(isVertical() ? "up/down" : "left/right", _("choose"));
	prompts.emplace_back("a", _("select"));
	return prompts;
}

HelpStyle SystemView::getHelpStyle()
{
	HelpStyle style;
	style.applyTheme(mEntries.at(mCursor).object->getTheme(), "system");
	return style;
}
