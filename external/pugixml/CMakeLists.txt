add_library(pugixml STATIC
    src/pugixml.cpp
    pugiconfig.hpp
    pugixml.hpp
)
target_include_directories(pugixml SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/external)
set_target_properties(pugixml PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(pugixml PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
