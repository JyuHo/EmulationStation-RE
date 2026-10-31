#include "components/AsyncReqComponent.h"
#include "renderer/Renderer.h"

AsyncReqComponent::AsyncReqComponent(Window* window, std::shared_ptr<HttpReq> req,
	std::function<void(std::shared_ptr<HttpReq>)> onSuccess, std::function<void()> onCancel) :
	GuiComponent(window), mSuccessFunc(onSuccess), mCancelFunc(onCancel), mTime(0), mRequest(req)
{}

bool AsyncReqComponent::input(InputConfig* config, Input input)
{
	if(input.value != 0 && config->isMappedTo("b", input))
	{
		if(mCancelFunc)
			mCancelFunc();

		delete this;
	}

	return true;
}

void AsyncReqComponent::update(int deltaTime)
{
	if(mRequest->status() != HttpReq::REQ_IN_PROGRESS)
	{
		mSuccessFunc(mRequest);
		delete this;
		return;
	}

	mTime += deltaTime;
}

void AsyncReqComponent::render(const glm::mat4& parentTrans)
{
	glm::mat4 trans = glm::mat4(1.0f);
	trans = glm::translate(trans, glm::vec3(Renderer::getScreenWidth() / 2.0f, Renderer::getScreenHeight() / 2.0f, 0));
	Renderer::setMatrix(trans);

	glm::vec3 point(cos(mTime * 0.01f) * 12, sin(mTime * 0.01f) * 12, 0);
	Renderer::drawRect((int)point.x, (int)point.y, 8, 8, 0x0000FFFF);
}

std::vector<HelpPrompt> AsyncReqComponent::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.emplace_back("b", "cancel");
	return prompts;
}
