#pragma once
#ifndef ES_APP_VIEWS_GAME_LIST_GRID_GAME_LIST_VIEW_H
#define ES_APP_VIEWS_GAME_LIST_GRID_GAME_LIST_VIEW_H

#include "components/DateTimeComponent.h"
#include "components/RatingComponent.h"
#include "components/ImageGridComponent.h"
#include "views/gamelist/ISimpleGameListView.h"
#include "views/gamelist/BasicGameListView.h"
#include "DetailedContainer.h"

class VideoComponent;

class GridGameListView : public ISimpleGameListView, public ILongMouseClickEvent
{
public:
	GridGameListView(Window* window, FolderData* root, const std::shared_ptr<ThemeData>& theme, std::string customThemeName = "", Vector2f gridSize = Vector2f(0,0));

	void onThemeChanged(const std::shared_ptr<ThemeData>& theme) override;

	FileData* getCursor() override;
	void setCursor(FileData*) override;
	int getCursorIndex() override; 
	void setCursorIndex(int index) override; 
	void resetLastCursor() override;
	void moveToRandomGame() override;
	bool onMouseWheel(int delta) override;

	bool input(InputConfig* config, Input input) override;

	const char* getName() const override
	{
		if (!mCustomThemeName.empty())
			return mCustomThemeName.c_str();

		return "grid";
	}

	void launch(FileData* game) override;
	Vector3f getLaunchTarget(Vector2f* size) override;
	void onFileChanged(FileData* file, FileChangeType change) override;

	void setThemeName(std::string name) override;
	void onShow() override;
	std::vector<FileData*> getFileDataEntries() override;
	void update(int deltaTime) override;

	void onLongMouseClick(GuiComponent* component) override;

protected:
	std::string getQuickSystemSelectRightButton() override;
	std::string getQuickSystemSelectLeftButton() override;
	void populateList(const std::vector<FileData*>& files) override;
	void remove(FileData* game) override;
	virtual void addPlaceholder();

	ImageGridComponent<FileData*> mGrid;

private:
	DetailedContainerHost mDetails;

	void updateInfoPanel();
	const std::string getImagePath(FileData* file);
};

#endif // ES_APP_VIEWS_GAME_LIST_GRID_GAME_LIST_VIEW_H
