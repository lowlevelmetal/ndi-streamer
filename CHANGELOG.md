# Changelog

## 2.0.1

- **Prebuilt portable binary.** Releases now include `ndistreamer-VERSION-linux-x86_64.tar.gz`. It
  runs on any x86-64 Linux with glibc 2.31 or newer (Ubuntu 20.04, Debian 11, RHEL 9, Fedora, Arch
  and later). FFmpeg 9.0 and its libraries are linked in statically, with software decoding of all
  common codecs including AV1, VAAPI and NVDEC hardware decoding, HTTPS, and V4L2 capture. CI
  checks that it runs on Ubuntu, Debian, Rocky Linux, Fedora and Arch.
- New `-DNDISTREAMER_STATIC=ON` build option and `packaging/` scripts to produce the portable
  build yourself.
- HTTPS inputs work with FFmpeg builds that use Mbed TLS: the system CA bundle is found
  automatically.

## 2.0.0

A ground-up rewrite. The goal is unchanged (stream media as an NDI source from Linux), but the
architecture, timing model and feature set are all new.

### Highlights

- **One pipeline for every decoder.** The three duplicated applications (software, VAAPI, CUDA) are
  replaced by a single threaded pipeline: demux → decode → process → present. Bounded queues connect
  the stages and a shared presentation clock drives the output.
- **Accurate A/V timing.** Frames are sent at their presentation timestamps, and audio is the master
  clock while it plays. NDI timecodes are derived from media timestamps, so receivers that
  synchronize on timecode get exact alignment. Variable frame rates, late-starting streams, long
  pauses and inputs without timestamps are handled.
- **Graceful overload.** When the machine can't keep up, video sheds work in stages while audio
  keeps playing. In order: drop late frames, skip processing of late frames, then decode keyframes
  only. Live sources tolerate clock drift and mid-GOP joins.
- **Generic hardware decoding.** `-t auto|vaapi|cuda|vdpau|...` accepts any FFmpeg device type.
  When the device can't handle a stream, that stream falls back to software. GPU frames are
  downloaded directly into NDI's buffer layout.
- **Lower CPU use.** Formats NDI accepts natively are passed through instead of always being
  converted to UYVY as 1.x did. Video is submitted with NDI's asynchronous API, and software
  decoding now uses multiple threads. For 1080p60 H.264, passthrough cut sender CPU from 66 % of a
  core (forcing UYVY) to 36 % with software decoding and 22 % with VAAPI.
- **No NDI SDK needed to build.** The NDI runtime is loaded at start-up from the usual locations,
  `NDI_RUNTIME_DIR_V6`, or `--ndi-lib`. The previous `/opt/ndi` SDK location is still searched.

### New features

- Seamless looping (`--loop`) with a continuous timeline. MP3 encoder delay and padding are
  accounted for.
- Any FFmpeg input: network streams (RTSP, SRT, HLS, ...), capture devices (`-f v4l2`, `x11grab`,
  `pulse`, ...) and protocol options (`-o key=value`).
- FFmpeg video filters (`--vf`), including GPU filters on hardware frames.
- Output pixel format choice (`-p auto|uyvy|nv12|i420|bgra|rgba`).
- Multichannel audio is preserved and sent as 32-bit float. Optional `--audio-rate` and
  `--audio-channels`.
- Stream selection (`--video-stream`, `--audio-stream`, `--no-video`, `--no-audio`), NDI groups
  (`-g`), and audio-only or video-only inputs.
- Interlaced content is flagged correctly for NDI.
- Clean shutdown on Ctrl+C or SIGTERM, levelled logging (`-v`/`-q`), `--stats`, `--version`, and
  meaningful exit codes.

### Fixes compared to 1.x

- Files without an audio stream no longer crash (the stream-count check was wrong and the code
  dereferenced a missing audio stream).
- The NDI frame rate is no longer taken from `codecpar->framerate`, which is often 0/0. It now comes
  from the container's real frame rate.
- Decoders are drained at the end of the input, so the last frames are no longer lost.
- Audio is no longer forced to 16-bit stereo, so no precision or channels are lost.
- Corrupt packets are skipped instead of stopping the stream.
- The CPU no longer busy-waits on queues, and a per-frame NV12 allocation and copy is gone.
- Builds with current FFmpeg: `avcodec_close` was removed upstream.

### Compatibility notes

- The command line is backward compatible: `-i`, `-s` and `-t software|vaapi|cuda` still work.
- Debug builds no longer produce a separately named `ndistreamer_debug` binary.
- Requires FFmpeg 6.0 or newer and a C++20 compiler (GCC 13+, Clang 17+).

### Development

- 85 automated tests, including whole-pipeline tests against a fake `libndi.so`. These check
  pacing, A/V alignment, loop continuity and frame lifetime. Tests also verify the NDI ABI against
  the official SDK headers and stream end to end to a real NDI receiver when the SDK is available.
- CI on Ubuntu 24.04 (FFmpeg 6.1) with GCC, Clang, and ASan/UBSan.
