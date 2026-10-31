#pragma once

#include "GuiComponent.h"
#include "renderer/Renderer.h"
#include <memory>

namespace GridFlags {
enum UpdateType
{
	UPDATE_ALWAYS,
	UPDATE_WHEN_SELECTED,
	UPDATE_NEVER
};

enum Border : unsigned int
{
	BORDER_NONE = 0,

	BORDER_TOP = 1,
	BORDER_BOTTOM = 2,
	BORDER_LEFT = 4,
	BORDER_RIGHT = 8
};
}; // namespace GridFlags

// Used to arrange a bunch of components in a spreadsheet-esque grid.
class ComponentGrid : public GuiComponent
{
public:
	ComponentGrid(Window* window, const glm::ivec2& gridDimensions);
	~ComponentGrid() override;

	bool removeEntry(const std::shared_ptr<GuiComponent>& comp);

	void setEntry(const std::shared_ptr<GuiComponent>& comp, const glm::ivec2& pos, bool canFocus, bool resize = true,
		const glm::ivec2& size = glm::ivec2(1, 1), unsigned int border = GridFlags::BORDER_NONE,
		GridFlags::UpdateType updateType = GridFlags::UPDATE_ALWAYS);

	void textInput(const char* text) override;
	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void render(const glm::mat4& parentTrans) override;
	void onSizeChanged() override;

	void resetCursor();
	bool cursorValid();

	float getColWidth(int col);
	float getRowHeight(int row);

	void setColWidthPerc(int col, float width,
		bool update =
			true); // if update is false, will not call an onSizeChanged() which triggers a (potentially costly) repositioning + resizing of every element
	void setRowHeightPerc(int row, float height,
		bool update =
			true); // if update is false, will not call an onSizeChanged() which triggers a (potentially costly) repositioning + resizing of every element

	bool moveCursor(glm::ivec2 dir);
	void setCursorTo(const std::shared_ptr<GuiComponent>& comp);

	inline std::shared_ptr<GuiComponent> getSelectedComponent()
	{
		GridEntry* e = getCellAt(mCursor);
		if(e)
			return e->component;
		else
			return nullptr;
	}

	void onFocusLost() override;
	void onFocusGained() override;

	std::vector<HelpPrompt> getHelpPrompts() override;

private:
	class GridEntry
	{
	public:
		glm::ivec2 pos;
		glm::ivec2 dim;
		std::shared_ptr<GuiComponent> component;
		bool canFocus;
		bool resize;
		GridFlags::UpdateType updateType;
		unsigned int border;

		GridEntry(const glm::ivec2& p = glm::ivec2(0), const glm::ivec2& d = glm::ivec2(0),
			const std::shared_ptr<GuiComponent>& cmp = nullptr, bool f = false, bool r = true,
			GridFlags::UpdateType u = GridFlags::UPDATE_ALWAYS, unsigned int b = GridFlags::BORDER_NONE) :
			pos(p), dim(d), component(cmp), canFocus(f), resize(r), updateType(u), border(b){};

		operator bool() const { return component != nullptr; }
	};

	std::unique_ptr<float[]> mRowHeights;
	std::unique_ptr<float[]> mColWidths;

	std::vector<Renderer::Vertex> mLines; // pairs of points (one line each), with their colour

	// Update position & size
	void updateCellComponent(const GridEntry& cell);
	void updateSeparators();

	GridEntry* getCellAt(int x, int y);
	inline GridEntry* getCellAt(const glm::ivec2& pos) { return getCellAt(pos.x, pos.y); }

	glm::ivec2 mGridSize;

	std::vector<GridEntry> mCells;

	void onCursorMoved(glm::ivec2 from, glm::ivec2 to);
	glm::ivec2 mCursor;
};
