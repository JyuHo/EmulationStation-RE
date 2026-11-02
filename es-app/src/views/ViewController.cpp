#include "views/ViewController.h"
#include "views/SystemCarousel.h"
#include "utils/I18n.h"
#include <cassert>
#include "Log.h"
#include "SystemData.h"
#include "Settings.h"

#include "views/gamelist/BasicGameListView.h"
#include "views/gamelist/DetailedGameListView.h"
#include "views/gamelist/GridGameListView.h"
#include "guis/GuiMenu.h"
#include "guis/GuiMsgBox.h"
#include "animations/LaunchAnimation.h"
#include "animations/MoveCameraAnimation.h"
#include "animations/LambdaAnimation.h"

ViewController* ViewController::sInstance = nullptr;

ViewController* ViewController::get()
{
	assert(sInstance);
	return sInstance;
}

void ViewController::init(Window* window)
{
	assert(!sInstance);
	// the window owns the view controller; sInstance is only a shortcut to it
	std::unique_ptr<ViewController> controller(new ViewController(window)); // private constructor
	sInstance = controller.get();
	window->pushGui(std::move(controller));
}

ViewController::ViewController(Window* window) :
	GuiComponent(window), mCurrentView(nullptr), mCamera(glm::mat4(1.0f)), mFadeOpacity(0), mLockInput(false)
{
	mState.viewing = NOTHING;
}

ViewController::~ViewController()
{
	assert(sInstance == this);
	sInstance = nullptr;
}

void ViewController::goToStart()
{
	// TODO
	/* mState.viewing = START_SCREEN;
	mCurrentView.reset();
	playViewTransition(); */
	goToSystemView(SystemData::sSystemVector.at(0));
}

int ViewController::getSystemId(SystemData* system)
{
	std::vector<SystemData*>& sysVec = SystemData::sSystemVector;
	return std::find(sysVec.begin(), sysVec.end(), system) - sysVec.begin();
}

void ViewController::goToSystemView(SystemData* system)
{
	mState.viewing = SYSTEM_SELECT;
	mState.system = system;

	auto systemList = getSystemListView();
	systemList->setPosition(getSystemId(system) * (float)Renderer::getScreenWidth(), systemList->getPosition().y);

	systemList->goToSystem(system, false);
	setCurrentView(systemList);

	playViewTransition();
}

void ViewController::goToNextGameList()
{
	assert(mState.viewing == GAME_LIST);
	SystemData* system = getState().getSystem();
	assert(system);
	goToGameList(system->getNext());
}

void ViewController::goToPrevGameList()
{
	assert(mState.viewing == GAME_LIST);
	SystemData* system = getState().getSystem();
	assert(system);
	goToGameList(system->getPrev());
}

void ViewController::goToGameList(SystemData* system)
{
	if(mState.viewing == SYSTEM_SELECT)
	{
		// move system list
		auto sysList = getSystemListView();
		float offX = sysList->getPosition().x;
		int sysId = getSystemId(system);
		sysList->setPosition(sysId * (float)Renderer::getScreenWidth(), sysList->getPosition().y);
		offX = sysList->getPosition().x - offX;
		mCamera[3].x -= offX;
	}

	mState.viewing = GAME_LIST;
	mState.system = system;

	setCurrentView(getGameListView(system));
	playViewTransition();
}

void ViewController::setCurrentView(std::shared_ptr<GuiComponent> view)
{
	if(view == mCurrentView)
		return;
	if(mCurrentView)
		mCurrentView->onHide();
	mCurrentView = std::move(view);
	if(mCurrentView)
		mCurrentView->onShow();
}

void ViewController::onShow()
{
	if(mCurrentView)
		mCurrentView->onShow();
}

void ViewController::playViewTransition()
{
	glm::vec3 target(1.0f, 0.0f, 0.0f); // unit x vector, as in the original code
	if(mCurrentView)
		target = mCurrentView->getPosition();

	// no need to animate, we're not going anywhere (probably goToNextGamelist() or goToPrevGamelist() when there's only 1 system)
	if(target == -getTranslation(mCamera) && !isAnimationPlaying(0))
		return;

	// the main theme's defaultTransition, else the setting (docs/THEMES.md, carousel, PR 11g)
	const std::string transition = es::carousel::transitionStyle(
		mWindow->getMainTheme().get(), Settings::getInstance()->getString("TransitionStyle"));
	if(transition == "instant")
	{
		cancelAnimation(0);
		setTranslation(mCamera, -target);
		updateHelpPrompts();
	}
	else if(transition == "fade")
	{
		// fade
		// stop whatever's currently playing, leaving mFadeOpacity wherever it is
		cancelAnimation(0);

		auto fadeFunc = [this](float t) {
			mFadeOpacity = lerp<float>(0, 1, t);
		};

		const static int FADE_DURATION = 240; // fade in/out time
		const static int FADE_WAIT = 320;	  // time to wait between in/out
		setAnimation(std::make_unique<LambdaAnimation>(fadeFunc, FADE_DURATION), 0, [this, fadeFunc, target] {
			setTranslation(this->mCamera, -target);
			updateHelpPrompts();
			setAnimation(std::make_unique<LambdaAnimation>(fadeFunc, FADE_DURATION), FADE_WAIT, nullptr, true);
		});

		// fast-forward animation if we're partway faded
		if(target == -getTranslation(mCamera))
		{
			// not changing screens, so cancel the first half entirely
			advanceAnimation(0, FADE_DURATION);
			advanceAnimation(0, FADE_WAIT);
			advanceAnimation(0, FADE_DURATION - (int)(mFadeOpacity * FADE_DURATION));
		}
		else
		{
			advanceAnimation(0, (int)(mFadeOpacity * FADE_DURATION));
		}
	}
	else
	{
		// slide
		setAnimation(std::make_unique<MoveCameraAnimation>(mCamera, target));
		updateHelpPrompts(); // update help prompts immediately
	}
}

void ViewController::onFileChanged(FileData* file, FileChangeType change)
{
	auto it = mGameListViews.find(file->getSystem());
	if(it != mGameListViews.end())
		it->second->onFileChanged(file, change);
}

void ViewController::launch(FileData* game, glm::vec3 center)
{
	if(game->getType() != GAME)
	{
		LOG(LogError) << "tried to launch something that isn't a game";
		return;
	}

	glm::mat4 origCamera = mCamera;
	setTranslation(origCamera, -mCurrentView->getPosition());

	center += mCurrentView->getPosition();
	stopAnimation(1); // make sure the fade in isn't still playing
	mLockInput = true;
	mCurrentView->onHide(); // stops the videos: the game gets the audio device

	if(Settings::getInstance()->getString("TransitionStyle") == "fade")
	{
		// fade out, launch game, fade back in
		auto fadeFunc = [this](float t) {
			//t -= 1;
			//mFadeOpacity = lerp<float>(0.0f, 1.0f, t*t*t + 1);
			mFadeOpacity = lerp<float>(0.0f, 1.0f, t);
		};
		setAnimation(std::make_unique<LambdaAnimation>(fadeFunc, 800), 0, [this, game, fadeFunc] {
			game->getSystem()->launchGame(mWindow, game);
			mCurrentView->onShow();
			mLockInput = false;
			setAnimation(std::make_unique<LambdaAnimation>(fadeFunc, 800), 0, nullptr, true);
			this->onFileChanged(game, FILE_METADATA_CHANGED);
		});
	}
	else
	{
		// move camera to zoom in on center + fade out, launch game, come back in
		setAnimation(std::make_unique<LaunchAnimation>(mCamera, mFadeOpacity, center, 1500), 0, [this, origCamera, center, game] {
			game->getSystem()->launchGame(mWindow, game);
			mCurrentView->onShow();
			mCamera = origCamera;
			mLockInput = false;
			setAnimation(std::make_unique<LaunchAnimation>(mCamera, mFadeOpacity, center, 600), 0, nullptr, true);
			this->onFileChanged(game, FILE_METADATA_CHANGED);
		});
	}
}

std::shared_ptr<IGameListView> ViewController::getGameListView(SystemData* system)
{
	//if we already made one, return that one
	auto exists = mGameListViews.find(system);
	if(exists != mGameListViews.end())
		return exists->second;

	//if we didn't, make it, remember it, and return it
	std::shared_ptr<IGameListView> view;

	//decide type
	bool detailed = false;
	std::vector<FileData*> files = system->getRootFolder()->getFilesRecursive(GAME | FOLDER);
	for(auto it = files.begin(); it != files.end(); it++)
	{
		if(!(*it)->getThumbnailPath().empty() || !(*it)->metadata.get("video").empty())
		{
			detailed = true;
			break;
		}
	}

	if(detailed)
		view = std::make_shared<DetailedGameListView>(mWindow, system->getRootFolder());
	else
		view = std::make_shared<BasicGameListView>(mWindow, system->getRootFolder());

	// uncomment for experimental "image grid" view
	//view = std::shared_ptr<IGameListView>(new GridGameListView(mWindow, system->getRootFolder()));

	view->setTheme(system->getTheme());

	std::vector<SystemData*>& sysVec = SystemData::sSystemVector;
	int id = std::find(sysVec.begin(), sysVec.end(), system) - sysVec.begin();
	view->setPosition(id * (float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight() * 2);

	addChild(view.get());

	mGameListViews[system] = view;
	return view;
}

std::shared_ptr<SystemView> ViewController::getSystemListView()
{
	//if we already made one, return that one
	if(mSystemListView)
		return mSystemListView;

	mSystemListView = std::make_shared<SystemView>(mWindow);
	addChild(mSystemListView.get());
	mSystemListView->setPosition(0, (float)Renderer::getScreenHeight());
	return mSystemListView;
}

bool ViewController::input(InputConfig* config, Input input)
{
	if(mLockInput)
		return true;

	// open menu
	if(config->isMappedTo("start", input) && input.value != 0)
	{
		// open menu
		mWindow->pushGui(std::make_unique<GuiMenu>(mWindow));
		return true;
	}

	if(mCurrentView)
		return mCurrentView->input(config, input);

	return false;
}

void ViewController::update(int deltaTime)
{
	if(mCurrentView)
	{
		mCurrentView->update(deltaTime);
	}

	updateSelf(deltaTime);
}

void ViewController::render(const glm::mat4& parentTrans)
{
	glm::mat4 trans = mCamera * parentTrans;

	// camera position, position + size
	const glm::mat4 inverseTrans = glm::inverse(trans);
	glm::vec3 viewStart = getTranslation(inverseTrans);
	glm::vec3 viewEnd = transformPoint(
		inverseTrans, glm::vec3((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight(), 0));

	// draw systemview
	getSystemListView()->render(trans);

	// draw gamelists
	for(auto it = mGameListViews.begin(); it != mGameListViews.end(); it++)
	{
		// clipping
		glm::vec3 guiStart = it->second->getPosition();
		glm::vec3 guiEnd = it->second->getPosition() + glm::vec3(it->second->getSize().x, it->second->getSize().y, 0);

		if(guiEnd.x >= viewStart.x && guiEnd.y >= viewStart.y && guiStart.x <= viewEnd.x && guiStart.y <= viewEnd.y)
			it->second->render(trans);
	}

	if(mWindow->peekGui() == this)
		mWindow->renderHelpPromptsEarly();

	// fade out
	if(mFadeOpacity)
	{
		Renderer::setMatrix(parentTrans);
		Renderer::drawRect(0, 0, Renderer::getScreenWidth(), Renderer::getScreenHeight(),
			0x00000000 | (unsigned char)(mFadeOpacity * 255));
	}
}

void ViewController::preload()
{
	for(auto it = SystemData::sSystemVector.begin(); it != SystemData::sSystemVector.end(); it++)
	{
		getGameListView(*it);
	}
}

void ViewController::reloadGameListView(IGameListView* view, bool reloadTheme)
{
	for(auto it = mGameListViews.begin(); it != mGameListViews.end(); it++)
	{
		if(it->second.get() == view)
		{
			bool isCurrent = (mCurrentView == it->second);
			SystemData* system = it->first;
			FileData* cursor = view->getCursor();
			mGameListViews.erase(it);

			if(reloadTheme)
				system->loadTheme();

			std::shared_ptr<IGameListView> newView = getGameListView(system);
			newView->setCursor(cursor);

			if(isCurrent)
				setCurrentView(newView);

			break;
		}
	}
}

void ViewController::reloadAll()
{
	mWindow->setMainTheme(ThemeData::loadMainTheme());

	std::map<SystemData*, FileData*> cursorMap;
	for(auto it = mGameListViews.begin(); it != mGameListViews.end(); it++)
	{
		cursorMap[it->first] = it->second->getCursor();
	}
	mGameListViews.clear();

	for(auto it = cursorMap.begin(); it != cursorMap.end(); it++)
	{
		it->first->loadTheme();
		getGameListView(it->first)->setCursor(it->second);
	}

	mSystemListView.reset();
	getSystemListView();

	// update mCurrentView since the pointers changed
	if(mState.viewing == GAME_LIST)
	{
		setCurrentView(getGameListView(mState.getSystem()));
	}
	else if(mState.viewing == SYSTEM_SELECT)
	{
		mSystemListView->goToSystem(mState.getSystem(), false);
		setCurrentView(mSystemListView);
	}
	else
	{
		goToSystemView(SystemData::sSystemVector.front());
	}

	updateHelpPrompts();
}

std::vector<HelpPrompt> ViewController::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	if(!mCurrentView)
		return prompts;

	prompts = mCurrentView->getHelpPrompts();
	prompts.emplace_back("start", _("menu"));

	return prompts;
}

HelpStyle ViewController::getHelpStyle()
{
	if(!mCurrentView)
		return GuiComponent::getHelpStyle();

	return mCurrentView->getHelpStyle();
}

std::string ViewController::keyboardThemeView(ViewMode viewing, const std::string& gameListViewName)
{
	switch(viewing)
	{
		case SYSTEM_SELECT: return "system";
		case GAME_LIST: return gameListViewName.empty() ? std::string("menu") : gameListViewName;
		default: return "menu";
	}
}

std::string ViewController::keyboardThemeView()
{
	std::string gameListView;
	if(mState.viewing == GAME_LIST && mState.system)
		gameListView = getGameListView(mState.system)->getName();
	return keyboardThemeView(mState.viewing, gameListView);
}
