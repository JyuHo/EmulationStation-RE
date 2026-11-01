#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <alsa/asoundlib.h>

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
} // namespace volume

/*!
System volume through an ALSA mixer. Singleton: call getInstance().

The card comes from the AudioCard setting ("" = automatic: "default", then each card in
turn, until one has a playback volume control) and the control from AudioDevice ("" =
automatic, see volume::chooseControl). The volume chosen in the Sound menu is saved in
SystemVolume and applied when the mixer is first opened (-1: left as it is).
*/
class VolumeControl
{
	std::string mCard; // the card the mixer is attached to
	std::string mControl;
	snd_mixer_t* mixerHandle = nullptr;
	snd_mixer_elem_t* mixerElem = nullptr;

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

	// card and control in use, "" when no mixer could be opened
	const std::string& getCard() const { return mCard; }
	const std::string& getControl() const { return mControl; }

	// the sound cards: {"hw:N", name}
	static std::vector<std::pair<std::string, std::string>> listCards();
	// those of them with a playback volume control (not HDMI or S/PDIF outputs, whose volume is
	// set by the sound server behind "default"): the choices of the Sound menu
	static std::vector<std::pair<std::string, std::string>> listVolumeCards();

	~VolumeControl();
};
