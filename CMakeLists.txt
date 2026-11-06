cmake_minimum_required(VERSION 3.25)

# C++23 everywhere (GCC 14+ on Armbian trixie, GCC 15 / Clang on Arch)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Graphics API : one GL 3 renderer, built either for desktop OpenGL 3.3 core
# (default on x86_64) or for OpenGL ES 3.x (default on ARM, Raspberry Pi 4+).
option(GL "Build the desktop OpenGL 3.3 core renderer" OFF)
option(GLES3 "Build the OpenGL ES 3.x renderer" OFF)

if(GL AND GLES3)
    message(FATAL_ERROR "GL and GLES3 are mutually exclusive; enable only one graphics API")
endif()

option(CEC "CEC" ON)


option(ENABLE_FILEMANAGER "Set to ON to enable f1 shortcut for filesystem" OFF)
option(DISABLE_KODI "Set to ON to disable kodi in menu" OFF)
option(ENABLE_PULSE "Set to ON to enable pulse audio (versus alsa)" OFF)
option(ENABLE_TTS "Set to ON to enable text to speech" OFF)
option(USE_SYSTEM_PUGIXML "Set to ON to use system-wide pugixml library" OFF)

project(emulationstation-all)

include(GNUInstallDirs)
# Installed files are also looked up in $XDG_DATA_DIRS, this is the fallback when the prefix is not listed there
add_compile_definitions(ES_INSTALL_DATADIR="${CMAKE_INSTALL_FULL_DATADIR}" ES_INSTALL_LOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}")

#-------------------------------------------------------------------------------
#add local find scripts to CMAKE path
LIST(APPEND CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/CMake/Utils
    ${CMAKE_CURRENT_SOURCE_DIR}/CMake/Packages
)

# disable kodi menu
if(DISABLE_KODI)
  MESSAGE("Kodi disabled")
else()
  add_definitions(-D_ENABLE_KODI_)
endif()

# batocera / file manager f1 button
# disable file manager
if(ENABLE_FILEMANAGER)
  MESSAGE("file manager enabled")
  add_definitions(-D_ENABLE_FILEMANAGER_)
endif()

#-------------------------------------------------------------------------------
# pick the graphics API
if(NOT GL AND NOT GLES3)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|aarch64)")
        set(GLES3 ON)
    else()
        set(GL ON)
    endif()
endif()

if(GLES3)
    MESSAGE("-- Graphics API: OpenGL ES 3.x")
else()
    MESSAGE("-- Graphics API: OpenGL 3.3 core")
endif()

#finding necessary packages
#-------------------------------------------------------------------------------
if(GLES3)
    find_package(OpenGLES3 REQUIRED)
else()
    find_package(OpenGL REQUIRED)
    # Prefer glvnd's libOpenGL (no GLX dependency); SDL creates the context itself
    if(TARGET OpenGL::OpenGL)
        set(ES_OPENGL_TARGET OpenGL::OpenGL)
    else()
        set(ES_OPENGL_TARGET OpenGL::GL)
    endif()
endif()
find_package(Freetype REQUIRED)
find_package(FreeImage REQUIRED)
find_package(SDL3 3.2.0 REQUIRED CONFIG COMPONENTS SDL3)
find_package(CURL REQUIRED)
find_package(VLC REQUIRED)
find_package(RapidJSON REQUIRED)

# FriBidi is only used to reorder right-to-left scripts (Arabic, Hebrew) at render time.
# It is optional on purpose: when it is missing, Font.cpp falls back on tryFastBidi(), which
# is what every build used before RTL support was added. pkg-config is a Unix mechanism and
# the Windows dependency package does not ship FriBidi, so this must never be REQUIRED.
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
	pkg_check_modules(FRIBIDI QUIET fribidi)
endif()

if(FRIBIDI_FOUND)
	MESSAGE("-- FriBidi found, using it for right-to-left text reordering")
	add_definitions(-DHAVE_FRIBIDI)
else()
	MESSAGE("-- FriBidi not found, falling back on the built-in right-to-left reordering")
endif()

if(ENABLE_PULSE)
  MESSAGE("pulse audio enabled")
  find_package(PulseAudio REQUIRED)
  add_definitions(-D_ENABLE_PULSE_)
else()
  MESSAGE("pulse audio disabled")
endif()

find_package(udev)
if(UDEV_FOUND)
  MESSAGE("udev found. guns enabled")
  add_definitions(-DHAVE_UDEV)
else()
  MESSAGE("udev not found. guns will be disabled")
endif()

if(ENABLE_TTS)
  MESSAGE("tts enabled")
  find_library(LIBESPEAK_LIBRARIES NAMES espeak REQUIRED)
  if("${LIBESPEAK_LIBRARIES}" STREQUAL "LIBESPEAK_LIBRARIES-NOTFOUND")
	MESSAGE(FATAL_ERROR "espeak not found")
  endif()
  add_definitions(-D_ENABLE_TTS_)
else()
  MESSAGE("tts disabled")
endif()

if(CEC)
  MESSAGE("CEC enabled")
  find_package(libCEC)
else()
  MESSAGE("CEC disabled")
endif()

#add ALSA for Linux
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    find_package(ALSA REQUIRED)
endif()

#-------------------------------------------------------------------------------
#set up compiler flags and excutable names
if(CEC)
  if(DEFINED libCEC_FOUND)
    add_definitions(-DHAVE_LIBCEC)
  endif()
endif()

if(DEFINED SCREENSCRAPER_SOFTNAME)
    add_definitions(-DSCREENSCRAPER_SOFTNAME="${SCREENSCRAPER_SOFTNAME}")
endif()

if(DEFINED SCREENSCRAPER_DEV_LOGIN)
    add_definitions(-DSCREENSCRAPER_DEV_LOGIN="${SCREENSCRAPER_DEV_LOGIN}")
endif()

if(DEFINED GAMESDB_APIKEY)
    add_definitions(-DGAMESDB_APIKEY="${GAMESDB_APIKEY}")
endif()

if(DEFINED CHEEVOS_DEV_LOGIN)
    add_definitions(-DCHEEVOS_DEV_LOGIN="${CHEEVOS_DEV_LOGIN}")
endif()

if(DEFINED HFS_DEV_LOGIN)
    add_definitions(-DHFS_DEV_LOGIN="${HFS_DEV_LOGIN}")
endif()

#-------------------------------------------------------------------------------

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 13)
    message(FATAL_ERROR "GCC 13 or newer is required for C++23")
endif()

add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wno-attributes>)

add_definitions(-DUSE_GL3)
if(GLES3)
    add_definitions(-DUSE_GLES)
endif()

#-------------------------------------------------------------------------------
#add include directories
set(COMMON_INCLUDE_DIRS
    ${FREETYPE_INCLUDE_DIRS}
    ${FreeImage_INCLUDE_DIRS}
    ${FRIBIDI_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIR}
    ${VLC_INCLUDE_DIR}
    ${RAPIDJSON_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/external
    ${CMAKE_CURRENT_SOURCE_DIR}/es-core/src
)

if(ENABLE_PULSE)
 LIST(APPEND COMMON_INCLUDE_DIRS
    ${PULSEAUDIO_INCLUDE_DIR}
      )
endif()

if(USE_SYSTEM_PUGIXML)
  MESSAGE("Use system pugixml")
  find_package(pugixml REQUIRED)
endif()

#add libCEC_INCLUDE_DIR
if(CEC)
  if(DEFINED libCEC_FOUND)
    LIST(APPEND COMMON_INCLUDE_DIRS
      ${libCEC_INCLUDE_DIR}
      )
  endif()
endif()

#add ALSA for Linux
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    LIST(APPEND COMMON_INCLUDE_DIRS
        ${ALSA_INCLUDE_DIRS}
    )
endif()

if(GLES3)
    LIST(APPEND COMMON_INCLUDE_DIRS ${OPENGLES3_INCLUDE_DIR})
endif()

#-------------------------------------------------------------------------------
#define libraries and directories
set(COMMON_LIBRARIES
    ${FREETYPE_LIBRARIES}
    ${FreeImage_LIBRARIES}
    ${FRIBIDI_LIBRARIES}
    SDL3::SDL3
    audiodecoders
    ${CURL_LIBRARIES}
    ${VLC_LIBRARIES}
    pugixml
    nanosvg 
    id3v2lib	
	libcheevos	
)
  
if(UDEV_FOUND)
 LIST(APPEND COMMON_LIBRARIES
    ${UDEV_LIBRARY}
      )
endif()

if(ENABLE_PULSE)
 LIST(APPEND COMMON_LIBRARIES
    ${PULSEAUDIO_LIBRARY}
      )
endif()

if(ENABLE_TTS)
 LIST(APPEND COMMON_LIBRARIES
    ${LIBESPEAK_LIBRARIES}
      )
endif()

#add libCEC_LIBRARIES
if(CEC)
  if(DEFINED libCEC_FOUND)
    LIST(APPEND COMMON_LIBRARIES
      dl
      ${libCEC_LIBRARIES}
      )
  endif()
endif()

#add ALSA for Linux
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    LIST(APPEND COMMON_LIBRARIES
        ${ALSA_LIBRARY}
    )
endif()

if(GLES3)
    LIST(APPEND COMMON_LIBRARIES EGL ${OPENGLES3_LIBRARIES})
else()
    LIST(APPEND COMMON_LIBRARIES ${ES_OPENGL_TARGET})
endif()

# batocera / i18n
# i18n
find_program (MSGFMT_EXECUTABLE msgfmt)
find_program (MSGMERGE_EXECUTABLE msgmerge)
find_program (XGETTEXT_EXECUTABLE xgettext)
find_package(Intl)

if(MSGFMT_EXECUTABLE AND MSGMERGE_EXECUTABLE AND XGETTEXT_EXECUTABLE AND Intl_FOUND)
  message (STATUS "Native language support enabled.")
  add_definitions(-DHAVE_INTL)
else()
  message (STATUS "Native language support disabled.")
endif()

find_program (PYTHON3_EXECUTABLE python3)
if(PYTHON3_EXECUTABLE)
  add_custom_target (checkgamesdb ALL COMMENT "Checking guns and wheels games db.")
  add_custom_command (TARGET checkgamesdb COMMAND "${PYTHON3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/resources/checkWheelGunGamesResources.py")
endif()

#-------------------------------------------------------------------------------
# set up build directories

set(dir ${CMAKE_CURRENT_SOURCE_DIR})

set(EXECUTABLE_OUTPUT_PATH ${dir} CACHE PATH "Build directory" FORCE)
set(LIBRARY_OUTPUT_PATH ${dir} CACHE PATH "Build directory" FORCE)

#-------------------------------------------------------------------------------
# add each component

add_subdirectory("external")
add_subdirectory("es-core")
add_subdirectory("es-app")

if(MSGFMT_EXECUTABLE AND MSGMERGE_EXECUTABLE AND XGETTEXT_EXECUTABLE AND Intl_FOUND)
  add_subdirectory (locale)
endif()

