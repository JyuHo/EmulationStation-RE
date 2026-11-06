#pragma once

#include <SDL3/SDL.h>

// Two flavours of the same GL 3 renderer :
//   USE_GLES  : OpenGL ES 3.x / GLSL ES 3.00 (Raspberry Pi 4 and later)
//   otherwise : desktop OpenGL 3.3 core / GLSL 3.30 (x86_64)
// Entry points come straight from the system libraries (libGLESv2 or glvnd's libOpenGL),
// so no runtime loader is needed.
#if defined(USE_GLES)
#include <GLES3/gl3.h>
#else
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#define GL_CHECK_ERROR(Function) (Function, _GLCheckError(#Function))
bool _GLCheckError(const char* _funcName);


namespace Renderer
{
	struct GLCapabilities
	{
		int  majorVersion = 3;
		int  minorVersion = 0;

		// Core in ES 3.2, otherwise GL_KHR_debug.
		bool debugOutput = false;

		bool isAtLeast(const int major, const int minor) const
		{
			return majorVersion > major || (majorVersion == major && minorVersion >= minor);
		}
	};

	const GLCapabilities& getGLCapabilities();

	// Must be called after the context is current, before any capability is queried.
	void detectGLCapabilities();
}

