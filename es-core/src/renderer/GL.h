#pragma once

// The only inclusion of the OpenGL loader. Private to the renderer module: include it only
// from the .cpp files of es-core/src/renderer/ (the CI step "No GL outside the renderer"
// checks it).
#include <glad/gl.h>
