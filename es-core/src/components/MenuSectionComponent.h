#pragma once

#include "GuiComponent.h"
#include "resources/Font.h"

#include <memory>
#include <string>

class TextComponent;

// Section header of a menu (Recalbox ItemHeader.h:94-153 @10.1.1, docs/THEMES.md "menuSection"):
// the label in capitals with the open mark, and a line in the middle of the row on the free side.
class MenuSectionComponent : public GuiComponent
{
public:
	MenuSectionComponent(Window* window, const std::string& label);

	void onSizeChanged() override;
	void render(const glm::mat4& parentTrans) override;

	// ComponentList recolors the elements of the selected row with setColor(selected text color)
	// and puts the old color back after drawing (themed selector bar). Any color other than the
	// normal one means "selected": the header then draws with its own selected color.
	void setColor(unsigned int color) override;
	unsigned int getColor() const override { return mColor; }

	// true when the row must be drawn after the selector bar (themed bar): the header picks its
	// selected color itself; false with the original inverting bar, as for the labels
	static bool drawnAfterSelector(const Window* window);

private:
	void applyColor();

	std::shared_ptr<Font> mFont;
	std::shared_ptr<TextComponent> mText;
	Alignment mAlignment;
	unsigned int mColor;
	unsigned int mSelectedColor;
	bool mSelected = false;
	float mLabelWidth = 0.0f;
};
