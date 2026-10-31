# glad 2: OpenGL 3.3 core loader (see README.md). Only es-core/src/renderer/ includes it.
add_library(glad STATIC
    src/gl.c
    include/glad/gl.h
    include/KHR/khrplatform.h
)
target_include_directories(glad SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(glad PROPERTIES POSITION_INDEPENDENT_CODE ON)
