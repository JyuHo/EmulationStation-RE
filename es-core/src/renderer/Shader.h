#pragma once

#include "math/Math.h"
#include "renderer/GL.h"

#include <string_view>

namespace Renderer {

// A linked GLSL program and the locations of its uniforms. Private to the renderer module.
class Shader
{
public:
	// false (and a LogError with the info log) if a stage does not compile or link
	bool build(std::string_view vertexSource, std::string_view fragmentSource);
	void destroy(); // needs the context that built it

	void use() const;
	void setProjection(const glm::mat4& projection) const;
	void setModelView(const glm::mat4& modelView) const;

	GLuint id() const { return mProgram; }

private:
	GLuint mProgram = 0;
	GLint mProjection = -1;
	GLint mModelView = -1;
};

} // namespace Renderer
