#pragma once

#include "animations/Animation.h"
#include "Log.h"

// let's look at the game launch effect:
// -move camera to center on point P (interpolation method: linear)
// -zoom camera to factor Z via matrix scale (interpolation method: exponential)
// -fade screen to black at rate F (interpolation method: exponential)

// How the animation gets constructed from the example of implementing the game launch effect:
// 1. Current parameters are retrieved through a get() call
//    ugliness:
//		-have to have a way to get a reference to the animation
//      -requires additional implementation in some parent object, potentially AnimationController
// 2. ViewController is passed in LaunchAnimation constructor, applies state through setters
//    ugliness:
//      -effect only works for ViewController
// 3. Pass references to ViewController variables - LaunchAnimation(mCameraPos, mFadePerc, target)
//    ugliness:
//      -what if ViewController is deleted? --> AnimationController class handles that
//      -no callbacks for changes...but that works well with this style of update, so I think it's okay
// 4. Use callbacks to set variables
//    ugliness:
//      -boilerplate as hell every time

// #3 wins
// can't wait to see how this one bites me in the ass

class LaunchAnimation : public Animation
{
public:
	//Target is a centerpoint
	LaunchAnimation(glm::mat4& camera, float& fade, const glm::vec3& target, int duration) :
		mCameraStart(camera), mTarget(target), mDuration(duration), cameraOut(camera), fadeOut(fade)
	{}

	int getDuration() const override { return mDuration; }

	void apply(float t) override
	{
		cameraOut = glm::mat4(1.0f);

		float zoom = lerp<float>(1.0, 4.25f, t * t);
		cameraOut = glm::scale(cameraOut, glm::vec3(zoom, zoom, 1));

		const float sw = (float)Renderer::getScreenWidth() / zoom;
		const float sh = (float)Renderer::getScreenHeight() / zoom;

		glm::vec3 centerPoint = lerp<glm::vec3>(-getTranslation(mCameraStart) +
				glm::vec3(Renderer::getScreenWidth() / 2.0f, Renderer::getScreenHeight() / 2.0f, 0),
			mTarget, smoothStep(0.0, 1.0, t));

		cameraOut = glm::translate(cameraOut, glm::vec3((sw / 2) - centerPoint.x, (sh / 2) - centerPoint.y, 0));

		fadeOut = lerp<float>(0.0, 1.0, t * t);
	}

private:
	glm::mat4 mCameraStart;
	glm::vec3 mTarget;
	int mDuration;

	glm::mat4& cameraOut;
	float& fadeOut;
};
