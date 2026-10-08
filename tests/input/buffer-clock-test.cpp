// SPDX-License-Identifier: MIT

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "input/buffer-clock.hpp"

namespace {

constexpr uint64_t NS_PER_MILLISECOND = 1'000'000;
constexpr uint64_t FIRST_ARRIVAL_NS = 5'000 * NS_PER_MILLISECOND;
constexpr uint64_t BUFFER_NS = 10 * NS_PER_MILLISECOND;
constexpr BufferSpan TEN_MS_AT_48K{480, 48'000};
constexpr BufferSpan TEN_MS_AT_44K{441, 44'100};
constexpr BufferClockSettings SETTINGS{100};

void expectEqual(uint64_t actual, uint64_t expected, const char *what)
{
	const bool isWrong = actual != expected;
	if (isWrong) {
		std::fprintf(stderr, "FAILED: %s: expected %llu, got %llu\n", what, (unsigned long long)expected,
			     (unsigned long long)actual);
		std::exit(EXIT_FAILURE);
	}
}

void firstBufferStartsOneBufferBeforeItsArrival()
{
	BufferClock clock(SETTINGS);

	const uint64_t timestamp = clock.takeTimestamp(TEN_MS_AT_48K, FIRST_ARRIVAL_NS);

	expectEqual(timestamp, FIRST_ARRIVAL_NS - BUFFER_NS, "first buffer");
}

void jitteredArrivalsStillGiveContiguousTimestamps()
{
	BufferClock clock(SETTINGS);
	const uint64_t jitterNs = 7 * NS_PER_MILLISECOND;
	const uint64_t first = clock.takeTimestamp(TEN_MS_AT_48K, FIRST_ARRIVAL_NS);

	const uint64_t second = clock.takeTimestamp(TEN_MS_AT_48K, FIRST_ARRIVAL_NS + BUFFER_NS + jitterNs);
	const uint64_t third = clock.takeTimestamp(TEN_MS_AT_48K, FIRST_ARRIVAL_NS + 2 * BUFFER_NS - jitterNs);

	expectEqual(second, first + BUFFER_NS, "second buffer after jitter");
	expectEqual(third, first + 2 * BUFFER_NS, "third buffer after jitter");
}

void aLongGapRestartsFromTheArrivalTime()
{
	BufferClock clock(SETTINGS);
	const uint64_t gapNs = 500 * NS_PER_MILLISECOND;
	clock.takeTimestamp(TEN_MS_AT_48K, FIRST_ARRIVAL_NS);

	const uint64_t afterGap = clock.takeTimestamp(TEN_MS_AT_48K, FIRST_ARRIVAL_NS + gapNs);
	const uint64_t next = clock.takeTimestamp(TEN_MS_AT_48K, FIRST_ARRIVAL_NS + gapNs + BUFFER_NS);

	expectEqual(afterGap, FIRST_ARRIVAL_NS + gapNs - BUFFER_NS, "buffer after a gap");
	expectEqual(next, afterGap + BUFFER_NS, "buffer following the restart");
}

void aSampleRateChangeRestartsFromTheArrivalTime()
{
	BufferClock clock(SETTINGS);
	const uint64_t lateNs = 30 * NS_PER_MILLISECOND;
	clock.takeTimestamp(TEN_MS_AT_48K, FIRST_ARRIVAL_NS);

	const uint64_t afterChange = clock.takeTimestamp(TEN_MS_AT_44K, FIRST_ARRIVAL_NS + BUFFER_NS + lateNs);

	expectEqual(afterChange, FIRST_ARRIVAL_NS + lateNs, "buffer after a sample rate change");
}

} // namespace

int main()
{
	firstBufferStartsOneBufferBeforeItsArrival();
	jitteredArrivalsStillGiveContiguousTimestamps();
	aLongGapRestartsFromTheArrivalTime();
	aSampleRateChangeRestartsFromTheArrivalTime();
	std::puts("buffer-clock: all tests passed");
	return EXIT_SUCCESS;
}
