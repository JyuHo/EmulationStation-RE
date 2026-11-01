#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <alsa/asoundlib.h>
#include <pulse/context.h>
#include <pulse/thread-mainloop.h>

// Pure helpers of VolumeControl, kept apart from ALSA so the tests can check them.
namespace volume {
// Raw mixer value -> 0-100, rounded to the nearest percent and clamped.
int toPercent(long raw, long min, long max);
// 0-100 -> raw mixer value, rounded; toPercent(toRaw(p)) == p for any range of 100 steps or more.
long toRaw(int percent, long min, long max);
// Index in `controls` of the mixer control to drive: `wanted` when it is there, otherwise the
// first of the usual master controls (Master, PCM, Speaker, ...), otherwise the first one;
// -1 when `controls` is empty.
int chooseControl(const std::vector<std::string>& controls, const std::string& wanted);

// The default output of the sound server through libpulse: PulseAudio, or PipeWire through
// pipewire-pulse. ALSA's "default" device can't be trusted for the volume there: on an HDMI
// output, pipewire-alsa shows a "Master" control stuck at 0% that changes nothing.
class PulseSink
{
public:
	PulseSink() = default;
	PulseSink(const PulseSink&) = delete;
	PulseSink& operator=(const PulseSink&) = delete;
	~PulseSink();

	// false when no sound server answers (never spawns one)
	bool connect();
	// 0-100 (0 when muted); false if the server could not be asked
	bool getVolume(int& percent);
	// every channel of the default output, muted at 0
	bool setVolume(int percent);

private:
	void disconnect();
	bool ready();

	pa_threaded_mainloop* mLoop = nullptr;
	pa_context* mContext = nullptr;
};
} // namespace volume

/*!
System volume of the default output. Singleton: call getInstance().

AudioCard picks the backend: "" = automatic, the sound server first (PipeWire or
PulseAudio, see volume::PulseSink), then ALSA's "default", then each card in turn until one
has a playback volume control; an ALSA card ("hw:N") = that card's mixer, with the control
from AudioDevice ("" = automatic, see volume::chooseControl). The volume chosen in the Sound
menu is saved in SystemVolume and applied when the backend is first opened (-1: left as it is).
*/
class VolumeControl
{
	std::string mCard; // the card the mixer is attached to
	std::string mControl;
	snd_mixer_t* mixerHandle = nullptr;
	snd_mixer_elem_t* mixerElem = nullptr;
	std::unique_ptr<volume::PulseSink> mPulse; // the sound server, used instead of ALSA

	static std::weak_ptr<VolumeControl> sInstance;

	VolumeControl();
	VolumeControl(const VolumeControl& right);
	VolumeControl& operator=(const VolumeControl& right);

	bool openMixer(const std::string& card);

public:
	static std::shared_ptr<VolumeControl>& getInstance();

	void init();
	void deinit();

	int getVolume() const;
	void setVolume(int volume);

	// card and control in use ("pulse" for the sound server), "" when none could be opened
	const std::string& getCard() const { return mCard; }
	const std::string& getControl() const { return mControl; }

	// the sound cards: {"hw:N", name}
	static std::vector<std::pair<std::string, std::string>> listCards();
	// those of them with a playback volume control (not HDMI or S/PDIF outputs, whose volume is
	// set by the sound server behind "default"): the choices of the Sound menu
	static std::vector<std::pair<std::string, std::string>> listVolumeCards();

	~VolumeControl();
};
