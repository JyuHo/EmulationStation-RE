# set up the external libraries that aren't popular enough to be found on most 
# package managers are included with the project (in the 'external' folder)

add_subdirectory("nanosvg")
if (NOT USE_SYSTEM_PUGIXML)
  add_subdirectory("pugixml" EXCLUDE_FROM_ALL)
endif()

add_subdirectory("id3v2lib" EXCLUDE_FROM_ALL)
add_subdirectory("libcheevos")
add_subdirectory("audio_decoders")
