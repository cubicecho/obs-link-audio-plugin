# Link Audio for OBS

An OBS Studio plugin that receives audio from [Ableton Link Audio](https://github.com/Ableton/link) peers on the
local network. Each **Link Audio Input** source plays one channel published by Ableton Live 12.4+ or any other
Link Audio app, with no virtual cables.

Linux is the only platform tested so far. CI also builds for Windows; that build has never been run in OBS.

## Build and install (Ubuntu)

```sh
sudo apt install obs-studio libobs-dev ninja-build
git submodule update --init --recursive

cmake --preset ubuntu-x86_64
cmake --build --preset ubuntu-x86_64

mkdir -p ~/.config/obs-studio/plugins/obs-link-audio/bin/64bit
cp build_x86_64/obs-link-audio.so ~/.config/obs-studio/plugins/obs-link-audio/bin/64bit/
cp -r data ~/.config/obs-studio/plugins/obs-link-audio/
```

Restart OBS afterwards.

## Use

1. Start the app that publishes audio and turn on Link and Link Audio in it.
2. In OBS, add a **Link Audio Input** source.
3. Pick a channel from the **Channel** list. Entries read `Peer / Channel`.

The source remembers the channel by name. When the publishing app restarts, the source reconnects by itself; until
then the channel shows as `(offline)`. The list updates while the properties window is open.

## Things to know

- **Same network only.** Link finds peers by UDP multicast, so OBS and the sender must be on the same subnet and
  the firewall must allow it. With `ufw`, allow UDP from your LAN, for example `sudo ufw allow from 192.168.1.0/24 proto udp`.
- **OBS joins the Link session as a peer named "OBS".** It takes part in tempo sync like any other peer. If OBS was
  running first, an app that joins later can adopt OBS's tempo of 120 bpm. Start the music app first, or set the
  tempo again after OBS joins.
- **Audio starts about a second after a channel is picked.** Link drops a request that reaches a sender which has
  not noticed OBS yet, so the plugin asks again after half a second of silence.
- **Sender and OBS run on different clocks.** The drift is absorbed until it reaches 100 ms, then the stream is
  realigned with a short skip. With typical hardware that is one skip every half hour or so.
- **One source per channel.** Two sources on the same channel in one OBS are not supported: removing one can
  silence the other for up to 5 seconds.
- **Names are the identity.** Two peers with the same peer and channel name appear as one entry, and the first is used.
- The Link Audio API is marked alpha by Ableton, so the Link version is pinned as a submodule in `deps/link`.

## Test without Live

`link-audio-probe` is built alongside the plugin. It is a Link Audio peer for the command line.

```sh
# Publish a 440 Hz tone as "Link Audio Probe / Tone"
build_x86_64/link-audio-probe send [sampleRate] [channelCount] [seconds]

# Receive the first channel found and print what arrives each second
build_x86_64/link-audio-probe receive [seconds]
```

Unit tests run with `ctest --test-dir build_x86_64`.

## Layout

- `src/session/` the Link session OBS shares between all inputs
- `src/input/` the OBS source, and the clock that timestamps received buffers
- `src/shared/defaults.hpp` every tunable value
- `tools/link-audio-probe/` the test peer
- `tests/` unit tests, mirroring `src/`

## Licence

The plugin's own source is MIT, see `LICENSE`. Two things it is built from are not:

- Ableton Link (`deps/link`) and OBS Studio's `libobs` are GPL-2.0-or-later, so a compiled plugin binary can only
  be distributed under the GPL.
- The build scripts in `cmake/`, `build-aux/` and `.github/` come from the GPL-2.0 OBS plugin template.
