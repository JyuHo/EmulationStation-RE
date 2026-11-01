#include "VolumeControl.h"

#include "Log.h"
#include "Settings.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <pulse/introspect.h>
#include <pulse/operation.h>
#include <pulse/volume.h>
#include <string_view>

namespace volume {
int toPercent(long raw, long min, long max)
{
	if(max <= min)
		return 0;
	const long percent = std::lround(static_cast<double>(raw - min) * 100.0 / static_cast<double>(max - min));
	return static_cast<int>(std::clamp(percent, 0L, 100L));
}

long toRaw(int percent, long min, long max)
{
	percent = std::clamp(percent, 0, 100);
	return min + std::lround(static_cast<double>(max - min) * percent / 100.0);
}

int chooseControl(const std::vector<std::string>& controls, const std::string& wanted)
{
	const auto indexOf = [&controls](std::string_view name) -> int {
		const auto it = std::find(controls.cbegin(), controls.cend(), name);
		return it == controls.cend() ? -1 : static_cast<int>(it - controls.cbegin());
	};

	if(!wanted.empty())
	{
		if(const int i = indexOf(wanted); i >= 0)
			return i;
	}

	// the controls that drive the whole output, most global first
	constexpr std::array<std::string_view, 6> usual = {"Master", "PCM", "Speaker", "Headphone", "Digital", "Line Out"};
	for(const std::string_view name : usual)
	{
		if(const int i = indexOf(name); i >= 0)
			return i;
	}
	return controls.empty() ? -1 : 0;
}
} // namespace volume

// ---- PulseSink ------------------------------------------------------------------------

namespace {
constexpr const char* kDefaultSink = "@DEFAULT_SINK@";

// Runs `op` to its end; the mainloop lock is held.
bool finish(pa_threaded_mainloop* loop, pa_operation* op)
{
	if(op == nullptr)
		return false;
	while(pa_operation_get_state(op) == PA_OPERATION_RUNNING)
		pa_threaded_mainloop_wait(loop);
	const bool done = pa_operation_get_state(op) == PA_OPERATION_DONE;
	pa_operation_unref(op);
	return done;
}

struct SinkQuery
{
	pa_threaded_mainloop* loop;
	bool found = false;
	pa_cvolume volume{};
	bool muted = false;
};

struct Success
{
	pa_threaded_mainloop* loop;
	bool ok = false;
};

void onSinkInfo(pa_context*, const pa_sink_info* info, int eol, void* data)
{
	auto* query = static_cast<SinkQuery*>(data);
	if(eol == 0 && info != nullptr)
	{
		query->found = true;
		query->volume = info->volume;
		query->muted = info->mute != 0;
	}
	pa_threaded_mainloop_signal(query->loop, 0);
}

void onSuccess(pa_context*, int success, void* data)
{
	auto* result = static_cast<Success*>(data);
	result->ok = success != 0;
	pa_threaded_mainloop_signal(result->loop, 0);
}

void onStateChanged(pa_context*, void* loop)
{
	pa_threaded_mainloop_signal(static_cast<pa_threaded_mainloop*>(loop), 0);
}

// the default output's volume, under the lock
bool querySink(pa_threaded_mainloop* loop, pa_context* context, SinkQuery& query)
{
	query.loop = loop;
	return finish(loop, pa_context_get_sink_info_by_name(context, kDefaultSink, onSinkInfo, &query)) && query.found;
}
} // namespace

namespace volume {
PulseSink::~PulseSink()
{
	disconnect();
}

void PulseSink::disconnect()
{
	if(mLoop != nullptr)
		pa_threaded_mainloop_stop(mLoop);
	if(mContext != nullptr)
	{
		pa_context_disconnect(mContext);
		pa_context_unref(mContext);
		mContext = nullptr;
	}
	if(mLoop != nullptr)
	{
		pa_threaded_mainloop_free(mLoop);
		mLoop = nullptr;
	}
}

bool PulseSink::connect()
{
	disconnect();
	mLoop = pa_threaded_mainloop_new();
	if(mLoop == nullptr)
		return false;
	mContext = pa_context_new(pa_threaded_mainloop_get_api(mLoop), "EmulationStation");
	if(mContext == nullptr)
	{
		disconnect();
		return false;
	}
	pa_context_set_state_callback(mContext, onStateChanged, mLoop);
	if(pa_context_connect(mContext, nullptr, PA_CONTEXT_NOAUTOSPAWN, nullptr) < 0 ||
		pa_threaded_mainloop_start(mLoop) < 0)
	{
		disconnect();
		return false;
	}

	pa_threaded_mainloop_lock(mLoop);
	pa_context_state_t state;
	while((state = pa_context_get_state(mContext)) != PA_CONTEXT_READY && PA_CONTEXT_IS_GOOD(state))
		pa_threaded_mainloop_wait(mLoop);
	// a server without any output is no use either
	SinkQuery query{mLoop};
	const bool usable = state == PA_CONTEXT_READY && querySink(mLoop, mContext, query);
	pa_threaded_mainloop_unlock(mLoop);

	if(!usable)
		disconnect();
	return usable;
}

bool PulseSink::ready()
{
	if(mContext != nullptr && pa_context_get_state(mContext) == PA_CONTEXT_READY)
		return true;
	// the server restarted (or went away): one attempt to reach it again
	return connect();
}

bool PulseSink::getVolume(int& percent)
{
	if(!ready())
		return false;
	pa_threaded_mainloop_lock(mLoop);
	SinkQuery query{mLoop};
	const bool ok = querySink(mLoop, mContext, query);
	pa_threaded_mainloop_unlock(mLoop);
	if(!ok)
		return false;
	percent = query.muted ? 0 : toPercent(pa_cvolume_avg(&query.volume), PA_VOLUME_MUTED, PA_VOLUME_NORM);
	return true;
}

bool PulseSink::setVolume(int percent)
{
	if(!ready())
		return false;
	percent = std::clamp(percent, 0, 100);
	pa_threaded_mainloop_lock(mLoop);
	SinkQuery query{mLoop};
	bool ok = querySink(mLoop, mContext, query);
	if(ok)
	{
		// the same level on each channel of the output
		pa_cvolume target;
		pa_cvolume_set(
			&target, query.volume.channels, static_cast<pa_volume_t>(toRaw(percent, PA_VOLUME_MUTED, PA_VOLUME_NORM)));
		Success volumeSet{mLoop};
		ok =
			finish(mLoop, pa_context_set_sink_volume_by_name(mContext, kDefaultSink, &target, onSuccess, &volumeSet)) &&
			volumeSet.ok;
		Success muteSet{mLoop};
		ok = finish(
				 mLoop, pa_context_set_sink_mute_by_name(mContext, kDefaultSink, percent == 0, onSuccess, &muteSet)) &&
			muteSet.ok && ok;
	}
	pa_threaded_mainloop_unlock(mLoop);
	return ok;
}
} // namespace volume

// ---- VolumeControl --------------------------------------------------------------------

std::weak_ptr<VolumeControl> VolumeControl::sInstance;

VolumeControl::VolumeControl()
{
	init();

	// the volume chosen in the Sound menu last time
	const int saved = Settings::getInstance()->getInt("SystemVolume");
	if(saved >= 0 && (mixerElem != nullptr || mPulse != nullptr))
	{
		LOG(LogInfo) << "VolumeControl: restoring the system volume to " << saved << "%";
		setVolume(saved);
	}
}

VolumeControl::VolumeControl(const VolumeControl& right)
{
	sInstance = right.sInstance;
}

VolumeControl& VolumeControl::operator=(const VolumeControl& right)
{
	if(this != &right)
	{
		sInstance = right.sInstance;
	}

	return *this;
}

VolumeControl::~VolumeControl()
{
	deinit();
}

std::shared_ptr<VolumeControl>& VolumeControl::getInstance()
{
	//check if an VolumeControl instance is already created, if not create one
	static std::shared_ptr<VolumeControl> sharedInstance = sInstance.lock();
	if(sharedInstance == nullptr)
	{
		sharedInstance.reset(new VolumeControl);
		sInstance = sharedInstance;
	}
	return sharedInstance;
}

std::vector<std::pair<std::string, std::string>> VolumeControl::listCards()
{
	std::vector<std::pair<std::string, std::string>> cards;
	int card = -1;
	while(snd_card_next(&card) >= 0 && card >= 0)
	{
		char* name = nullptr;
		const std::string id = "hw:" + std::to_string(card);
		if(snd_card_get_name(card, &name) >= 0 && name != nullptr)
		{
			cards.emplace_back(id, name);
			free(name); // allocated by ALSA
		}
		else
		{
			cards.emplace_back(id, id);
		}
	}
	return cards;
}

namespace {
// The mixer of `card` with its playback volume controls, or nullptr when it has none.
snd_mixer_t* loadMixer(const std::string& card, std::vector<std::string>& names, std::vector<snd_mixer_elem_t*>& elems)
{
	snd_mixer_t* handle = nullptr;
	if(snd_mixer_open(&handle, 0) < 0)
	{
		LOG(LogError) << "VolumeControl: failed to open an ALSA mixer";
		return nullptr;
	}
	if(snd_mixer_attach(handle, card.c_str()) < 0 || snd_mixer_selem_register(handle, nullptr, nullptr) < 0 ||
		snd_mixer_load(handle) < 0)
	{
		LOG(LogDebug) << "VolumeControl: no mixer on card \"" << card << "\"";
		snd_mixer_close(handle);
		return nullptr;
	}

	names.clear();
	elems.clear();
	for(snd_mixer_elem_t* elem = snd_mixer_first_elem(handle); elem != nullptr; elem = snd_mixer_elem_next(elem))
	{
		if(snd_mixer_selem_is_active(elem) && snd_mixer_selem_has_playback_volume(elem))
		{
			names.emplace_back(snd_mixer_selem_get_name(elem));
			elems.push_back(elem);
		}
	}
	if(names.empty())
	{
		// an HDMI or S/PDIF output: its volume is set by the sound server ("default")
		LOG(LogDebug) << "VolumeControl: card \"" << card << "\" has no playback volume control";
		snd_mixer_close(handle);
		return nullptr;
	}
	return handle;
}
} // namespace

std::vector<std::pair<std::string, std::string>> VolumeControl::listVolumeCards()
{
	std::vector<std::pair<std::string, std::string>> usable;
	std::vector<std::string> names;
	std::vector<snd_mixer_elem_t*> elems;
	for(auto& card : listCards())
	{
		if(snd_mixer_t* handle = loadMixer(card.first, names, elems))
		{
			snd_mixer_close(handle);
			usable.push_back(std::move(card));
		}
	}
	return usable;
}

bool VolumeControl::openMixer(const std::string& card)
{
	std::vector<std::string> names;
	std::vector<snd_mixer_elem_t*> elems;
	snd_mixer_t* handle = loadMixer(card, names, elems);
	if(handle == nullptr)
		return false;

	const std::string& wanted = Settings::getInstance()->getString("AudioDevice");
	const int chosen = volume::chooseControl(names, wanted);
	if(!wanted.empty() && names[chosen] != wanted)
		LOG(LogWarning) << "VolumeControl: no control \"" << wanted << "\" on card \"" << card << "\", using \""
						<< names[chosen] << "\"";

	mixerHandle = handle;
	mixerElem = elems[chosen];
	mCard = card;
	mControl = names[chosen];
	LOG(LogInfo) << "VolumeControl: card \"" << mCard << "\", control \"" << mControl << "\"";
	return true;
}

void VolumeControl::init()
{
	if(mixerHandle != nullptr || mPulse != nullptr)
		return;

	const std::string& wanted = Settings::getInstance()->getString("AudioCard");
	if(!wanted.empty())
	{
		if(openMixer(wanted))
			return;
		LOG(LogWarning) << "VolumeControl: card \"" << wanted
						<< "\" missing or without a volume control, using the automatic choice (\"Automatic\" in "
						   "the Sound menu)";
	}

	// automatic: the sound server first, it drives the output that SDL plays on
	auto pulse = std::make_unique<volume::PulseSink>();
	if(pulse->connect())
	{
		mPulse = std::move(pulse);
		mCard = "pulse";
		mControl = "@DEFAULT_SINK@";
		LOG(LogInfo) << "VolumeControl: sound server (PipeWire or PulseAudio), default output";
		return;
	}
	LOG(LogDebug) << "VolumeControl: no PipeWire/PulseAudio server, using ALSA";

	// then ALSA: the default device, then each card
	std::vector<std::string> cards;
	if(wanted != "default")
		cards.emplace_back("default");
	for(const auto& [id, name] : listCards())
	{
		if(id != wanted)
			cards.push_back(id);
	}
	for(const std::string& card : cards)
	{
		if(openMixer(card))
			return;
	}
	LOG(LogWarning) << "VolumeControl: no sound server nor sound card with a playback volume control";
}

void VolumeControl::deinit()
{
	if(mPulse != nullptr)
	{
		mPulse.reset();
		mCard.clear();
		mControl.clear();
	}
	if(mixerHandle != nullptr)
	{
		snd_mixer_close(mixerHandle); // detaches the card and frees the elements
		mixerHandle = nullptr;
		mixerElem = nullptr;
		mCard.clear();
		mControl.clear();
	}
}

int VolumeControl::getVolume() const
{
	if(mPulse != nullptr)
	{
		int percent = 0;
		if(!mPulse->getVolume(percent))
			LOG(LogWarning) << "VolumeControl::getVolume() - the sound server did not answer";
		return percent;
	}

	if(mixerElem == nullptr)
		return 0;

	// take in the changes made outside ES since the mixer was loaded
	snd_mixer_handle_events(mixerHandle);

	long minVolume = 0;
	long maxVolume = 0;
	if(snd_mixer_selem_get_playback_volume_range(mixerElem, &minVolume, &maxVolume) != 0)
	{
		LOG(LogError) << "VolumeControl::getVolume() - Failed to get volume range!";
		return 0;
	}

	// the first channel the control has (a mono control only has FRONT_LEFT, alias MONO)
	for(int channel = SND_MIXER_SCHN_FRONT_LEFT; channel <= SND_MIXER_SCHN_LAST; channel++)
	{
		const auto id = static_cast<snd_mixer_selem_channel_id_t>(channel);
		if(!snd_mixer_selem_has_playback_channel(mixerElem, id))
			continue;

		long rawVolume = 0;
		if(snd_mixer_selem_get_playback_volume(mixerElem, id, &rawVolume) != 0)
		{
			LOG(LogError) << "VolumeControl::getVolume() - Failed to get mixer volume!";
			return 0;
		}
		// a muted control plays nothing, whatever its level
		int on = 1;
		if(snd_mixer_selem_has_playback_switch(mixerElem) &&
			snd_mixer_selem_get_playback_switch(mixerElem, id, &on) == 0 && !on)
			return 0;
		return volume::toPercent(rawVolume, minVolume, maxVolume);
	}
	return 0;
}

void VolumeControl::setVolume(int volume)
{
	volume = std::clamp(volume, 0, 100);
	if(mPulse != nullptr)
	{
		if(!mPulse->setVolume(volume))
			LOG(LogWarning) << "VolumeControl::setVolume() - the sound server did not answer";
		return;
	}
	if(mixerElem == nullptr)
		return;

	long minVolume = 0;
	long maxVolume = 0;
	if(snd_mixer_selem_get_playback_volume_range(mixerElem, &minVolume, &maxVolume) != 0)
	{
		LOG(LogError) << "VolumeControl::setVolume() - Failed to get volume range!";
		return;
	}

	// every channel, whatever the layout (a mono control has no FRONT_RIGHT)
	if(snd_mixer_selem_set_playback_volume_all(mixerElem, volume::toRaw(volume, minVolume, maxVolume)) < 0)
		LOG(LogError) << "VolumeControl::setVolume() - Failed to set mixer volume!";

	// a muted control would ignore the new level
	if(snd_mixer_selem_has_playback_switch(mixerElem) &&
		snd_mixer_selem_set_playback_switch_all(mixerElem, volume > 0 ? 1 : 0) < 0)
		LOG(LogError) << "VolumeControl::setVolume() - Failed to switch the mixer on!";
}
