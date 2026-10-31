#pragma once

#include "math/Math.h"

// Between the files of the renderer module only.
namespace Renderer {

// GL objects of the drawing code (programs, VAO, VBO): created once the context exists,
// destroyed before it is (they are lost with it when a game is launched)
bool initDrawing(glm::ivec2 screenSize);
void deinitDrawing();

} // namespace Renderer
