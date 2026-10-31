#pragma once

// Vector and transformation types, from glm (header-only, column-major matrices, the layout
// OpenGL expects): glm::vec2 / vec3 / vec4 / ivec2 / ivec4 and glm::mat4 for 2D/3D affine
// transformations. Composition is post-multiplied: t = glm::translate(t, v) applies v first.
//
// es-core defines GLM_FORCE_CTOR_INIT (default-constructed vectors are zero, matrices identity)
// and GLM_FORCE_EXPLICIT_CTOR (no implicit vec3 -> vec2 or ivec2 -> vec2 conversion) for every
// target that links it, so that all translation units see the same types.
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Translation part of a transformation.
[[nodiscard]] inline glm::vec3 getTranslation(const glm::mat4& mat)
{
	return glm::vec3(mat[3]);
}

// Replaces the translation part only.
inline void setTranslation(glm::mat4& mat, const glm::vec3& translation)
{
	mat[3] = glm::vec4(translation, mat[3].w);
}

// Applies the full transformation to a point.
[[nodiscard]] inline glm::vec3 transformPoint(const glm::mat4& mat, const glm::vec3& point)
{
	return glm::vec3(mat * glm::vec4(point, 1.0f));
}

// Rounds the x and y translation to whole pixels (z is left alone), so that textures are drawn
// on pixel boundaries.
glm::mat4& roundMatrix(glm::mat4& mat);
[[nodiscard]] glm::mat4 roundMatrix(const glm::mat4& mat);

[[nodiscard]] glm::vec3 roundVector(const glm::vec3& vec);
[[nodiscard]] glm::vec2 roundVector(const glm::vec2& vec);
