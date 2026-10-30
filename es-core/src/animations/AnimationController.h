#pragma once

#include <memory>
#include <functional>
#include "animations/Animation.h"

class AnimationController
{
public:
	// Takes ownership of anim (will delete in destructor).
	AnimationController(
		std::unique_ptr<Animation> anim, int delay = 0, std::function<void()> finishedCallback = nullptr, bool reverse = false);
	virtual ~AnimationController();

	// Returns true if the animation is complete.
	bool update(int deltaTime);

	inline bool isReversed() const { return mReverse; }
	inline int getTime() const { return mTime; }
	inline int getDelay() const { return mDelay; }
	inline const std::function<void()>& getFinishedCallback() const { return mFinishedCallback; }
	inline Animation* getAnimation() const { return mAnimation.get(); }

	inline void removeFinishedCallback() { mFinishedCallback = nullptr; }

private:
	std::unique_ptr<Animation> mAnimation;
	std::function<void()> mFinishedCallback;
	bool mReverse;
	int mTime;
	int mDelay;
};
