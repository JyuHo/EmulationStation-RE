cmake_minimum_required(VERSION 3.25)

project(emulationstation-re
    VERSION 2.0.0
    DESCRIPTION "A flexible graphical emulator front-end"
    HOMEPAGE_URL "https://github.com/JyuHo/EmulationStation-RE"
    LANGUAGES C CXX
)

#-------------------------------------------------------------------------------
# Global settings
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not supported. Use: cmake -B build")
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(GNUInstallDirs)
include(CompilerWarnings)

#-------------------------------------------------------------------------------
# Options
option(ES_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(ES_ENABLE_SANITIZERS "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(ES_ENABLE_LTO "Enable link-time optimisation for release builds" OFF)
option(ES_BUILD_TESTS "Build the unit tests (requires Catch2 v3)" OFF)

if(ES_ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

if(ES_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ES_IPO_SUPPORTED OUTPUT ES_IPO_ERROR)
    if(ES_IPO_SUPPORTED)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    else()
        message(WARNING "LTO not supported: ${ES_IPO_ERROR}")
    endif()
endif()

#-------------------------------------------------------------------------------
# Dependencies (imported targets only)
set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)
find_package(Freetype REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WEBP REQUIRED IMPORTED_TARGET libwebp)
find_package(SDL2 CONFIG REQUIRED)
# Boost fully removed (phase 3): only the C++ standard library is used.# No version constraint: Arch ships Eigen 5, whose config rejects "3.x" requests.
find_package(Eigen3 CONFIG REQUIRED NO_MODULE)
find_package(CURL REQUIRED)
find_package(ALSA REQUIRED)

#-------------------------------------------------------------------------------
# Components
add_subdirectory(external)
add_subdirectory(es-core)
add_subdirectory(es-app)

if(ES_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

#-------------------------------------------------------------------------------
# Developer tooling
find_program(ES_RUN_CLANG_TIDY NAMES run-clang-tidy run-clang-tidy-18 run-clang-tidy-19)
if(ES_RUN_CLANG_TIDY)
    add_custom_target(es-tidy
        COMMAND ${ES_RUN_CLANG_TIDY} -quiet -p ${CMAKE_BINARY_DIR} "es-(core|app)/src/"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy on es-core and es-app"
        USES_TERMINAL)
endif()
