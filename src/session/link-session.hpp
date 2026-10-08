// SPDX-License-Identifier: MIT

#pragma once

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <ableton/LinkAudio.hpp>

#include "shared/defaults.hpp"

/** An audio channel another peer publishes. */
using LinkChannel = ableton::LinkAudio::Channel;

/**
 * A running subscription to one channel.
 * Held by pointer because a LinkAudioSource that is copied or moved silences or crashes the original.
 */
using LinkSubscription = std::unique_ptr<ableton::LinkAudioSource>;

/**
 * Names a channel the way the user sees and saves it.
 *
 * @param channel The channel to name.
 * @return "Peer / Channel".
 */
std::string channelLabel(const LinkChannel &channel);

/**
 * OBS's membership of the Link session on the local network.
 * One instance serves every Link Audio input, so OBS appears to other peers once.
 */
class LinkSession {
public:
	/** What the session tells one input. The two callbacks never run at the same time. */
	struct Listener {
		/** Called on a Link thread when channels appear, disappear or are renamed. */
		std::function<void()> onChannelsChanged;
		/** Called on the session's own thread at every heartbeat. */
		std::function<void()> onHeartbeat;
	};
	/** Identifies a registered listener. */
	using ListenerId = uint64_t;

	/**
	 * Joins the Link session and turns on audio sharing.
	 *
	 * @param settings How OBS presents itself to other peers.
	 */
	explicit LinkSession(LinkSessionSettings settings = LINK_SESSION_DEFAULTS);

	~LinkSession();

	/**
	 * Lists the channels currently published by other peers.
	 *
	 * @return A snapshot, sorted by peer and channel name.
	 */
	std::vector<LinkChannel> channels() const;

	/**
	 * Starts receiving one channel.
	 *
	 * @param channelId The channel to receive.
	 * @param onBuffer Called on a Link thread for each received buffer.
	 * @return The subscription. Destroying it stops the callbacks.
	 */
	template<typename BufferCallback>
	LinkSubscription subscribe(const ableton::ChannelId &channelId, BufferCallback onBuffer)
	{
		return std::make_unique<ableton::LinkAudioSource>(link, channelId, std::move(onBuffer));
	}

	/**
	 * Registers a listener for channel changes and heartbeats.
	 *
	 * @param listener The callbacks to run.
	 * @return The id to remove it with.
	 */
	ListenerId addListener(Listener listener);

	/**
	 * Removes a listener and waits for a call to it that is already running.
	 *
	 * @param listenerId The id addListener returned.
	 */
	void removeListener(ListenerId listenerId);

private:
	void notifyChannelsChanged();
	void runHeartbeat();

	const LinkSessionSettings settings;
	std::mutex listenersMutex;
	std::map<ListenerId, Listener> listeners;
	ListenerId nextListenerId = 0;
	std::mutex heartbeatMutex;
	std::condition_variable heartbeatWake;
	bool isLeaving = false;
	std::thread heartbeatThread;
	// Declared last, so its thread stops before the listeners it calls are destroyed.
	ableton::LinkAudio link;
};

/**
 * Returns the session shared by every input, joining it on first use.
 *
 * @return The shared session.
 */
LinkSession &sharedLinkSession();

/** Leaves the shared session. Call once every input is destroyed. */
void leaveSharedLinkSession();
