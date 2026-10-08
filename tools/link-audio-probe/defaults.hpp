// SPDX-License-Identifier: MIT

#pragma once

/** How the probe presents itself and what it sends. */
struct ProbeSettings {
	/** The name other peers see for the probe. */
	const char *peerName;
	/** The name of the channel the probe publishes. */
	const char *channelName;
	/** The tempo the probe proposes when it is the first peer, in beats per minute. */
	double tempoBpm;
	/** The Link quantum buffers are aligned to, in beats. */
	double quantumBeats;
	/** The pitch of the test tone, in Hz. */
	double toneHz;
	/** The level of the test tone, from 0 to 1. */
	double toneLevel;
	/** The length of each buffer sent, in milliseconds. */
	unsigned bufferMilliseconds;
	/** The sample rate sent when none is given, in Hz. */
	unsigned sampleRate;
	/** The channel count sent when none is given: 1 for mono, 2 for stereo. */
	unsigned channelCount;
	/** How long the probe runs when no duration is given, in seconds. */
	unsigned runSeconds;
};

inline constexpr ProbeSettings PROBE_DEFAULTS{
	"Link Audio Probe", "Tone", 120.0, 4.0, 440.0, 0.25, 10, 48'000, 2, 3'600,
};
