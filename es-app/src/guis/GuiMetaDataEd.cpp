#include "Window.h"
#include "components/MenuComponent.h"
#include "guis/GuiMetaDataEd.h"
#include "utils/I18n.h"
#include <cassert>
#include "renderer/Renderer.h"
#include "Log.h"
#include "components/AsyncReqComponent.h"
#include "Settings.h"
#include "views/ViewController.h"
#include "guis/GuiGameScraper.h"
#include "guis/GuiMsgBox.h"
#include <filesystem>

#include "components/TextEditComponent.h"
#include "components/DateTimeComponent.h"
#include "components/RatingComponent.h"
#include "guis/GuiTextEditPopup.h"

GuiMetaDataEd::GuiMetaDataEd(Window* window, MetaDataList* md, const std::vector<MetaDataDecl>& mdd,
	ScraperSearchParams scraperParams, const std::string& header, std::function<void()> saveCallback,
	std::function<void()> deleteFunc) :
	GuiComponent(window),
	mBackground(window, ":/frame.png"),

	mGrid(window, glm::ivec2(1, 3)),
	mScraperParams(scraperParams),

	mMetaDataDecl(mdd),
	mMetaData(md),
	mSavedCallback(saveCallback),
	mDeleteFunc(deleteFunc)
{
	applyMenuFrame(mBackground, mWindow->getMenuTheme()); // theme frame (11f2)
	addChild(&mBackground);
	addChild(&mGrid);

	mHeaderGrid = std::make_shared<ComponentGrid>(mWindow, glm::ivec2(1, 5));

	mTitle =
		std::make_shared<TextComponent>(mWindow, _U("Edit metadata"), es::theme::menuFontOr(mWindow->getMenuTheme().title.font, Font::get(FONT_SIZE_LARGE)),
		mWindow->getMenuTheme().title.color.value_or(0x555555FF), ALIGN_CENTER);
	mSubtitle =
		std::make_shared<TextComponent>(mWindow, strToUpper(scraperParams.game->getPath().filename().generic_string()),
			es::theme::menuFontOr(mWindow->getMenuTheme().textSmall.font, Font::get(FONT_SIZE_SMALL)),
		mWindow->getMenuTheme().textSmall.color.value_or(0x777777FF), ALIGN_CENTER);
	mHeaderGrid->setEntry(mTitle, glm::ivec2(0, 1), false, true);
	mHeaderGrid->setEntry(mSubtitle, glm::ivec2(0, 3), false, true);

	mGrid.setEntry(mHeaderGrid, glm::ivec2(0, 0), false, true);

	mList = std::make_shared<ComponentList>(mWindow);
	mGrid.setEntry(mList, glm::ivec2(0, 1), true, true);

	// populate list
	for(auto iter = mdd.begin(); iter != mdd.end(); iter++)
	{
		std::shared_ptr<GuiComponent> ed;

		// don't add statistics
		if(iter->isStatistic)
			continue;

		// create ed and add it (and any related components) to mMenu
		// ed's value will be set below
		ComponentListRow row;
		auto lbl = std::make_shared<TextComponent>(
			mWindow, _U(iter->displayName), es::theme::menuFontOr(mWindow->getMenuTheme().textSmall.font, Font::get(FONT_SIZE_SMALL)),
		mWindow->getMenuTheme().textSmall.color.value_or(0x777777FF));
		row.addElement(lbl, true); // label

		switch(iter->type)
		{
		case MD_RATING: {
			ed = std::make_shared<RatingComponent>(window);
			const float height = lbl->getSize().y * 0.71f;
			ed->setSize(0, height);
			row.addElement(ed, false, true);

			auto spacer = std::make_shared<GuiComponent>(mWindow);
			spacer->setSize(Renderer::getScreenWidth() * 0.0025f, 0);
			row.addElement(spacer, false);

			// pass input to the actual RatingComponent instead of the spacer
			row.input_handler = std::bind(&GuiComponent::input, ed.get(), std::placeholders::_1, std::placeholders::_2);

			break;
		}
		case MD_DATE: {
			ed = std::make_shared<DateTimeComponent>(window);
			row.addElement(ed, false);

			auto spacer = std::make_shared<GuiComponent>(mWindow);
			spacer->setSize(Renderer::getScreenWidth() * 0.0025f, 0);
			row.addElement(spacer, false);

			// pass input to the actual DateTimeComponent instead of the spacer
			row.input_handler = std::bind(&GuiComponent::input, ed.get(), std::placeholders::_1, std::placeholders::_2);

			break;
		}
		case MD_TIME: {
			ed = std::make_shared<DateTimeComponent>(window, DateTimeComponent::DISP_RELATIVE_TO_NOW);
			row.addElement(ed, false);
			break;
		}
		case MD_MULTILINE_STRING:
		default: {
			// MD_STRING
			ed = std::make_shared<TextComponent>(
				window, "", es::theme::menuFontOr(mWindow->getMenuTheme().textSmall.font, Font::get(FONT_SIZE_SMALL, FONT_PATH_LIGHT)),
		mWindow->getMenuTheme().textSmall.color.value_or(0x777777FF), ALIGN_RIGHT);
			row.addElement(ed, true);

			auto spacer = std::make_shared<GuiComponent>(mWindow);
			spacer->setSize(Renderer::getScreenWidth() * 0.005f, 0);
			row.addElement(spacer, false);

			auto bracket = std::make_shared<ImageComponent>(mWindow);
			bracket->setImage(":/arrow.svg");
			bracket->setResize(glm::vec2(0, lbl->getFont()->getLetterHeight()));
			row.addElement(bracket, false);

			bool multiLine = iter->type == MD_MULTILINE_STRING;
			const std::string title = _(iter->displayPrompt);
			auto updateVal = [ed](const std::string& newVal) {
				ed->setValue(newVal);
			}; // ok callback (apply new value to ed)
			row.makeAcceptInputHandler([this, title, ed, updateVal, multiLine] {
				mWindow->pushGui(std::make_unique<GuiTextEditPopup>(mWindow, title, ed->getValue(), updateVal, multiLine));
			});
			break;
		}
		}

		assert(ed);
		mList->addRow(row);
		ed->setValue(mMetaData->get(iter->key));
		mEditors.push_back(ed);
	}

	std::vector<std::shared_ptr<ButtonComponent>> buttons;

	if(!scraperParams.system->hasPlatformId(PlatformIds::PLATFORM_IGNORE))
		buttons.push_back(
			std::make_shared<ButtonComponent>(mWindow, _U("Scrape"), _("scrape"), std::bind(&GuiMetaDataEd::fetch, this)));

	buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _U("Save"), _("save"), [&] {
		save();
		mWindow->closeGui(this);
	}));
	buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _U("Cancel"), _("cancel"), [&] { mWindow->closeGui(this); }));

	if(mDeleteFunc)
	{
		auto deleteFileAndSelf = [&] {
			mDeleteFunc();
			mWindow->closeGui(this);
		};
		auto deleteBtnFunc = [this, deleteFileAndSelf] {
			mWindow->pushGui(std::make_unique<GuiMsgBox>(
				mWindow, _U("This will delete a file!\nAre you sure?"), _U("Yes"), deleteFileAndSelf, _U("No"), nullptr));
		};
		buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _U("Delete"), _("delete"), deleteBtnFunc));
	}

	mButtons = makeButtonGrid(mWindow, buttons);
	mGrid.setEntry(mButtons, glm::ivec2(0, 2), true, false);

	// resize + center
	setSize(Renderer::getScreenWidth() * 0.5f, Renderer::getScreenHeight() * 0.82f);
	setPosition((Renderer::getScreenWidth() - mSize.x) / 2, (Renderer::getScreenHeight() - mSize.y) / 2);
}

void GuiMetaDataEd::onSizeChanged()
{
	mBackground.fitTo(mSize, glm::vec3(0.0f), glm::vec2(-32, -32));

	mGrid.setSize(mSize);

	const float titleHeight = mTitle->getFont()->getLetterHeight();
	const float subtitleHeight = mSubtitle->getFont()->getLetterHeight();
	const float titleSubtitleSpacing = mSize.y * 0.03f;

	mGrid.setRowHeightPerc(0, (titleHeight + titleSubtitleSpacing + subtitleHeight + TITLE_VERT_PADDING) / mSize.y);
	mGrid.setRowHeightPerc(2, mButtons->getSize().y / mSize.y);

	mHeaderGrid->setRowHeightPerc(1, titleHeight / mHeaderGrid->getSize().y);
	mHeaderGrid->setRowHeightPerc(2, titleSubtitleSpacing / mHeaderGrid->getSize().y);
	mHeaderGrid->setRowHeightPerc(3, subtitleHeight / mHeaderGrid->getSize().y);
}

void GuiMetaDataEd::save()
{
	for(unsigned int i = 0; i < mEditors.size(); i++)
	{
		if(mMetaDataDecl.at(i).isStatistic)
			continue;

		mMetaData->set(mMetaDataDecl.at(i).key, mEditors.at(i)->getValue());
	}

	if(mSavedCallback)
		mSavedCallback();
}

void GuiMetaDataEd::fetch()
{
	auto scr = std::make_unique<GuiGameScraper>(mWindow, mScraperParams, std::bind(&GuiMetaDataEd::fetchDone, this, std::placeholders::_1));
	mWindow->pushGui(std::move(scr));
}

void GuiMetaDataEd::fetchDone(const ScraperSearchResult& result)
{
	for(unsigned int i = 0; i < mEditors.size(); i++)
	{
		if(mMetaDataDecl.at(i).isStatistic)
			continue;

		const std::string& key = mMetaDataDecl.at(i).key;
		mEditors.at(i)->setValue(result.mdl.get(key));
	}
}

void GuiMetaDataEd::close(bool closeAllWindows)
{
	// find out if the user made any changes
	bool dirty = false;
	for(unsigned int i = 0; i < mEditors.size(); i++)
	{
		const std::string& key = mMetaDataDecl.at(i).key;
		if(mMetaData->get(key) != mEditors.at(i)->getValue())
		{
			dirty = true;
			break;
		}
	}

	std::function<void()> closeFunc;
	if(!closeAllWindows)
	{
		closeFunc = [this] {
			mWindow->closeGui(this);
		};
	}
	else
	{
		Window* window = mWindow;
		closeFunc = [window] {
			while(window->peekGui() != ViewController::get())
				delete window->peekGui();
		};
	}

	if(dirty)
	{
		// changes were made, ask if the user wants to save them
		mWindow->pushGui(std::make_unique<GuiMsgBox>(
			mWindow, _U("Save changes?"), _U("Yes"),
			[this, closeFunc] {
				save();
				closeFunc();
			},
			_U("No"), closeFunc));
	}
	else
	{
		closeFunc();
	}
}

bool GuiMetaDataEd::input(InputConfig* config, Input input)
{
	if(GuiComponent::input(config, input))
		return true;

	const bool isStart = config->isMappedTo("start", input);
	if(input.value != 0 && (config->isMappedTo("b", input) || isStart))
	{
		close(isStart);
		return true;
	}

	return false;
}

std::vector<HelpPrompt> GuiMetaDataEd::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mGrid.getHelpPrompts();
	prompts.emplace_back("b", _("back"));
	prompts.emplace_back("start", _("close"));
	return prompts;
}
