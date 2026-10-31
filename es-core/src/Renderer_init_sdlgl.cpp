#include "Renderer.h"
#include <iostream>
#include "platform.h"
#include GLHEADER
#include "resources/Font.h"
#include <SDL3/SDL.h>
#include <cstddef>
#include "Log.h"
#include "ImageIO.h"
#include "Resources.h"
#include "Settings.h"

namespace Renderer {
static bool initialCursorState;

unsigned int display_width = 0;
unsigned int display_height = 0;

unsigned int getScreenWidth()
{
	return display_width;
}
unsigned int getScreenHeight()
{
	return display_height;
}

SDL_Window* sdlWindow = nullptr;
SDL_GLContext sdlContext = nullptr;

// Shader program state
GLuint shaderProgram = 0;
GLuint vao = 0;
GLuint vbo = 0;
GLint uProjection = -1;
GLint uTexture = -1;
GLint uUseTexture = -1;
GLint uModel = -1;
glm::mat4 projection = glm::mat4(1.0f);
glm::mat4 modelMatrix = glm::mat4(1.0f);

SDL_Window* getSDLWindow()
{
	return sdlWindow;
}

bool createSurface()
{
	LOG(LogInfo) << "Creating surface...";

	if(!SDL_Init(SDL_INIT_VIDEO))
	{
		LOG(LogError) << "Error initializing SDL!\n	" << SDL_GetError();
		return false;
	}

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#ifndef NDEBUG
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif

	// multisample anti-aliasing
	//SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	//SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 2);

	const SDL_DisplayID display = SDL_GetPrimaryDisplay();
	const SDL_DisplayMode* desktopMode = SDL_GetDesktopDisplayMode(display);
	if(desktopMode == nullptr)
	{
		LOG(LogError) << "Error reading the desktop display mode!\n\t" << SDL_GetError();
		return false;
	}
	if(display_width == 0)
		display_width = desktopMode->w;
	if(display_height == 0)
		display_height = desktopMode->h;

	const bool fullscreen = !Settings::getInstance()->getBool("Windowed");
	sdlWindow = SDL_CreateWindow("EmulationStation", display_width, display_height,
		SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY | (fullscreen ? SDL_WINDOW_FULLSCREEN : 0));

	if(sdlWindow == nullptr)
	{
		LOG(LogError) << "Error creating SDL window!\n\t" << SDL_GetError();
		return false;
	}

	if(fullscreen)
	{
		// the desktop mode by default; another --resolution asks for the closest exclusive mode
		SDL_DisplayMode mode{};
		const bool otherMode = (int)display_width != desktopMode->w || (int)display_height != desktopMode->h;
		if(otherMode && SDL_GetClosestFullscreenDisplayMode(display, display_width, display_height, 0.0f, true, &mode))
			SDL_SetWindowFullscreenMode(sdlWindow, &mode);
		else
			SDL_SetWindowFullscreenMode(sdlWindow, nullptr);
		SDL_SyncWindow(sdlWindow);
	}

	// the viewport is in pixels, which differ from window coordinates on high density displays
	int pixelWidth = 0;
	int pixelHeight = 0;
	if(SDL_GetWindowSizeInPixels(sdlWindow, &pixelWidth, &pixelHeight) && pixelWidth > 0 && pixelHeight > 0)
	{
		display_width = pixelWidth;
		display_height = pixelHeight;
	}

	LOG(LogInfo) << "Created window successfully.";

	//set an icon for the window
	size_t width = 0;
	size_t height = 0;
	std::vector<unsigned char> rawData =
		ImageIO::loadFromMemoryRGBA32(window_icon_256_png_data, window_icon_256_png_size, width, height);
	if(!rawData.empty())
	{
		ImageIO::flipPixelsVert(rawData.data(), width, height);

		// SDL_PIXELFORMAT_RGBA32 is R, G, B, A in memory order whatever the endianness
		SDL_Surface* logoSurface = SDL_CreateSurfaceFrom(
			(int)width, (int)height, SDL_PIXELFORMAT_RGBA32, (void*)rawData.data(), (int)width * 4);
		if(logoSurface != nullptr)
		{
			SDL_SetWindowIcon(sdlWindow, logoSurface);
			SDL_DestroySurface(logoSurface);
		}
	}

	sdlContext = SDL_GL_CreateContext(sdlWindow);
	if(sdlContext == nullptr)
	{
		LOG(LogError) << "Error creating OpenGL context!\n\t" << SDL_GetError();
		return false;
	}

	// vsync
	if(Settings::getInstance()->getBool("VSync"))
	{
		// 0 for immediate updates (no vsync, default), 1 for updates synchronized with the vertical
		// retrace, -1 for adaptive vsync (late swap tearing).
		// if vsync is requested, try adaptive vsync; if that doesn't work, try normal vsync
		// if that doesn't work, report an error
		if(!SDL_GL_SetSwapInterval(-1) && !SDL_GL_SetSwapInterval(1))
			LOG(LogWarning) << "Tried to enable vsync, but failed! (" << SDL_GetError() << ")";
	}

	//hide mouse cursor
	initialCursorState = SDL_CursorVisible();
	SDL_HideCursor();

	return true;
}

void swapBuffers()
{
	SDL_GL_SwapWindow(sdlWindow);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void destroySurface()
{
	if(sdlContext)
	{
		SDL_GL_MakeCurrent(sdlWindow, sdlContext);
		if(vbo) glDeleteBuffers(1, &vbo);
		if(vao) glDeleteVertexArrays(1, &vao);
		if(shaderProgram) glDeleteProgram(shaderProgram);
		vbo = 0; vao = 0; shaderProgram = 0;
	}
	SDL_GL_DestroyContext(sdlContext);
	sdlContext = nullptr;

	SDL_DestroyWindow(sdlWindow);
	sdlWindow = nullptr;

	//show mouse cursor
	if(initialCursorState)
		SDL_ShowCursor();

	SDL_Quit();
}

static GLuint compileShader(GLenum type, const char* src)
{
	GLuint s = glCreateShader(type);
	glShaderSource(s, 1, &src, nullptr);
	glCompileShader(s);
	GLint ok = 0;
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if(!ok)
	{
		char buf[1024];
		glGetShaderInfoLog(s, 1024, nullptr, buf);
		LOG(LogError) << "Shader compile error: " << buf;
		glDeleteShader(s);
		return 0;
	}
	return s;
}

static GLuint buildProgram(const char* vs, const char* fs)
{
	GLuint v = compileShader(GL_VERTEX_SHADER, vs);
	GLuint f = compileShader(GL_FRAGMENT_SHADER, fs);
	if(!v || !f)
		return 0;
	GLuint p = glCreateProgram();
	glAttachShader(p, v);
	glAttachShader(p, f);
	glLinkProgram(p);
	GLint ok = 0;
	glGetProgramiv(p, GL_LINK_STATUS, &ok);
	if(!ok)
	{
		char buf[1024];
		glGetProgramInfoLog(p, 1024, nullptr, buf);
		LOG(LogError) << "Program link error: " << buf;
		glDeleteProgram(p);
		p = 0;
	}
	glDeleteShader(v);
	glDeleteShader(f);
	return p;
}

static const char* VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTex;
layout(location = 2) in vec4 aColor;
uniform mat4 uProjection;
uniform mat4 uModel;
out vec2 vTex;
out vec4 vColor;
void main() {
    gl_Position = uProjection * uModel * vec4(aPos, 0.0, 1.0);
    vTex = aTex;
    vColor = aColor;
}
)";

static const char* FS = R"(#version 330 core
in vec2 vTex;
in vec4 vColor;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform int uUseTexture;
void main() {
    if (uUseTexture == 1)
        FragColor = texture(uTexture, vTex) * vColor;
    else
        FragColor = vColor;
}
)";

#ifndef NDEBUG
static void APIENTRY debugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
	if(severity == GL_DEBUG_SEVERITY_NOTIFICATION)
		return;
	LOG(LogWarning) << "GL debug: " << message;
}
#endif

bool init(int w, int h)
{
	if(w)
		display_width = w;
	if(h)
		display_height = h;

	bool createdSurface = createSurface();

	if(!createdSurface)
		return false;

	glViewport(0, 0, display_width, display_height);

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

	projection = glm::ortho(0.0f, (float)display_width, (float)display_height, 0.0f, -1.0f, 1.0f);

#ifndef NDEBUG
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(debugCallback, nullptr);
#endif

	shaderProgram = buildProgram(VS, FS);
	if(!shaderProgram)
		return false;
	glUseProgram(shaderProgram);
	uProjection = glGetUniformLocation(shaderProgram, "uProjection");
	uTexture = glGetUniformLocation(shaderProgram, "uTexture");
	uUseTexture = glGetUniformLocation(shaderProgram, "uUseTexture");
	uModel = glGetUniformLocation(shaderProgram, "uModel");
	glUniformMatrix4fv(uProjection, 1, GL_FALSE, glm::value_ptr(projection));
	glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(modelMatrix));
	glUniform1i(uTexture, 0);

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	// pos
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
	// tex
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tex));
	// color
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, color));

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	return true;
}

void deinit()
{
	destroySurface();
}
}; // namespace Renderer
