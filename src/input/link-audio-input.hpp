// SPDX-License-Identifier: MIT

#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include <obs-module.h>

#include "input/buffer-clock.hpp"
#include "session/link-session.hpp"

/** One entry the user can pick in the channel list. */
struct ChannelChoice {
	/** The channel's label, which is also the value that is saved. */
	std::string label;
	/** Whether a peer is publishing the channel right now. */
	bool isOnline;
};

/**
 * Feeds one Link Audio channel into one OBS source.
 * It follows the selected channel by name, so it reconnects when the publishing peer restarts.
 */
class LinkAudioInput {
public:
	/**
	 * @param source The OBS source that receives the audio.
	 * @param session The Link session to find channels in.
	 * @param settings How the input looks after its subscription.
	 */
	LinkAudioInput(obs_source_t *source, LinkSession &session,
		       SubscriptionSettings settings = SUBSCRIPTION_DEFAULTS);

	~LinkAudioInput();

	/**
	 * Chooses the channel to receive.
	 *
	 * @param label The channel's label, or an empty string for none.
	 */
	void selectChannel(std::string label);

	/**
	 * Lists what the user can pick: every published channel, plus the selected one when it is offline.
	 *
	 * @return The choices, in display order.
	 */
	std::vector<ChannelChoice> channelChoices();

private:
	void followSelectedChannel();
	// Needs selectionMutex held.
	void followSelectedChannelLocked();
	void retrySilentSubscription();
	void subscribeTo(const LinkChannel &channel);
	void outputBuffer(const ableton::LinkAudioSource::BufferHandle &buffer);

	obs_source_t *const source;
	LinkSession &session;
	const SubscriptionSettings settings;
	LinkSession::ListenerId listenerId = 0;
	// Only the Link thread touches the clock.
	BufferClock clock;
	std::mutex selectionMutex;
	std::string selectedLabel;
	// Stays true across retries, so a retry is not logged as a new connection.
	bool isReceiving = false;
	uint64_t subscribedAtNs = 0;
	std::atomic<bool> hasReceivedBuffer{false};
	// Declared last, so the buffer callbacks stop before the members they use are destroyed.
	LinkSubscription subscription;
};
