project("audiodecoders" C)

# dr_mp3 / dr_flac / dr_wav (mackron/dr_libs) and stb_vorbis (nothings/stb),
# both public domain / MIT-0. See dr_libs/VERSION and stb_vorbis/VERSION.
add_library(audiodecoders STATIC
	${CMAKE_CURRENT_SOURCE_DIR}/decoders_impl.c
	${CMAKE_CURRENT_SOURCE_DIR}/stb_vorbis/stb_vorbis.c
)

# PRIVATE on purpose : libcheevos ships an older dr_libs/dr_flac.h.
# Users include "audio_decoders/dr_libs/..." through the external/ include dir.
target_include_directories(audiodecoders PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/dr_libs
	${CMAKE_CURRENT_SOURCE_DIR}/stb_vorbis
)

# Third-party code: keep the build log readable
target_compile_options(audiodecoders PRIVATE -w)
set_target_properties(audiodecoders PROPERTIES POSITION_INDEPENDENT_CODE ON)
