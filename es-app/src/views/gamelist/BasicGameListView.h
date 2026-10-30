#pragma once

#include "views/gamelist/ISimpleGameListView.h"
#include "components/TextListComponent.h"

class BasicGameListView : public ISimpleGameListView
{
public:
	BasicGameListView(Window* window, FileData* root);

	// Called when a FileData* is added, has its metadata changed, or is removed
	void onFileChanged(FileData* file, FileChangeType change) override;

	void onThemeChanged(const std::shared_ptr<ThemeData>& theme) override;

	FileData* getCursor() override;
	void setCursor(FileData* file) override;

	const char* getName() const override { return "basic"; }

	std::vector<HelpPrompt> getHelpPrompts() override;

protected:
	void populateList(const std::vector<FileData*>& files) override;
	void launch(FileData* game) override;

	TextListComponent<FileData*> mList;
};
