add_library(stb STATIC
    src/stb_impl.cpp
    stb_image.h
    stb_image_write.h
)
target_include_directories(stb SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/external)
target_include_directories(stb PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(stb PROPERTIES POSITION_INDEPENDENT_CODE ON)
