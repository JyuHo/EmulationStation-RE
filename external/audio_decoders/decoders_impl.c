/* Single compilation unit for the header-only audio decoders used by AudioMixer.
   dr_flac is compiled privately inside AudioMixer.cpp: libcheevos (libchdr) already
   links its own copy with public symbols. */
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
