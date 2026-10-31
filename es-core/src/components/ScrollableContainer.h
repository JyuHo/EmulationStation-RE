#pragma once

#include "GuiComponent.h"

class ScrollableContainer : public GuiComponent
{
public:
	ScrollableContainer(Window* window);

	glm::vec2 getScrollPos() const;
	void setScrollPos(const glm::vec2& pos);
	void setAutoScroll(bool autoScroll);
	void reset();

	void update(int deltaTime) override;
	void render(const glm::mat4& parentTrans) override;

private:
	glm::vec2 getContentSize();

	glm::vec2 mScrollPos;
	glm::vec2 mScrollDir;
	int mAutoScrollDelay; // ms to wait before starting to autoscroll
	int mAutoScrollSpeed; // ms to wait before scrolling down by mScrollDir
	int mAutoScrollAccumulator;
	bool mAtEnd;
	int mAutoScrollResetAccumulator;
};
