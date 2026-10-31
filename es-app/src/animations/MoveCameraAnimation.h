#pragma once

#include "animations/Animation.h"

class MoveCameraAnimation : public Animation
{
public:
	MoveCameraAnimation(glm::mat4& camera, const glm::vec3& target) :
		mCameraStart(camera), mTarget(target), cameraOut(camera)
	{}

	int getDuration() const override { return 400; }

	void apply(float t) override
	{
		// cubic ease out
		t -= 1;
		setTranslation(cameraOut, -lerp<glm::vec3>(-getTranslation(mCameraStart), mTarget, t * t * t + 1));
	}

private:
	glm::mat4 mCameraStart;
	glm::vec3 mTarget;

	glm::mat4& cameraOut;
};
