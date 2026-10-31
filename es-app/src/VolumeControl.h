#pragma once

#include <memory>
#include <stdint.h>

#include <unistd.h>
#include <fcntl.h>
#include <alsa/asoundlib.h>

/*!
Singleton pattern. Call getInstance() to get an object.
*/
class VolumeControl
{
	static const char* mixerName;
	static const char* mixerCard;
	int mixerIndex = 0;
	snd_mixer_t* mixerHandle = nullptr;
	snd_mixer_elem_t* mixerElem = nullptr;
	snd_mixer_selem_id_t* mixerSelemId = nullptr;

	int originalVolume = 0;
	int internalVolume = 0;

	static std::weak_ptr<VolumeControl> sInstance;

	VolumeControl();
	VolumeControl(const VolumeControl& right);
	VolumeControl& operator=(const VolumeControl& right);

public:
	static std::shared_ptr<VolumeControl>& getInstance();

	void init();
	void deinit();

	int getVolume() const;
	void setVolume(int volume);

	~VolumeControl();
};
