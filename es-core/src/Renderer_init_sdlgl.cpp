#include "Renderer.h"
#include <iostream>
#include "platform.h"
#include GLHEADER
#include "resources/Font.h"
#include <SDL3/SDL.h>
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

	glMatrixMode(GL_PROJECTION);
	glOrtho(0, display_width, display_height, 0, -1.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

	return true;
}

void deinit()
{
	destroySurface();
}
}; // namespace Renderer
