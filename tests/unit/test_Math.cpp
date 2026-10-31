// The transformations the code base performs, with the values Eigen produced before the
// migration to glm (phase 4; checked side by side with Eigen in PR #30).
#include "math/Math.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("glm types default to zero and identity", "[Math]")
{
	CHECK(glm::vec2() == glm::vec2(0.0f, 0.0f));
	CHECK(glm::vec3() == glm::vec3(0.0f, 0.0f, 0.0f));
	CHECK(glm::ivec2() == glm::ivec2(0, 0));
	CHECK(glm::mat4() == glm::mat4(1.0f));
}

TEST_CASE("glm::mat4 is column-major, as glLoadMatrixf expects", "[Math]")
{
	glm::mat4 g = glm::translate(glm::mat4(1.0f), glm::vec3(10, 20, 30));

	// translation in the last column, i.e. elements 12 to 14
	CHECK(glm::value_ptr(g)[12] == 10.0f);
	CHECK(glm::value_ptr(g)[13] == 20.0f);
	CHECK(glm::value_ptr(g)[14] == 30.0f);
	CHECK(glm::value_ptr(g)[15] == 1.0f);
	CHECK(glm::value_ptr(g)[3] == 0.0f);
}

TEST_CASE("translate, scale and composition", "[Math]")
{
	// GuiComponent::getTransform, then parentTrans * getTransform() as in every render()
	glm::mat4 parent = glm::translate(glm::mat4(1.0f), glm::vec3(100, 50, 0));
	glm::mat4 child = glm::translate(glm::mat4(1.0f), glm::vec3(12.5f, 7.25f, 0));
	glm::mat4 trans = parent * child;
	trans = glm::translate(trans, glm::vec3(-3, 4, 0)); // TextComponent: alignment offset
	CHECK(getTranslation(trans) == glm::vec3(109.5f, 61.25f, 0));
	CHECK(trans[0][0] == 1.0f);

	// LaunchAnimation: scale, then translate (post-multiplied: the translation is scaled)
	glm::mat4 camera = glm::translate(glm::mat4(1.0f), glm::vec3(-640, 0, 0));
	camera = glm::scale(camera, glm::vec3(2, 2, 1));
	camera = glm::translate(camera, glm::vec3(10, -20, 0));
	CHECK(getTranslation(camera) == glm::vec3(-620, -40, 0));
	CHECK(camera[0][0] == 2.0f);
	CHECK(camera[1][1] == 2.0f);
	CHECK(camera[2][2] == 1.0f);
}

TEST_CASE("transformPoint and inverse", "[Math]")
{
	glm::mat4 g = glm::translate(glm::mat4(1.0f), glm::vec3(-1280, 0, 0));
	g = glm::scale(g, glm::vec3(0.5f, 0.5f, 1));

	// TextComponent / ComponentList: trans * dim
	CHECK(transformPoint(g, glm::vec3(200, 100, 0)) == glm::vec3(-1180, 50, 0));

	// ViewController::render: visible area from the inverse camera
	const glm::mat4 inverse = glm::inverse(g);
	CHECK(getTranslation(inverse) == glm::vec3(2560, 0, 0));
	CHECK(transformPoint(inverse, glm::vec3(1280, 720, 0)) == glm::vec3(5120, 1440, 0));
}

TEST_CASE("setTranslation only replaces the translation", "[Math]")
{
	// SystemView: logoTrans.translation() = trans.translation() + offset
	glm::mat4 g = glm::scale(glm::mat4(1.0f), glm::vec3(2, 3, 1));
	setTranslation(g, glm::vec3(5, 6, 7));

	CHECK(getTranslation(g) == glm::vec3(5, 6, 7));
	CHECK(g[0][0] == 2.0f);
	CHECK(g[1][1] == 3.0f);
	CHECK(g[3][3] == 1.0f);
}

TEST_CASE("roundMatrix and roundVector", "[Math]")
{
	glm::mat4 g = glm::translate(glm::mat4(1.0f), glm::vec3(10.5f, -2.5f, 0.75f));
	g = glm::scale(g, glm::vec3(1.5f, 1.5f, 1));

	// halves away from zero, z untouched, scale untouched
	CHECK(getTranslation(roundMatrix(g)) == glm::vec3(11, -3, 0.75f));
	CHECK(roundMatrix(g)[0][0] == 1.5f);

	glm::mat4 inPlace = g;
	CHECK(&roundMatrix(inPlace) == &inPlace);
	CHECK(inPlace == roundMatrix(g));

	CHECK(roundVector(glm::vec2(0.5f, -1.4f)) == glm::vec2(1, -1));
	CHECK(roundVector(glm::vec3(2.49f, -0.5f, 7.5f)) == glm::vec3(2, -1, 8));
}
