# FindFreeImage
# -------------
# Locates the FreeImage library and exposes the imported target FreeImage::FreeImage.
# FreeImage ships neither a CMake config nor a pkg-config file, hence this module.
# NOTE: FreeImage is scheduled for removal (phase 2 of docs/MODERNIZATION_PLAN.md).

find_path(FreeImage_INCLUDE_DIR NAMES FreeImage.h)
find_library(FreeImage_LIBRARY NAMES freeimage FreeImage)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FreeImage
    REQUIRED_VARS FreeImage_LIBRARY FreeImage_INCLUDE_DIR)

if(FreeImage_FOUND AND NOT TARGET FreeImage::FreeImage)
    add_library(FreeImage::FreeImage UNKNOWN IMPORTED)
    set_target_properties(FreeImage::FreeImage PROPERTIES
        IMPORTED_LOCATION "${FreeImage_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${FreeImage_INCLUDE_DIR}")
endif()

mark_as_advanced(FreeImage_INCLUDE_DIR FreeImage_LIBRARY)
