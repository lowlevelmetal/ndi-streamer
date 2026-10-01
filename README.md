# ndistreamer

Stream any media file, network stream or capture device as an NDI&reg; source on Linux.

```
ndistreamer -i movie.mp4 -s "Lobby Screen" --loop
```

ndistreamer decodes its input with FFmpeg and publishes it on the network as an NDI source,
sending audio and video in real time with frame-accurate timing. It is a small command line tool
built for servers, signage players and test rigs, with no GUI and no X server needed.

## Features

- **Anything FFmpeg can read.** Files (MP4, MKV, MOV, TS, ...), network streams (RTSP, SRT, HLS,
  RTMP, HTTP), and capture devices (V4L2 webcams and capture cards, X11 screen capture, ALSA and
  PulseAudio).
- **Accurate timing.** Every frame is sent at its presentation time from a shared clock. Audio and
  video stay in sync, and NDI timecodes are derived from media timestamps. Variable frame rate
  content, audio that starts late, and files without timestamps are handled.
- **Seamless looping** with `--loop`. Timestamps keep counting across the loop point, so receivers
  never see a jump.
- **Hardware decoding** with `-t auto|vaapi|cuda|vdpau|...`. If the device can't decode a stream,
  that stream falls back to software decoding.
- **Low overhead.** Formats NDI accepts natively are passed through untouched, and GPU frames are
  downloaded straight into NDI's buffer layout. Video is sent asynchronously, so NDI compresses one
  frame while the next one is being decoded.
- **Full audio.** All channels are kept (5.1, 7.1, ...) and sent as 32-bit float, with optional
  resampling and remixing.
- **FFmpeg filters** with `--vf`: deinterlace, scale, crop, overlay, and more.
- **No NDI SDK needed to build.** The NDI runtime is loaded when the program starts.
- **Well behaved.** Ctrl+C or SIGTERM stops cleanly, errors are reported clearly, exit codes are
  meaningful, and `--stats` gives periodic statistics.

## Requirements

- Linux on x86-64 or ARM
- A C++20 compiler (GCC 13+ or Clang 17+) and CMake 3.20+
- FFmpeg 6.0 or newer, with development headers: libavformat, libavcodec, libavfilter, libavdevice,
  libavutil, libswscale and libswresample
- **At run time:** the NDI runtime (`libndi.so`), which comes with the
  [NDI SDK or NDI Tools](https://ndi.video/download-ndi-sdk/)
- For NDI discovery via mDNS, the Avahi daemon must be running (`systemctl enable --now avahi-daemon`)

Installing the build dependencies:

```sh
# Debian / Ubuntu (24.04 or newer)
sudo apt install build-essential cmake pkg-config libavformat-dev libavcodec-dev libavfilter-dev \
                 libavdevice-dev libavutil-dev libswscale-dev libswresample-dev

# Arch
sudo pacman -S base-devel cmake ffmpeg

# Fedora (with RPM Fusion for the full FFmpeg)
sudo dnf install gcc-c++ cmake ffmpeg-devel
```

## Building

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build        # optional: installs to /usr/local/bin/ndistreamer
```

### Installing the NDI runtime

Download the NDI SDK for Linux and run its installer. It unpacks into `NDI SDK for Linux/`. Then do
one of the following:

- copy `lib/x86_64-linux-gnu/libndi.so*` to `/usr/local/lib` and run `sudo ldconfig`;
- point ndistreamer at it with `--ndi-lib "/path/to/NDI SDK for Linux/lib/x86_64-linux-gnu"`;
- set `NDI_RUNTIME_DIR_V6` to that directory.

ndistreamer searches these places, in order: `--ndi-lib`, `NDI_RUNTIME_DIR_V6`,
`NDI_RUNTIME_DIR_V5`, the system library path, `/usr/local/lib`, and `/opt/ndi` (including the
`/opt/ndi/NDI SDK for Linux` location used by earlier versions of this project).

## Usage

```
ndistreamer -i INPUT [options]
```

| Option | Description |
|---|---|
| `-i, --input INPUT` | File, URL or device to stream (required) |
| `-s, --name NAME` | NDI source name (default: `NDI Source`). It appears on the network as `HOSTNAME (NAME)` |
| `-l, --loop` | Restart the input when it ends, without a gap |
| `-t, --hwaccel TYPE` | Hardware decoding: `none` (default), `auto`, `vaapi`, `cuda`, `vdpau`, or any other FFmpeg device type |
| `--hwaccel-device DEV` | Device to use, e.g. `/dev/dri/renderD129` or a CUDA index |
| `-p, --pixel-format FMT` | NDI video layout: `auto` (default), `uyvy`, `nv12`, `i420`, `bgra`, `rgba` |
| `--vf FILTERGRAPH` | FFmpeg video filters, e.g. `yadif` or `scale=1280:-2` |
| `--audio-rate HZ` | Resample audio (default: keep the source rate) |
| `--audio-channels N` | Remix audio to N channels (default: keep all channels) |
| `-f, --format NAME` | Force the input format, e.g. `v4l2`, `x11grab`, `pulse` |
| `-o, --input-option K=V` | Demuxer or protocol option; can be repeated (e.g. `rtsp_transport=tcp`) |
| `--video-stream N`, `--audio-stream N` | Choose which streams to use (default: the best of each) |
| `--no-video`, `--no-audio` | Send only audio or only video |
| `-g, --groups LIST` | Comma separated NDI groups |
| `--ndi-lib PATH` | `libndi.so` to load, or the directory containing it |
| `--threads N` | Decoder threads (default: automatic) |
| `--no-pacing` | Send as fast as possible instead of in real time (for benchmarking) |
| `--stats[=SECONDS]` | Log statistics periodically (default interval: 5 s) |
| `-v`, `-q` | More or less logging; can be repeated |
| `-h`, `-V` | Show help or version |

Exit status: 0 when the input ends or after Ctrl+C/SIGTERM, 1 on errors, 2 on invalid usage.

### Examples

```sh
# Loop a file forever with GPU decoding
ndistreamer -i promo.mp4 -s "Promo" --loop -t auto

# An IP camera
ndistreamer -i rtsp://10.0.0.20/stream1 -o rtsp_transport=tcp -s "Camera 1"

# A webcam or HDMI capture card, with its audio
ndistreamer -f v4l2 -i /dev/video0 -s Webcam
ndistreamer -f pulse -i default --no-video -s "Desk Mic"

# The desktop
ndistreamer -f x11grab -o framerate=30 -i :0.0 -s Desktop

# Deinterlace and downscale on the way out
ndistreamer -i broadcast.ts --vf "bwdif,scale=1280:-2" -s "Feed (720p)"

# Stereo 48 kHz for receivers that expect it
ndistreamer -i concert.mkv --audio-channels 2 --audio-rate 48000
```

## How it works

```
             ┌─> video packets ─> decode + filter + convert ─> video frames ─> present ─┐
 input ─> demux                                                                         ├─> NDI
             └─> audio packets ─> decode + resample ─────────> audio frames ─> present ─┘
```

Each stage runs on its own thread. The stages are connected by bounded queues, so a slow stage
holds back the ones before it instead of using up memory.

- **Timing.** The presenters release frames at their timestamps using a shared clock. NDI's own
  clocking is turned off. Audio is the master clock while it is being sent; otherwise video takes
  over. If the master falls more than a second behind, the clock re-synchronizes. For live inputs it
  also re-synchronizes when the master runs far ahead, since timestamps there can jump. In files, long
  gaps are real pauses and are honoured.
- **Overload.** If the machine can't keep up, quality degrades in this order:
  1. a late video frame is dropped when a newer one is already waiting;
  2. decoded frames that are already late skip filtering and conversion;
  3. if decoding itself is too slow, the decoder skips ahead to keyframes.

  Audio is never starved. A live source that runs slow is never thinned out, because dropping its
  frames would not help.
- **Video.** Frames come from the decoder (CPU or GPU) and go through the optional filter graph. GPU
  frames are downloaded before the filters run, unless the filters are hardware filters themselves.
  Otherwise they are downloaded afterwards, directly into the NDI layout when the formats match. Then the frame is
  either passed through, repacked into NDI's contiguous plane layout, or converted with swscale.
  NDI reads single-plane frames, such as UYVY or BGRA from the decoder, in place.
- **Pixel formats.** With `auto`, formats NDI accepts (UYVY, NV12, I420, BGRA/BGRX, RGBA/RGBX) pass
  through and everything else becomes UYVY. Measured with 1080p60 H.264 and the NDI 6.3 runtime,
  sender CPU use was:

  | Path | CPU (% of one core) |
  |---|---|
  | software decode → UYVY | 66 % |
  | VAAPI → UYVY | 54 % |
  | software decode → I420 (`auto`) | 36 % |
  | VAAPI → NV12 (`auto`) | 22 % |

  Forcing `-p uyvy` costs CPU and adds no quality for 4:2:0 sources.
- **Audio.** Audio is sent as planar 32-bit float, which is NDI's native format. Most decoders
  already produce it, so it is copied once and needs no conversion.

## Development

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

The test suite does not need the NDI SDK. It streams generated clips (made with the `ffmpeg` CLI)
through the whole pipeline into a fake `libndi.so` that records every frame. It then checks frame
counts, pacing, audio/video alignment, loop continuity, pixel layouts and the async frame lifetime.
Run it under AddressSanitizer and UndefinedBehaviorSanitizer with `-DNDISTREAMER_SANITIZE=ON`.

If you have the NDI SDK, configure with `-DNDI_SDK_DIR="/path/to/NDI SDK for Linux"` to add two
more tests:

- `ndi_abi` checks the project's NDI declarations against the official headers.
- `ndi_end_to_end` streams to a real NDI receiver. It runs a private discovery server, so Avahi is
  not needed.

## Troubleshooting

- **"the NDI runtime (libndi.so) was not found"**: install the runtime (see above) or pass
  `--ndi-lib`.
- **The source doesn't show up on other machines**: make sure `avahi-daemon` is running and that
  your firewall allows mDNS (UDP 5353) and NDI's ports (TCP 5959 and up; the NDI SDK documentation
  lists them all). Alternatively, use an NDI Discovery Server.
- **Hardware decoding isn't used**: run with `-v` to see why. Check `vainfo` for VAAPI, and make
  sure your FFmpeg was built with the hwaccel you need (`ffmpeg -hwaccels`).
- **Frames are dropped or skipped** (`--stats` shows them): the machine can't decode, filter and send
  in real time. Try `-t auto`, or downscale with `--vf scale=...`. Filters work with hardware decoding
  too: frames are downloaded from the GPU automatically unless the filtergraph uses hardware filters
  such as `scale_vaapi`.
- **A live stream takes a few seconds to start**: FFmpeg analyses the start of the stream first. Use
  `-o analyzeduration=1000000` to shorten that. Joining mid-GOP waits for the next keyframe.

## License

MIT, see [LICENSE](LICENSE).

NDI&reg; is a registered trademark of Vizrt NDI AB. This project is not affiliated with or endorsed
by Vizrt NDI AB. The NDI runtime is licensed separately under the NDI SDK license.
