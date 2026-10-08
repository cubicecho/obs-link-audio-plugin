// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

#include "shared/defaults.hpp"

/** The length of one received audio buffer. */
struct BufferSpan {
	/** Frames in the buffer. */
	uint32_t frameCount;
	/** Sample rate in Hz. */
	uint32_t sampleRate;
};

/**
 * Gives each received buffer a timestamp that follows the previous one exactly.
 * Network jitter then never reaches OBS. The clock restarts from the arrival time when the stream
 * starts, changes sample rate, or has drifted too far from the arrival time.
 */
class BufferClock {
public:
	/**
	 * @param settings When the clock restarts.
	 */
	explicit BufferClock(BufferClockSettings settings = BUFFER_CLOCK_DEFAULTS);

	/**
	 * Returns the timestamp for a buffer and advances the clock past it.
	 *
	 * @param buffer The buffer that just arrived.
	 * @param arrivalNs When it arrived, in nanoseconds on the OBS clock.
	 * @return The time of the buffer's first frame, in nanoseconds on the OBS clock.
	 */
	uint64_t takeTimestamp(BufferSpan buffer, uint64_t arrivalNs);

private:
	const BufferClockSettings settings;
	uint64_t streamStartNs = 0;
	uint64_t framesSinceStart = 0;
	uint32_t sampleRate = 0;
};
