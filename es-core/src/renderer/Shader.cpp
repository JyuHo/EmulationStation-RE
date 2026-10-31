#include "renderer/Shader.h"

#include "Log.h"

#include <string>

namespace Renderer {

namespace {

std::string shaderLog(GLuint shader)
{
	GLint length = 0;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
	std::string log(length > 0 ? length : 0, '\0');
	if(length > 0)
		glGetShaderInfoLog(shader, length, nullptr, log.data());
	return log;
}

std::string programLog(GLuint program)
{
	GLint length = 0;
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
	std::string log(length > 0 ? length : 0, '\0');
	if(length > 0)
		glGetProgramInfoLog(program, length, nullptr, log.data());
	return log;
}

GLuint compile(GLenum type, std::string_view source)
{
	const GLuint shader = glCreateShader(type);
	const GLchar* text = source.data();
	const GLint length = static_cast<GLint>(source.size());
	glShaderSource(shader, 1, &text, &length);
	glCompileShader(shader);

	GLint ok = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if(ok != GL_TRUE)
	{
		Log::write(LogError, "{} shader does not compile: {}", type == GL_VERTEX_SHADER ? "Vertex" : "Fragment",
			shaderLog(shader));
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

} // namespace

bool Shader::build(std::string_view vertexSource, std::string_view fragmentSource)
{
	destroy();

	const GLuint vertex = compile(GL_VERTEX_SHADER, vertexSource);
	const GLuint fragment = compile(GL_FRAGMENT_SHADER, fragmentSource);
	if(vertex == 0 || fragment == 0)
	{
		glDeleteShader(vertex); // 0 is ignored
		glDeleteShader(fragment);
		return false;
	}

	mProgram = glCreateProgram();
	glAttachShader(mProgram, vertex);
	glAttachShader(mProgram, fragment);
	glLinkProgram(mProgram);
	glDetachShader(mProgram, vertex);
	glDetachShader(mProgram, fragment);
	glDeleteShader(vertex);
	glDeleteShader(fragment);

	GLint ok = GL_FALSE;
	glGetProgramiv(mProgram, GL_LINK_STATUS, &ok);
	if(ok != GL_TRUE)
	{
		Log::write(LogError, "Shader program does not link: {}", programLog(mProgram));
		destroy();
		return false;
	}

	mProjection = glGetUniformLocation(mProgram, "uProjection");
	mModelView = glGetUniformLocation(mProgram, "uModelView");

	// texture unit 0, once and for all (the colour program has no sampler: -1 is ignored)
	glUseProgram(mProgram);
	glUniform1i(glGetUniformLocation(mProgram, "uTexture"), 0);
	return true;
}

void Shader::destroy()
{
	if(mProgram != 0)
		glDeleteProgram(mProgram);
	mProgram = 0;
	mProjection = -1;
	mModelView = -1;
}

void Shader::use() const
{
	glUseProgram(mProgram);
}

// column-major, as glm stores it: no transposition
void Shader::setProjection(const glm::mat4& projection) const
{
	glUniformMatrix4fv(mProjection, 1, GL_FALSE, glm::value_ptr(projection));
}

void Shader::setModelView(const glm::mat4& modelView) const
{
	glUniformMatrix4fv(mModelView, 1, GL_FALSE, glm::value_ptr(modelView));
}

} // namespace Renderer
