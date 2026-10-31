// Phase 4 (Eigen -> glm): the operations the code base performs on Eigen types, done with
// both libraries on the same input. The literal values pin the expected result; the Eigen
// side proves that glm reproduces the current behaviour and goes away with Eigen.
#include "math/Math.h"

#include <catch2/catch_test_macros.hpp>

#include <Eigen/Dense>

#include <cstring>

namespace {
bool same(const glm::mat4& g, const Eigen::Affine3f& e)
{
	// both are column-major 4x4 floats: Renderer::setMatrix passes data() to glLoadMatrixf
	return std::memcmp(glm::value_ptr(g), e.data(), 16 * sizeof(float)) == 0;
}

bool same(const glm::vec3& g, const Eigen::Vector3f& e)
{
	return g.x == e.x() && g.y == e.y() && g.z == e.z();
}
} // namespace

TEST_CASE("glm types default to zero and identity", "[Math]")
{
	CHECK(glm::vec2() == glm::vec2(0.0f, 0.0f));
	CHECK(glm::vec3() == glm::vec3(0.0f, 0.0f, 0.0f));
	CHECK(glm::ivec2() == glm::ivec2(0, 0));
	CHECK(glm::mat4() == glm::mat4(1.0f));
}

TEST_CASE("glm::mat4 has the memory layout of Eigen::Affine3f", "[Math]")
{
	Eigen::Affine3f e = Eigen::Affine3f::Identity();
	e.translate(Eigen::Vector3f(10, 20, 30));
	glm::mat4 g = glm::translate(glm::mat4(1.0f), glm::vec3(10, 20, 30));

	CHECK(same(g, e));
	// translation in the last column, i.e. elements 12 to 14
	CHECK(glm::value_ptr(g)[12] == 10.0f);
	CHECK(glm::value_ptr(g)[13] == 20.0f);
	CHECK(glm::value_ptr(g)[14] == 30.0f);
}

TEST_CASE("translate, scale and composition match Eigen", "[Math]")
{
	// GuiComponent::getTransform, then parentTrans * getTransform() as in every render()
	Eigen::Affine3f eParent = Eigen::Affine3f::Identity();
	eParent.translate(Eigen::Vector3f(100, 50, 0));
	Eigen::Affine3f eChild = Eigen::Affine3f::Identity();
	eChild.translate(Eigen::Vector3f(12.5f, 7.25f, 0));
	Eigen::Affine3f eTrans = eParent * eChild;
	eTrans.translate(Eigen::Vector3f(-3, 4, 0)); // TextComponent: alignment offset

	glm::mat4 gParent = glm::translate(glm::mat4(1.0f), glm::vec3(100, 50, 0));
	glm::mat4 gChild = glm::translate(glm::mat4(1.0f), glm::vec3(12.5f, 7.25f, 0));
	glm::mat4 gTrans = gParent * gChild;
	gTrans = glm::translate(gTrans, glm::vec3(-3, 4, 0));

	CHECK(same(gTrans, eTrans));
	CHECK(getTranslation(gTrans) == glm::vec3(109.5f, 61.25f, 0));

	// LaunchAnimation: scale, then translate (post-multiplied: the translation is scaled)
	Eigen::Affine3f eCamera = Eigen::Affine3f::Identity();
	eCamera.translate(Eigen::Vector3f(-640, 0, 0));
	eCamera.scale(Eigen::Vector3f(2, 2, 1));
	eCamera.translate(Eigen::Vector3f(10, -20, 0));

	glm::mat4 gCamera = glm::translate(glm::mat4(1.0f), glm::vec3(-640, 0, 0));
	gCamera = glm::scale(gCamera, glm::vec3(2, 2, 1));
	gCamera = glm::translate(gCamera, glm::vec3(10, -20, 0));

	CHECK(same(gCamera, eCamera));
	CHECK(getTranslation(gCamera) == glm::vec3(-620, -40, 0));
}

TEST_CASE("transformPoint and inverse match Eigen", "[Math]")
{
	Eigen::Affine3f e = Eigen::Affine3f::Identity();
	e.translate(Eigen::Vector3f(-1280, 0, 0));
	e.scale(Eigen::Vector3f(0.5f, 0.5f, 1));
	glm::mat4 g = glm::translate(glm::mat4(1.0f), glm::vec3(-1280, 0, 0));
	g = glm::scale(g, glm::vec3(0.5f, 0.5f, 1));

	// TextComponent / ComponentList: trans * dim
	const Eigen::Vector3f eDim = e * Eigen::Vector3f(200, 100, 0);
	const glm::vec3 gDim = transformPoint(g, glm::vec3(200, 100, 0));
	CHECK(same(gDim, eDim));
	CHECK(gDim == glm::vec3(-1180, 50, 0));

	// ViewController::render: visible area from the inverse camera
	const Eigen::Vector3f eStart = e.inverse().translation();
	const Eigen::Vector3f eEnd = e.inverse() * Eigen::Vector3f(1280, 720, 0);
	const glm::mat4 gInverse = glm::inverse(g);
	CHECK(same(getTranslation(gInverse), eStart));
	CHECK(same(transformPoint(gInverse, glm::vec3(1280, 720, 0)), eEnd));
	CHECK(getTranslation(gInverse) == glm::vec3(2560, 0, 0));
	CHECK(transformPoint(gInverse, glm::vec3(1280, 720, 0)) == glm::vec3(5120, 1440, 0));
}

TEST_CASE("setTranslation only replaces the translation", "[Math]")
{
	// SystemView: logoTrans.translation() = trans.translation() + offset
	Eigen::Affine3f e = Eigen::Affine3f::Identity();
	e.scale(Eigen::Vector3f(2, 3, 1));
	e.translation() = Eigen::Vector3f(5, 6, 7);
	glm::mat4 g = glm::scale(glm::mat4(1.0f), glm::vec3(2, 3, 1));
	setTranslation(g, glm::vec3(5, 6, 7));

	CHECK(same(g, e));
}

// The Eigen versions were removed from Util with the migration; the values below are the ones
// both versions produced (checked side by side before the migration).
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
