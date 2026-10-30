add_library(nanosvg STATIC
    src/nanosvg_impl.cpp
    nanosvg.h
    nanosvgrast.h
)
target_include_directories(nanosvg SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/external)
set_target_properties(nanosvg PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(nanosvg PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
