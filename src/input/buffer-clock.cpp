// SPDX-License-Identifier: MIT

#include "input/buffer-clock.hpp"

namespace {

constexpr uint64_t NS_PER_SECOND = 1'000'000'000;
constexpr uint64_t NS_PER_MILLISECOND = 1'000'000;

uint64_t framesToNs(uint64_t frameCount, uint32_t sampleRate)
{
	return frameCount * NS_PER_SECOND / sampleRate;
}

uint64_t distanceNs(uint64_t first, uint64_t second)
{
	return first > second ? first - second : second - first;
}

} // namespace

BufferClock::BufferClock(BufferClockSettings settings) : settings(settings) {}

uint64_t BufferClock::takeTimestamp(BufferSpan buffer, uint64_t arrivalNs)
{
	// A buffer arrives once its last frame exists, so its first frame is one buffer length older.
	const uint64_t arrivalStartNs = arrivalNs - framesToNs(buffer.frameCount, buffer.sampleRate);

	const bool isFirstBuffer = sampleRate == 0;
	const bool sampleRateChanged = buffer.sampleRate != sampleRate;
	const bool shouldRestart = isFirstBuffer || sampleRateChanged;
	if (shouldRestart) {
		streamStartNs = arrivalStartNs;
		framesSinceStart = 0;
		sampleRate = buffer.sampleRate;
	}

	const uint64_t runningNs = streamStartNs + framesToNs(framesSinceStart, sampleRate);
	const uint64_t maxDriftNs = settings.maxDriftMilliseconds * NS_PER_MILLISECOND;
	const bool hasDriftedTooFar = distanceNs(runningNs, arrivalStartNs) > maxDriftNs;
	if (hasDriftedTooFar) {
		streamStartNs = arrivalStartNs;
		framesSinceStart = buffer.frameCount;
		return arrivalStartNs;
	}

	framesSinceStart += buffer.frameCount;
	return runningNs;
}
