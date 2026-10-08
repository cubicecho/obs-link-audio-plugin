// SPDX-License-Identifier: MIT

#pragma once

/** How OBS presents itself to the Link session. */
struct LinkSessionSettings {
	/** The name other peers see for this OBS instance. */
	const char *peerName;
	/** The tempo OBS proposes when it is the first peer, in beats per minute. */
	double tempoBpm;
	/** How often the session prompts its listeners to check on their subscriptions, in milliseconds. */
	unsigned heartbeatMilliseconds;
};

inline constexpr LinkSessionSettings LINK_SESSION_DEFAULTS{
	"OBS",
	120.0,
	250,
};

/** How an input looks after its subscription to a channel. */
struct SubscriptionSettings {
	/**
	 * How long a new subscription may stay silent before it is requested again, in milliseconds.
	 * Link drops a request that reaches a peer which has not discovered OBS yet, then waits 5 s to repeat it.
	 */
	unsigned retryAfterSilentMilliseconds;
};

inline constexpr SubscriptionSettings SUBSCRIPTION_DEFAULTS{
	500,
};

/** How received buffers are placed on the OBS timeline. */
struct BufferClockSettings {
	/**
	 * How far the running timestamp may drift from the arrival time before it restarts, in milliseconds.
	 * Keep it above OBS's own 70 ms smoothing window, or OBS ignores the restart.
	 */
	unsigned maxDriftMilliseconds;
};

inline constexpr BufferClockSettings BUFFER_CLOCK_DEFAULTS{
	100,
};
