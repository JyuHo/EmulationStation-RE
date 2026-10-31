#include "math/Math.h"

#include <cmath>

// std::round, not glm::round: halves go away from zero.
glm::mat4& roundMatrix(glm::mat4& mat)
{
	mat[3].x = std::round(mat[3].x);
	mat[3].y = std::round(mat[3].y);
	return mat;
}

glm::mat4 roundMatrix(const glm::mat4& mat)
{
	glm::mat4 ret = mat;
	roundMatrix(ret);
	return ret;
}

glm::vec3 roundVector(const glm::vec3& vec)
{
	return {std::round(vec.x), std::round(vec.y), std::round(vec.z)};
}

glm::vec2 roundVector(const glm::vec2& vec)
{
	return {std::round(vec.x), std::round(vec.y)};
}
