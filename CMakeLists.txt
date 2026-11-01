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
find_package(Fontconfig REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WEBP REQUIRED IMPORTED_TARGET libwebp)
pkg_check_modules(WEBPDEMUX REQUIRED IMPORTED_TARGET libwebpdemux)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswresample libswscale)
find_package(SDL3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(CURL REQUIRED)
find_package(ALSA REQUIRED)
pkg_check_modules(PULSE REQUIRED IMPORTED_TARGET libpulse) # system volume through PipeWire/PulseAudio
find_package(Gettext REQUIRED)
find_program(XGETTEXT_EXECUTABLE xgettext REQUIRED)
find_program(MSGMERGE_EXECUTABLE msgmerge REQUIRED)
find_program(MSGFMT_EXECUTABLE msgfmt REQUIRED)

#-------------------------------------------------------------------------------
# Components
add_subdirectory(external)
add_subdirectory(es-core)
add_subdirectory(es-app)

# --- Internationalisation (phase 10) ---
# Paths relative to the source tree: the "#:" references of the .pot must not depend on
# where the repository is checked out (CI compares it with the committed one).
file(GLOB_RECURSE ES_I18N_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/es-core/src/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/es-core/src/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/es-app/src/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/es-app/src/*.h
)
list(SORT ES_I18N_SOURCES)
set(ES_POT ${CMAKE_CURRENT_SOURCE_DIR}/locale/emulationstation2.pot)

# --force-po: the header is written even while no string is marked yet
add_custom_target(es-update-pot
	COMMAND ${XGETTEXT_EXECUTABLE}
		--language=C++
		--from-code=UTF-8
		--keyword=_
		--keyword=_N:1,2
		--keyword=_P:1c,2
		--keyword=_U
		--keyword=N_
		--add-comments=TRANSLATORS:
		--no-location
		--package-name=emulationstation-re
		--sort-by-file
		--force-po
		-o ${ES_POT}
		${ES_I18N_SOURCES}
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMENT "Updating locale/emulationstation2.pot"
	VERBATIM
)

# CONFIGURE_DEPENDS: a new language is picked up without running cmake by hand
file(GLOB_RECURSE ES_PO_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/locale/lang/*/LC_MESSAGES/emulationstation2.po)
set(ES_MO_FILES)
set(ES_MSGMERGE_COMMANDS)
foreach(po ${ES_PO_FILES})
	# locale/lang/<lang>/LC_MESSAGES/emulationstation2.po
	get_filename_component(lc_dir ${po} DIRECTORY)
	get_filename_component(lang_dir ${lc_dir} DIRECTORY)
	get_filename_component(lang ${lang_dir} NAME)
	set(mo_dir ${CMAKE_BINARY_DIR}/locale/${lang}/LC_MESSAGES)
	set(mo ${mo_dir}/emulationstation2.mo)
	add_custom_command(
		OUTPUT ${mo}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${mo_dir}
		COMMAND ${MSGFMT_EXECUTABLE} --check -o ${mo} ${po}
		DEPENDS ${po}
		COMMENT "Compiling ${lang} translations"
		VERBATIM
	)
	list(APPEND ES_MO_FILES ${mo})
	list(APPEND ES_MSGMERGE_COMMANDS
		COMMAND ${MSGMERGE_EXECUTABLE} --update --backup=none --quiet ${po} ${ES_POT})
	install(FILES ${mo} DESTINATION ${CMAKE_INSTALL_DATADIR}/emulationstation/locale/${lang}/LC_MESSAGES)
endforeach()
if(ES_MO_FILES)
	add_custom_target(es-compile-mo ALL DEPENDS ${ES_MO_FILES})
endif()

# merges the .pot into every .po (run es-update-pot first)
add_custom_target(es-update-po
	${ES_MSGMERGE_COMMANDS}
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMENT "Merging locale/emulationstation2.pot into the .po files"
	VERBATIM
)

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
