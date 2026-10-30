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
find_package(FreeImage REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
# Boost.System (1.69) and Boost.DateTime (1.73) are header-only; the Boost.System
# stub library was removed in 1.89, so only compiled components are requested.
find_package(Boost 1.74 CONFIG REQUIRED COMPONENTS filesystem locale)
find_package(Eigen3 3.3 CONFIG REQUIRED NO_MODULE)
find_package(CURL REQUIRED)
find_package(ALSA REQUIRED)

#-------------------------------------------------------------------------------
# Components
add_subdirectory(external)
add_subdirectory(es-core)
add_subdirectory(es-app)
