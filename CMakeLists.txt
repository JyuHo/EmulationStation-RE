cmake_minimum_required(VERSION 3.10)

option(GLES "Set to ON if targeting Embedded OpenGL" ${GLES})
option(GLES2 "Set to ON if targeting OpenGL ES 2.0" ${GLES2})
option(GLES3 "Set to ON if targeting OpenGL ES 3.0" ${GLES3})
option(GL "Set to ON if targeting Desktop OpenGL" ${GL})

set(OPENGL_OPTION_COUNT 0)
foreach(OPENGL_OPTION GLES GLES2 GLES3 GL)
    if(${OPENGL_OPTION})
        math(EXPR OPENGL_OPTION_COUNT "${OPENGL_OPTION_COUNT} + 1")
    endif()
endforeach()
if(OPENGL_OPTION_COUNT GREATER 1)
    message(FATAL_ERROR "GLES, GLES2, GLES3, and GL are mutually exclusive; enable only one graphics API")
endif()
option(CEC "CEC" ON)

option(BATOCERA "Set to ON to enable BATOCERA specific code" OFF)

option(ENABLE_FILEMANAGER "Set to ON to enable f1 shortcut for filesystem" OFF)
option(DISABLE_KODI "Set to ON to disable kodi in menu" OFF)
option(ENABLE_PULSE "Set to ON to enable pulse audio (versus alsa)" OFF)
option(ENABLE_TTS "Set to ON to enable text to speech" OFF)
option(USE_SYSTEM_PUGIXML "Set to ON to use system-wide pugixml library" OFF)

project(emulationstation-all)

#-------------------------------------------------------------------------------
#add local find scripts to CMAKE path
LIST(APPEND CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/CMake/Utils
    ${CMAKE_CURRENT_SOURCE_DIR}/CMake/Packages
)

if(BATOCERA)
  MESSAGE("Batocera build")
  add_definitions(-DBATOCERA)
endif()

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
#set up OpenGL system variable
if(GLES)
    set(GLSystem "Embedded OpenGL" CACHE STRING "The OpenGL system to be used" FORCE)
elseif(GLES2)
    set(GLSystem "OpenGL ES 2.0" CACHE STRING "The OpenGL system to be used" FORCE)
elseif(GLES3)
    set(GLSystem "OpenGL ES 3.0" CACHE STRING "The OpenGL system to be used" FORCE)
elseif(GL)
    set(GLSystem "Desktop OpenGL" CACHE STRING "The OpenGL system to be used" FORCE)
elseif(NOT GLSystem)
    set(_ES_DEFAULT_GLSYSTEM "Desktop OpenGL")
    find_package(OpenGLES2 QUIET)
    if(OPENGLES2_FOUND OR OpenGLES2_FOUND)
        set(_ES_DEFAULT_GLSYSTEM "OpenGL ES 2.0")
    endif()
    set(GLSystem "${_ES_DEFAULT_GLSYSTEM}" CACHE STRING "The OpenGL system to be used" FORCE)
    unset(_ES_DEFAULT_GLSYSTEM)
endif()

set_property(CACHE GLSystem PROPERTY STRINGS "Desktop OpenGL" "Embedded OpenGL" "OpenGL ES 2.0" "OpenGL ES 3.0")

MESSAGE("-- Graphics API: ${GLSystem}")
if(NOT GLES AND NOT GLES2 AND NOT GLES3 AND NOT GL)
    MESSAGE("-- No graphics API selected; using ${GLSystem}. Pass -DGLES3=ON for the OpenGL ES 3.0 renderer.")
endif()

#finding necessary packages
#-------------------------------------------------------------------------------
if(${GLSystem} MATCHES "Desktop OpenGL")
    find_package(OpenGL REQUIRED)
elseif(${GLSystem} MATCHES "OpenGL ES 3.0")
    find_package(OpenGLES3 REQUIRED)
elseif(${GLSystem} MATCHES "OpenGL ES 2.0")
    find_package(OpenGLES2 REQUIRED)
else()
    find_package(OpenGLES REQUIRED)
endif()
find_package(Freetype REQUIRED)
find_package(FreeImage REQUIRED)
# batocera / background musics
find_package(SDL2MIXER REQUIRED)
# batocera / customisations
find_package(SDL2 REQUIRED)
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

if(CMAKE_COMPILER_IS_GNUCXX)
    #check for G++ 4.7+
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion OUTPUT_VARIABLE G++_VERSION)
    if (G++_VERSION VERSION_LESS 4.7)
            message(SEND_ERROR "You need at least G++ 4.7 to compile EmulationStation!")
    endif()

    #set up compiler flags for GCC
if (CMAKE_BUILD_TYPE MATCHES Debug)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wno-attributes -O0") #support C++17 for std::, optimize
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -O0")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wno-attributes -O3") #support C++17 for std::, optimize
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -O3")  #-s = strip binary
endif()
endif()

if(${GLSystem} MATCHES "Desktop OpenGL")
    add_definitions(-DUSE_OPENGL_21)
elseif(${GLSystem} MATCHES "OpenGL ES 3.0")
    add_definitions(-DUSE_OPENGLES_30)
elseif(${GLSystem} MATCHES "OpenGL ES 2.0")
    add_definitions(-DUSE_OPENGLES_20)
else()
    add_definitions(-DUSE_OPENGLES_10)
endif()

#-------------------------------------------------------------------------------
#add include directories
set(COMMON_INCLUDE_DIRS
    ${FREETYPE_INCLUDE_DIRS}
    ${FreeImage_INCLUDE_DIRS}
    ${FRIBIDI_INCLUDE_DIRS}
    # batocera / background musics
    ${SDLMIXER_INCLUDE_DIR}
    # batocera / customisations    
    ${SDL2_INCLUDE_DIR}
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

if(${GLSystem} MATCHES "Desktop OpenGL")
    LIST(APPEND COMMON_INCLUDE_DIRS
        ${OPENGL_INCLUDE_DIR}
    )
elseif(${GLSystem} MATCHES "OpenGL ES 3.0")
    LIST(APPEND COMMON_INCLUDE_DIRS
        ${OPENGLES3_INCLUDE_DIR}
    )
elseif(${GLSystem} MATCHES "OpenGL ES 2.0")
    LIST(APPEND COMMON_INCLUDE_DIRS
        ${OPENGLES2_INCLUDE_DIR}
    )
else()
    LIST(APPEND COMMON_INCLUDE_DIRS
        ${OPENGLES_INCLUDE_DIR}
    )
endif()

#-------------------------------------------------------------------------------
#define libraries and directories
set(COMMON_LIBRARIES
    ${FREETYPE_LIBRARIES}
    ${FreeImage_LIBRARIES}
    ${FRIBIDI_LIBRARIES}
    ${SDL2_LIBRARY}
    # batocera / background musics
    ${SDLMIXER_LIBRARY}
    # batocera / customisations    
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

if(${GLSystem} MATCHES "Desktop OpenGL")
    LIST(APPEND COMMON_LIBRARIES
        ${OPENGL_LIBRARIES}
    )
elseif(${GLSystem} MATCHES "OpenGL ES 3.0")
    LIST(APPEND COMMON_LIBRARIES
        EGL
        ${OPENGLES3_LIBRARIES}
    )
elseif(${GLSystem} MATCHES "OpenGL ES 2.0")
    LIST(APPEND COMMON_LIBRARIES
        EGL
        ${OPENGLES2_LIBRARIES}
    )
else()
    LIST(APPEND COMMON_LIBRARIES
        EGL
        ${OPENGLES_LIBRARIES}
    )
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

