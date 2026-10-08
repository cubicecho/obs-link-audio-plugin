// SPDX-License-Identifier: MIT

#include "input/link-audio-input.hpp"

#include <algorithm>
#include <iterator>

#include <plugin-support.h>
#include <util/platform.h>

namespace {

// Indexed by channel count. Link Audio carries mono or stereo.
constexpr uint64_t NANOSECONDS_PER_MILLISECOND = 1'000'000;

constexpr speaker_layout SPEAKERS_BY_CHANNEL_COUNT[] = {
	SPEAKERS_UNKNOWN,
	SPEAKERS_MONO,
	SPEAKERS_STEREO,
};

speaker_layout speakersFor(size_t channelCount)
{
	const bool isBeyondTable = channelCount >= std::size(SPEAKERS_BY_CHANNEL_COUNT);
	if (isBeyondTable) {
		return SPEAKERS_UNKNOWN;
	}
	return SPEAKERS_BY_CHANNEL_COUNT[channelCount];
}

} // namespace

LinkAudioInput::LinkAudioInput(obs_source_t *source, LinkSession &session, SubscriptionSettings settings)
	: source(source),
	  session(session),
	  settings(settings)
{
	LinkSession::Listener listener;
	listener.onChannelsChanged = [this]() {
		followSelectedChannel();
		obs_source_update_properties(this->source);
	};
	listener.onHeartbeat = [this]() {
		retrySilentSubscription();
	};
	listenerId = session.addListener(std::move(listener));
}

LinkAudioInput::~LinkAudioInput()
{
	session.removeListener(listenerId);
}

void LinkAudioInput::selectChannel(std::string label)
{
	{
		const std::lock_guard lock(selectionMutex);
		selectedLabel = std::move(label);
	}
	followSelectedChannel();
}

std::vector<ChannelChoice> LinkAudioInput::channelChoices()
{
	std::vector<ChannelChoice> choices;
	for (const LinkChannel &channel : session.channels()) {
		// Two peers with the same names share a label. They are listed once, and the first is received.
		const std::string label = channelLabel(channel);
		const bool isListed = std::any_of(choices.begin(), choices.end(),
						  [&](const ChannelChoice &choice) { return choice.label == label; });
		if (isListed == false) {
			choices.push_back({label, true});
		}
	}

	const std::lock_guard lock(selectionMutex);
	const bool selectionIsListed = selectedLabel.empty() ||
				       std::any_of(choices.begin(), choices.end(), [this](const ChannelChoice &choice) {
					       return choice.label == selectedLabel;
				       });
	if (selectionIsListed) {
		return choices;
	}

	choices.push_back({selectedLabel, false});
	return choices;
}

void LinkAudioInput::followSelectedChannel()
{
	const std::lock_guard lock(selectionMutex);
	followSelectedChannelLocked();
}

void LinkAudioInput::followSelectedChannelLocked()
{
	const std::vector<LinkChannel> channels = session.channels();
	const auto selected = std::find_if(channels.begin(), channels.end(), [this](const LinkChannel &channel) {
		return channelLabel(channel) == selectedLabel;
	});

	const bool selectionIsOffline = selected == channels.end();
	if (selectionIsOffline) {
		if (isReceiving) {
			obs_log(LOG_INFO, "'%s' stopped receiving", obs_source_get_name(source));
		}
		subscription.reset();
		isReceiving = false;
		return;
	}

	const bool isAlreadyReceiving = subscription && subscription->id() == selected->id;
	if (isAlreadyReceiving) {
		return;
	}

	subscribeTo(*selected);
	if (isReceiving == false) {
		obs_log(LOG_INFO, "'%s' is receiving '%s'", obs_source_get_name(source), selectedLabel.c_str());
	}
	isReceiving = true;
}

void LinkAudioInput::retrySilentSubscription()
{
	const std::lock_guard lock(selectionMutex);
	if (!subscription) {
		followSelectedChannelLocked();
		return;
	}

	const uint64_t silentNs = os_gettime_ns() - subscribedAtNs;
	const uint64_t retryAfterNs = settings.retryAfterSilentMilliseconds * NANOSECONDS_PER_MILLISECOND;
	const bool isRetryDue = hasReceivedBuffer.load() == false && silentNs >= retryAfterNs;
	if (isRetryDue == false) {
		return;
	}

	// Only dropped here. Link sends the stop for it late, so a request made now would be cancelled by it.
	obs_log(LOG_DEBUG, "'%s' heard nothing and asks for its channel again", obs_source_get_name(source));
	subscription.reset();
}

void LinkAudioInput::subscribeTo(const LinkChannel &channel)
{
	// The old subscription goes first: its callback shares the clock and the flag with the new one.
	subscription.reset();
	hasReceivedBuffer = false;
	subscribedAtNs = os_gettime_ns();
	subscription = session.subscribe(channel.id, [this](ableton::LinkAudioSource::BufferHandle buffer) {
		outputBuffer(buffer);
	});
}

void LinkAudioInput::outputBuffer(const ableton::LinkAudioSource::BufferHandle &buffer)
{
	hasReceivedBuffer = true;

	const speaker_layout speakers = speakersFor(buffer.info.numChannels);
	const bool bufferIsUnusable = speakers == SPEAKERS_UNKNOWN || buffer.info.numFrames == 0 ||
				      buffer.info.sampleRate == 0;
	if (bufferIsUnusable) {
		return;
	}

	const BufferSpan span{static_cast<uint32_t>(buffer.info.numFrames), buffer.info.sampleRate};

	obs_source_audio audio = {};
	audio.data[0] = reinterpret_cast<const uint8_t *>(buffer.samples);
	audio.frames = span.frameCount;
	audio.speakers = speakers;
	audio.format = AUDIO_FORMAT_16BIT;
	audio.samples_per_sec = span.sampleRate;
	audio.timestamp = clock.takeTimestamp(span, os_gettime_ns());

	obs_source_output_audio(source, &audio);
}
