#include "GuiComponent.h"
#include "ImageComponent.h"

struct AnimationFrame
{
	const char* path;
	int time;
};

struct AnimationDef
{
	AnimationFrame* frames;
	size_t frameCount;
	bool loop;
};

class AnimatedImageComponent : public GuiComponent
{
public:
	AnimatedImageComponent(Window* window);

	void load(const AnimationDef* def); // no reference to def is kept after loading is complete
	void loadAnimated(const std::string& path); // load an animated WebP/GIF

	void reset(); // set to frame 0

	void update(int deltaTime) override;
	void render(const glm::mat4& trans) override;

	void onSizeChanged() override;

private:
	using ImageFrame = std::pair<std::unique_ptr<ImageComponent>, int>;

	std::vector<ImageFrame> mFrames;

	bool mLoop;
	bool mEnabled;
	int mFrameAccumulator;
	int mCurrentFrame;
};
