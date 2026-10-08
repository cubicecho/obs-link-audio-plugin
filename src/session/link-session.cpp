// SPDX-License-Identifier: MIT

#include "session/link-session.hpp"

#include <chrono>
#include <memory>

namespace {

const char *const PEER_AND_CHANNEL_SEPARATOR = " / ";

std::mutex sharedSessionMutex;
std::unique_ptr<LinkSession> sharedSession;

} // namespace

std::string channelLabel(const LinkChannel &channel)
{
	return channel.peerName + PEER_AND_CHANNEL_SEPARATOR + channel.name;
}

LinkSession::LinkSession(LinkSessionSettings settings) : settings(settings), link(settings.tempoBpm, settings.peerName)
{
	link.setChannelsChangedCallback([this]() { notifyChannelsChanged(); });
	link.enable(true);
	link.enableLinkAudio(true);
	heartbeatThread = std::thread([this]() { runHeartbeat(); });
}

LinkSession::~LinkSession()
{
	{
		const std::lock_guard lock(heartbeatMutex);
		isLeaving = true;
	}
	heartbeatWake.notify_all();
	heartbeatThread.join();
}

std::vector<LinkChannel> LinkSession::channels() const
{
	return link.channels();
}

LinkSession::ListenerId LinkSession::addListener(Listener listener)
{
	const std::lock_guard lock(listenersMutex);
	const ListenerId listenerId = nextListenerId;
	nextListenerId += 1;
	listeners.emplace(listenerId, std::move(listener));
	return listenerId;
}

void LinkSession::removeListener(ListenerId listenerId)
{
	const std::lock_guard lock(listenersMutex);
	listeners.erase(listenerId);
}

void LinkSession::notifyChannelsChanged()
{
	const std::lock_guard lock(listenersMutex);
	for (const auto &[listenerId, listener] : listeners) {
		listener.onChannelsChanged();
	}
}

void LinkSession::runHeartbeat()
{
	const auto interval = std::chrono::milliseconds(settings.heartbeatMilliseconds);
	std::unique_lock heartbeatLock(heartbeatMutex);
	while (true) {
		const bool shouldStop = heartbeatWake.wait_for(heartbeatLock, interval, [this]() { return isLeaving; });
		if (shouldStop) {
			return;
		}

		const std::lock_guard listenersLock(listenersMutex);
		for (const auto &[listenerId, listener] : listeners) {
			listener.onHeartbeat();
		}
	}
}

LinkSession &sharedLinkSession()
{
	const std::lock_guard lock(sharedSessionMutex);
	if (!sharedSession) {
		sharedSession = std::make_unique<LinkSession>();
	}
	return *sharedSession;
}

void leaveSharedLinkSession()
{
	const std::lock_guard lock(sharedSessionMutex);
	sharedSession.reset();
}
