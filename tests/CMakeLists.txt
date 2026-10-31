find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

add_executable(es-tests
    unit/test_ImageIO.cpp
    unit/test_Util.cpp
    unit/test_MetaData.cpp
    unit/test_ThemeData.cpp
    unit/test_Paths.cpp
    unit/test_Window.cpp
    unit/test_Locale.cpp
    unit/test_Math.cpp
    unit/test_Input.cpp
    unit/test_Audio.cpp
    unit/test_Renderer.cpp
    unit/test_VideoPlayer.cpp
    unit/test_TextureLoader.cpp
)
target_link_libraries(es-tests PRIVATE es-app-lib Catch2::Catch2WithMain)
target_compile_definitions(es-tests PRIVATE ES_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
es_set_warnings(es-tests)

catch_discover_tests(es-tests)
