# SPDX-License-Identifier: LGPL-3.0-or-later
#
# es-launcher: configures emulators and builds the command that starts a game.
# It is licensed under the LGPL 3.0 or later (see COPYING.LESSER), unlike the rest of
# EmulationStation-RE (MIT), because its generators are adapted from batocera-launch.
# It is built as a shared library so that ES only links to it dynamically.
#
# It depends on nothing from es-core or es-app: ES fills a LaunchContext and calls it.
# It can also be built on its own: cmake -S es-launcher -B build-launcher

cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED PROJECT_NAME)
    project(es-launcher LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    include(GNUInstallDirs)
    set(ES_LAUNCHER_STANDALONE ON)
endif()

option(ES_LAUNCHER_TESTS "Build the es-launcher tests" ${ES_LAUNCHER_STANDALONE})

add_library(es-launcher SHARED
    src/Command.cpp
    src/Generator.cpp
    src/LaunchContext.cpp
    src/Launcher.cpp
    src/Settings.cpp
)

target_include_directories(es-launcher PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(es-launcher PRIVATE ES_LAUNCHER_BUILDING)
target_compile_options(es-launcher PRIVATE -Wall -Wextra)
set_target_properties(es-launcher PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    VERSION 0.1.0
    SOVERSION 0
)

install(TARGETS es-launcher LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

if(ES_LAUNCHER_TESTS)
    enable_testing()
    add_executable(es-launcher-tests tests/launcher_tests.cpp)
    target_link_libraries(es-launcher-tests PRIVATE es-launcher)
    target_compile_options(es-launcher-tests PRIVATE -Wall -Wextra)
    add_test(NAME es-launcher-tests COMMAND es-launcher-tests)
endif()
