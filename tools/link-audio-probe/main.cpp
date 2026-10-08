// SPDX-License-Identifier: MIT

// A test peer for the plugin: "send" publishes a sine tone, "receive" reports what the first channel delivers.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <thread>

#include <ableton/LinkAudio.hpp>
#include <ableton/util/FloatIntConversion.hpp>

#include "defaults.hpp"

namespace {

using SteadyClock = std::chrono::steady_clock;

const char *const MODE_SEND = "send";
const char *const MODE_RECEIVE = "receive";
constexpr double TWO_PI = 6.283185307179586;
constexpr unsigned MILLISECONDS_PER_SECOND = 1'000;
constexpr auto REPORT_INTERVAL = std::chrono::seconds(1);

/** What the receive callback has seen since the last report. */
struct ReceiveCounters {
	std::atomic<uint64_t> bufferCount{0};
	std::atomic<uint64_t> frameCount{0};
	std::atomic<uint64_t> lostBufferCount{0};
	std::atomic<uint32_t> sampleRate{0};
	std::atomic<uint32_t> channelCount{0};
	std::atomic<int> peakSample{0};
};

unsigned argumentOr(int argc, char **argv, int index, unsigned fallback)
{
	const bool isMissing = index >= argc;
	if (isMissing) {
		return fallback;
	}
	return static_cast<unsigned>(std::strtoul(argv[index], nullptr, 10));
}

int printUsage()
{
	std::fprintf(stderr, "usage: link-audio-probe send [sampleRate] [channelCount] [seconds]\n"
			     "       link-audio-probe receive [seconds]\n");
	return EXIT_FAILURE;
}

int send(unsigned sampleRate, unsigned channelCount, unsigned runSeconds)
{
	const size_t framesPerBuffer = sampleRate * PROBE_DEFAULTS.bufferMilliseconds / MILLISECONDS_PER_SECOND;
	const double phasePerFrame = TWO_PI * PROBE_DEFAULTS.toneHz / sampleRate;
	const auto bufferDuration = std::chrono::milliseconds(PROBE_DEFAULTS.bufferMilliseconds);

	ableton::LinkAudio link(PROBE_DEFAULTS.tempoBpm, PROBE_DEFAULTS.peerName);
	link.enable(true);
	link.enableLinkAudio(true);
	ableton::LinkAudioSink sink(link, PROBE_DEFAULTS.channelName, framesPerBuffer * channelCount);
	std::printf("sending '%s' at %u Hz, %u channel(s)\n", PROBE_DEFAULTS.channelName, sampleRate, channelCount);

	const auto start = SteadyClock::now();
	const auto end = start + std::chrono::seconds(runSeconds);
	auto nextBufferAt = start;
	auto nextReportAt = start + REPORT_INTERVAL;
	uint64_t sentBufferCount = 0;
	double phase = 0.0;

	while (SteadyClock::now() < end) {
		std::this_thread::sleep_until(nextBufferAt);
		nextBufferAt += bufferDuration;

		// The sink only hands out a buffer while some peer is receiving the channel.
		ableton::LinkAudioSink::BufferHandle buffer(sink);
		if (buffer) {
			for (size_t frame = 0; frame < framesPerBuffer; ++frame) {
				const int16_t sample =
					ableton::util::floatToInt16(PROBE_DEFAULTS.toneLevel * std::sin(phase));
				for (unsigned channel = 0; channel < channelCount; ++channel) {
					buffer.samples[frame * channelCount + channel] = sample;
				}
				phase = std::fmod(phase + phasePerFrame, TWO_PI);
			}

			const auto sessionState = link.captureAppSessionState();
			const double beatsAtBufferBegin =
				sessionState.beatAtTime(link.clock().micros(), PROBE_DEFAULTS.quantumBeats);
			buffer.commit(sessionState, beatsAtBufferBegin, PROBE_DEFAULTS.quantumBeats, framesPerBuffer,
				      channelCount, sampleRate);
			sentBufferCount += 1;
		}

		const bool isReportDue = SteadyClock::now() >= nextReportAt;
		if (isReportDue) {
			std::printf("peers: %zu, buffers sent in the last second: %llu\n", link.numPeers(),
				    (unsigned long long)sentBufferCount);
			std::fflush(stdout);
			sentBufferCount = 0;
			nextReportAt += REPORT_INTERVAL;
		}
	}
	return EXIT_SUCCESS;
}

void countBuffer(ReceiveCounters &counters, const ableton::LinkAudioSource::BufferHandle &buffer,
		 uint64_t &expectedCount)
{
	const bool hasEarlierBuffer = counters.sampleRate.load() != 0;
	const bool buffersWereLost = hasEarlierBuffer && buffer.info.count > expectedCount;
	if (buffersWereLost) {
		counters.lostBufferCount += buffer.info.count - expectedCount;
	}
	expectedCount = buffer.info.count + 1;

	int peak = counters.peakSample.load();
	const size_t sampleCount = buffer.info.numFrames * buffer.info.numChannels;
	for (size_t index = 0; index < sampleCount; ++index) {
		peak = std::max(peak, std::abs(static_cast<int>(buffer.samples[index])));
	}

	counters.peakSample = peak;
	counters.bufferCount += 1;
	counters.frameCount += buffer.info.numFrames;
	counters.sampleRate = buffer.info.sampleRate;
	counters.channelCount = static_cast<uint32_t>(buffer.info.numChannels);
}

int receive(unsigned runSeconds)
{
	ableton::LinkAudio link(PROBE_DEFAULTS.tempoBpm, PROBE_DEFAULTS.peerName);
	link.enable(true);
	link.enableLinkAudio(true);

	ReceiveCounters counters;
	uint64_t expectedCount = 0;
	std::unique_ptr<ableton::LinkAudioSource> subscription;
	const auto end = SteadyClock::now() + std::chrono::seconds(runSeconds);

	while (SteadyClock::now() < end) {
		std::this_thread::sleep_for(REPORT_INTERVAL);

		if (!subscription) {
			const auto channels = link.channels();
			if (channels.empty()) {
				std::printf("waiting for a channel (peers: %zu)\n", link.numPeers());
				std::fflush(stdout);
				continue;
			}

			const auto &channel = channels.front();
			std::printf("receiving '%s / %s'\n", channel.peerName.c_str(), channel.name.c_str());
			subscription = std::make_unique<ableton::LinkAudioSource>(
				link, channel.id, [&](ableton::LinkAudioSource::BufferHandle buffer) {
					countBuffer(counters, buffer, expectedCount);
				});
			continue;
		}

		std::printf("%u Hz, %u channel(s), buffers: %llu, frames: %llu, lost buffers: %llu, peak: %d\n",
			    counters.sampleRate.load(), counters.channelCount.load(),
			    (unsigned long long)counters.bufferCount.exchange(0),
			    (unsigned long long)counters.frameCount.exchange(0),
			    (unsigned long long)counters.lostBufferCount.exchange(0), counters.peakSample.exchange(0));
		std::fflush(stdout);
	}
	return EXIT_SUCCESS;
}

} // namespace

int main(int argc, char **argv)
{
	const bool hasNoMode = argc < 2;
	if (hasNoMode) {
		return printUsage();
	}

	const char *const mode = argv[1];
	const bool isSend = std::strcmp(mode, MODE_SEND) == 0;
	if (isSend) {
		const unsigned sampleRate = argumentOr(argc, argv, 2, PROBE_DEFAULTS.sampleRate);
		const unsigned channelCount = argumentOr(argc, argv, 3, PROBE_DEFAULTS.channelCount);
		const unsigned runSeconds = argumentOr(argc, argv, 4, PROBE_DEFAULTS.runSeconds);
		return send(sampleRate, channelCount, runSeconds);
	}

	const bool isReceive = std::strcmp(mode, MODE_RECEIVE) == 0;
	if (isReceive) {
		return receive(argumentOr(argc, argv, 2, PROBE_DEFAULTS.runSeconds));
	}

	return printUsage();
}
