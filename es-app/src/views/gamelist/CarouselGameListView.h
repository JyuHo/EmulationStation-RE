#pragma once
#ifndef ES_APP_VIEWS_GAME_LIST_CARR_GAME_LIST_VIEW_H
#define ES_APP_VIEWS_GAME_LIST_CARR_GAME_LIST_VIEW_H

#include "views/gamelist/ISimpleGameListView.h"
#include "components/CarouselComponent.h"
#include "DetailedContainer.h"

class CarouselGameListView : public ISimpleGameListView
{
public:
	CarouselGameListView(Window* window, FolderData* root);

	// Called when a FileData* is added, has its metadata changed, or is removed
	void onFileChanged(FileData* file, FileChangeType change) override;

	void onThemeChanged(const std::shared_ptr<ThemeData>& theme) override;

	FileData* getCursor() override;
	void setCursor(FileData* file) override;
	int getCursorIndex() override; 
	void setCursorIndex(int index) override; 
	void resetLastCursor() override;
	bool onMouseWheel(int delta) override;
	void onShow() override;

	const char* getName() const override
	{
		if (!mCustomThemeName.empty())
			return mCustomThemeName.c_str();

		return "gamecarousel";
	}

	void launch(FileData* game) override;
	Vector3f getLaunchTarget(Vector2f* size) override;
	std::vector<FileData*> getFileDataEntries() override;
	void update(int deltaTime) override;

protected:
	std::string getQuickSystemSelectRightButton() override;
	std::string getQuickSystemSelectLeftButton() override;
	void populateList(const std::vector<FileData*>& files) override;
	void remove(FileData* game) override;
	virtual void addPlaceholder();

	void updateInfoPanel();

	CarouselComponent mList;
	DetailedContainerHost mDetails;
};

#endif // ES_APP_VIEWS_GAME_LIST_BASIC_GAME_LIST_VIEW_H
