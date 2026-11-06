# Find libvlc through pkg-config.
# Defines VLC_FOUND, VLC_INCLUDE_DIR and VLC_LIBRARIES.

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
	pkg_check_modules(PC_VLC QUIET libvlc>=3.0.0)
endif()

find_path(VLC_INCLUDE_DIR
	NAMES vlc/vlc.h
	HINTS ${PC_VLC_INCLUDE_DIRS})

find_library(VLC_LIBRARIES
	NAMES vlc
	HINTS ${PC_VLC_LIBRARY_DIRS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VLC
	REQUIRED_VARS VLC_LIBRARIES VLC_INCLUDE_DIR
	VERSION_VAR PC_VLC_VERSION)

mark_as_advanced(VLC_INCLUDE_DIR VLC_LIBRARIES)
