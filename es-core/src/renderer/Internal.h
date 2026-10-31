#pragma once

#include "math/Math.h"

// Between the files of the renderer module only.
namespace Renderer {

// GL objects of the drawing code (programs, VAO, VBO): created once the context exists,
// destroyed before it is (they are lost with it when a game is launched)
bool initDrawing(glm::ivec2 screenSize);
void deinitDrawing();

// false before init and after deinit: no GL call may be made (the functions are not loaded).
// Window and the resources exist before the first init (Window's constructor loads images).
bool isReady();

// submits the pending batch (before a texture it may use changes or goes away)
void flushBatch();
// before the swap: submits the pending batch and closes the frame's draw call count
void endFrame();

} // namespace Renderer
