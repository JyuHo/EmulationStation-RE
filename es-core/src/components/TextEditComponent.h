#pragma once

#include "GuiComponent.h"
#include "components/NinePatchComponent.h"

class Font;
class TextCache;

// Used to enter text.
class TextEditComponent : public GuiComponent
{
public:
	TextEditComponent(Window* window);

	void textInput(const char* text) override;
	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void render(const glm::mat4& parentTrans) override;

	void onFocusGained() override;
	void onFocusLost() override;

	void onSizeChanged() override;

	void setValue(const std::string& val) override;
	std::string getValue() const override;

	inline bool isEditing() const { return mEditing; };
	inline const std::shared_ptr<Font>& getFont() const { return mFont; }

	void setCursor(size_t pos);

	std::vector<HelpPrompt> getHelpPrompts() override;

private:
	void startEditing();
	void stopEditing();

	void onTextChanged();
	void onCursorChanged();

	void updateCursorRepeat(int deltaTime);
	void moveCursor(int amt);

	bool isMultiline();
	glm::vec2 getTextAreaPos() const;
	glm::vec2 getTextAreaSize() const;

	std::string mText;
	bool mFocused;
	bool mEditing;
	int mCursor; // cursor position in characters

	int mCursorRepeatTimer;
	int mCursorRepeatDir;

	glm::vec2 mScrollOffset;

	NinePatchComponent mBox;

	std::shared_ptr<Font> mFont;
	std::unique_ptr<TextCache> mTextCache;
};
