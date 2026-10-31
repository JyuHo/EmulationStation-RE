#pragma once

#include "GuiComponent.h"
#include "resources/TextureResource.h"
#include <memory>

// Display an image in a way so that edges don't get too distorted no matter the final size. Useful for UI elements like backgrounds, buttons, etc.
// This is accomplished by splitting an image into 9 pieces:
//  ___________
// |_1_|_2_|_3_|
// |_4_|_5_|_6_|
// |_7_|_8_|_9_|

// Corners (1, 3, 7, 9) will not be stretched at all.
// Borders (2, 4, 6, 8) will be stretched along one axis (2 and 8 horizontally, 4 and 6 vertically).
// The center (5) will be stretched.

class NinePatchComponent : public GuiComponent
{
public:
	NinePatchComponent(Window* window, const std::string& path = "", unsigned int edgeColor = 0xFFFFFFFF,
		unsigned int centerColor = 0xFFFFFFFF);
	~NinePatchComponent() override;

	void render(const glm::mat4& parentTrans) override;

	void onSizeChanged() override;

	void fitTo(glm::vec2 size, glm::vec3 position = glm::vec3(0.0f),
		glm::vec2 padding = glm::vec2(0.0f));

	void setImagePath(const std::string& path);
	void setEdgeColor(unsigned int edgeColor);	   // Apply a color shift to the "edge" parts of the ninepatch.
	void setCenterColor(unsigned int centerColor); // Apply a color shift to the "center" part of the ninepatch.

	void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element,
		unsigned int properties) override;

private:
	glm::vec2 getCornerSize() const;

	void buildVertices();
	void updateColors();

	struct Vertex
	{
		glm::vec2 pos;
		glm::vec2 tex;
	};

	std::unique_ptr<Vertex[]> mVertices;
	std::unique_ptr<GLubyte[]> mColors;

	std::string mPath;
	unsigned int mEdgeColor;
	unsigned int mCenterColor;
	std::shared_ptr<TextureResource> mTexture;
};
