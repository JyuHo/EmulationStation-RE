#include "renderer/Renderer.h"

#include "renderer/GL.h"
#include "renderer/Internal.h"
#include <SDL3/SDL.h>
#include <cstdlib>
#include <string_view>
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

SDL_Window* getSDLWindow()
{
	return sdlWindow;
}

// GL_KHR_debug: debug builds, or ES_GL_DEBUG=1 (set by the smoke test and the screenshots)
bool glDebugRequested()
{
#ifndef NDEBUG
	return true;
#else
	const char* env = std::getenv("ES_GL_DEBUG");
	return env != nullptr && std::string_view(env) == "1";
#endif
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
	// read when the window and the context are created: set them before SDL_CreateWindow.
	// No SDL_GL_ALPHA_SIZE: under Wayland an alpha channel makes the window translucent.
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, glDebugRequested() ? SDL_GL_CONTEXT_DEBUG_FLAG : 0);

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
	SDL_GL_DestroyContext(sdlContext);
	sdlContext = nullptr;

	SDL_DestroyWindow(sdlWindow);
	sdlWindow = nullptr;

	//show mouse cursor
	if(initialCursorState)
		SDL_ShowCursor();

	SDL_Quit();
}

namespace {

GLADapiproc loadProc(const char* name)
{
	return reinterpret_cast<GLADapiproc>(SDL_GL_GetProcAddress(name));
}

const char* glString(GLenum name)
{
	const GLubyte* value = glGetString(name);
	return value != nullptr ? reinterpret_cast<const char*>(value) : "unknown";
}

// valid in core and compatibility profiles (glGetString(GL_EXTENSIONS) is not, in core)
void logContextInfo()
{
	Log::write(LogInfo, "GL_VERSION: {}", glString(GL_VERSION));
	Log::write(LogInfo, "GL_RENDERER: {}", glString(GL_RENDERER));
	Log::write(LogInfo, "GL_VENDOR: {}", glString(GL_VENDOR));
	Log::write(LogInfo, "GL_SHADING_LANGUAGE_VERSION: {}", glString(GL_SHADING_LANGUAGE_VERSION));
}

void GLAD_API_PTR debugCallback(GLenum /*source*/, GLenum type, GLuint id, GLenum severity, GLsizei /*length*/,
	const GLchar* message, const void* /*userParam*/)
{
	if(severity == GL_DEBUG_SEVERITY_NOTIFICATION)
		return;

	// a GL error fails the smoke test (it fails on any [ERROR] line): on purpose
	if(type == GL_DEBUG_TYPE_ERROR)
		Log::write(LogError, "OpenGL error {}: {}", id, message);
	else if(severity == GL_DEBUG_SEVERITY_HIGH || severity == GL_DEBUG_SEVERITY_MEDIUM)
		Log::write(LogWarning, "OpenGL {}: {}", id, message);
	else
		Log::write(LogDebug, "OpenGL {}: {}", id, message);
}

void enableDebugOutput()
{
	if(!GLAD_GL_KHR_debug)
	{
		LOG(LogWarning) << "GL_KHR_debug is not available: no OpenGL debug output.";
		return;
	}

	glEnable(GL_DEBUG_OUTPUT);
	// the callback then runs on the calling (main) thread: Log needs no lock
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(debugCallback, nullptr);
	LOG(LogInfo) << "OpenGL debug output enabled (GL_KHR_debug).";
}

} // namespace

bool init(int w, int h)
{
	if(w)
		display_width = w;
	if(h)
		display_height = h;

	if(!createSurface())
		return false;

	// SDL_GL_CreateContext made the context current
	const int version = gladLoadGL(loadProc);
	if(version == 0)
	{
		Log::write(LogError, "Could not load the OpenGL functions: {}", SDL_GetError());
		return false;
	}
	if(GLAD_VERSION_MAJOR(version) < 3 || (GLAD_VERSION_MAJOR(version) == 3 && GLAD_VERSION_MINOR(version) < 3))
	{
		Log::write(LogError, "OpenGL 3.3 core is required, got {}.{}", GLAD_VERSION_MAJOR(version),
			GLAD_VERSION_MINOR(version));
		return false;
	}

	logContextInfo();
	if(glDebugRequested())
		enableDebugOutput();

	// tightly packed rows (font atlases of any width)
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glViewport(0, 0, display_width, display_height);
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

	if(!initDrawing(glm::ivec2(static_cast<int>(display_width), static_cast<int>(display_height))))
		return false;

	return true;
}

void deinit()
{
	// the GL objects go before the context (SDL_GL_DestroyContext in destroySurface)
	if(sdlContext != nullptr)
		deinitDrawing();
	destroySurface();
}

} // namespace Renderer
