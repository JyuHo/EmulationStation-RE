#pragma once
#ifndef ES_APP_VIEWS_GAME_LIST_BASIC_GAME_LIST_VIEW_H
#define ES_APP_VIEWS_GAME_LIST_BASIC_GAME_LIST_VIEW_H

#include "components/TextListComponent.h"
#include "views/gamelist/ISimpleGameListView.h"

class BasicGameListView : public ISimpleGameListView, public ILongMouseClickEvent
{
public:
	BasicGameListView(Window* window, FolderData* root);

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

		return "basic";
	}

	void launch(FileData* game) override;
	std::vector<FileData*> getFileDataEntries() override;

	void onLongMouseClick(GuiComponent* component) override;

protected:
	std::string getQuickSystemSelectRightButton() override;
	std::string getQuickSystemSelectLeftButton() override;
	void populateList(const std::vector<FileData*>& files) override;
	void remove(FileData* game) override;
	virtual void addPlaceholder();

	TextListComponent<FileData*> mList;
};

#endif // ES_APP_VIEWS_GAME_LIST_BASIC_GAME_LIST_VIEW_H
