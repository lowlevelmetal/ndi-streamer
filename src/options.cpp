/**
 * @file options.cpp
 * @brief Command line options.
 */

#include "options.hpp"
#include "version.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <string_view>

#include <getopt.h>

extern "C" {
#include <libavutil/avutil.h>
}

namespace ndistreamer {

namespace {

enum LongOnly {
    kVideoStream = 1000,
    kAudioStream,
    kNoVideo,
    kNoAudio,
    kHwaccelDevice,
    kThreads,
    kVideoFilter,
    kAudioRate,
    kAudioChannels,
    kNdiLib,
    kNoPacing,
    kStats,
};

constexpr option kLongOptions[] = {
    {"input", required_argument, nullptr, 'i'},
    {"format", required_argument, nullptr, 'f'},
    {"input-option", required_argument, nullptr, 'o'},
    {"loop", no_argument, nullptr, 'l'},
    {"video-stream", required_argument, nullptr, kVideoStream},
    {"audio-stream", required_argument, nullptr, kAudioStream},
    {"no-video", no_argument, nullptr, kNoVideo},
    {"no-audio", no_argument, nullptr, kNoAudio},
    {"hwaccel", required_argument, nullptr, 't'},
    {"hwaccel-device", required_argument, nullptr, kHwaccelDevice},
    {"threads", required_argument, nullptr, kThreads},
    {"vf", required_argument, nullptr, kVideoFilter},
    {"pixel-format", required_argument, nullptr, 'p'},
    {"audio-rate", required_argument, nullptr, kAudioRate},
    {"audio-channels", required_argument, nullptr, kAudioChannels},
    {"name", required_argument, nullptr, 's'},
    {"groups", required_argument, nullptr, 'g'},
    {"ndi-lib", required_argument, nullptr, kNdiLib},
    {"no-pacing", no_argument, nullptr, kNoPacing},
    {"stats", optional_argument, nullptr, kStats},
    {"verbose", no_argument, nullptr, 'v'},
    {"quiet", no_argument, nullptr, 'q'},
    {"help", no_argument, nullptr, 'h'},
    {"version", no_argument, nullptr, 'V'},
    {nullptr, 0, nullptr, 0},
};

constexpr const char *kShortOptions = ":i:f:o:lt:p:s:g:vqhV";

std::string Usage(std::string_view program) {
    return std::format(R"(Usage: {0} -i INPUT [options]

Stream a media file, network stream or capture device as an NDI source.

Input:
  -i, --input INPUT          file, URL or device to stream (required)
  -f, --format NAME          force the input format, e.g. v4l2, x11grab, pulse
  -o, --input-option KEY=VAL demuxer/protocol option; repeatable (e.g. rtsp_transport=tcp)
  -l, --loop                 restart the input when it ends, without a gap
      --video-stream N       use stream N for video (default: best)
      --audio-stream N       use stream N for audio (default: best)
      --no-video             do not send video
      --no-audio             do not send audio

Decoding and processing:
  -t, --hwaccel TYPE         hardware decoding: none, auto, vaapi, cuda, vdpau, ...
                             (default: none; falls back to software per stream)
      --hwaccel-device DEV   e.g. /dev/dri/renderD129 or a CUDA device index
      --threads N            decoder threads, 0 = automatic (default: 0)
      --vf FILTERGRAPH       FFmpeg video filters, e.g. "yadif" or "scale=1280:-2"
  -p, --pixel-format FMT     NDI video layout: auto, uyvy, nv12, i420, bgra or rgba
                             (default: auto, which passes through layouts NDI accepts
                             and converts anything else to uyvy)
      --audio-rate HZ        resample audio (default: keep the source rate)
      --audio-channels N     remix audio to N channels (default: keep all channels)

NDI:
  -s, --name NAME            NDI source name (default: "NDI Source")
  -g, --groups LIST          comma separated NDI groups (default: NDI's default groups)
      --ndi-lib PATH         libndi.so to load, or the directory containing it
      --no-pacing            send frames as fast as possible instead of in real time

General:
      --stats[=SECONDS]      log statistics periodically (default interval: 5 s)
  -v, --verbose              more detailed logging; repeat for even more
  -q, --quiet                log only warnings and errors; repeat for errors only
  -h, --help                 show this help
  -V, --version              show version information

Examples:
  {0} -i movie.mp4 -s "Lobby Screen" --loop
  {0} -i movie.mkv -t auto
  {0} -i rtsp://camera/stream -o rtsp_transport=tcp -s Camera
  {0} -f v4l2 -i /dev/video0 -s Webcam
)",
                       program);
}

template <typename T>
bool ParseNumber(std::string_view text, T min, T max, T &out) {
    T value{};
    auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc{} || end != text.data() + text.size() || value < min || value > max) return false;
    out = value;
    return true;
}

} // namespace

ParseOutcome ParseOptions(int argc, char *argv[], Options &options, std::string &message) {
    std::string_view program = argc > 0 ? argv[0] : "ndistreamer";
    if (auto slash = program.rfind('/'); slash != std::string_view::npos) program.remove_prefix(slash + 1);

    auto fail = [&](std::string error) {
        message = std::format("{}: {}\nTry '{} --help' for more information.", program, error, program);
        return ParseOutcome::ExitError;
    };

    int verbosity = 0;
    opterr = 0;
    optind = 0; // fully reinitialize getopt (GNU), so parsing can run more than once

    int opt = 0;
    int long_index = 0;
    while ((opt = getopt_long(argc, argv, kShortOptions, kLongOptions, &long_index)) != -1) {
        std::string_view arg = optarg ? optarg : "";
        switch (opt) {
        case 'i':
            options.input.url = arg;
            break;
        case 'f':
            options.input.format = arg;
            break;
        case 'o': {
            auto eq = arg.find('=');
            if (eq == std::string_view::npos || eq == 0) {
                return fail(std::format("input option '{}' must have the form KEY=VALUE", arg));
            }
            options.input.options.emplace_back(std::string(arg.substr(0, eq)), std::string(arg.substr(eq + 1)));
            break;
        }
        case 'l':
            options.loop = true;
            break;
        case kVideoStream:
            if (!ParseNumber(arg, 0, 65535, options.input.video_stream)) {
                return fail(std::format("invalid video stream index '{}'", arg));
            }
            break;
        case kAudioStream:
            if (!ParseNumber(arg, 0, 65535, options.input.audio_stream)) {
                return fail(std::format("invalid audio stream index '{}'", arg));
            }
            break;
        case kNoVideo:
            options.input.disable_video = true;
            break;
        case kNoAudio:
            options.input.disable_audio = true;
            break;
        case 't':
            options.decoder.hwaccel = arg == "software" ? "none" : std::string(arg);
            break;
        case kHwaccelDevice:
            options.decoder.hwaccel_device = arg;
            break;
        case kThreads:
            if (!ParseNumber(arg, 0, 128, options.decoder.threads)) {
                return fail(std::format("invalid thread count '{}' (expected 0-128)", arg));
            }
            break;
        case kVideoFilter:
            options.video_filter = arg;
            break;
        case 'p': {
            auto format = av::ParseOutputPixelFormat(arg);
            if (!format) {
                return fail(std::format("invalid pixel format '{}' (expected auto, uyvy, nv12, i420, bgra or rgba)",
                                        arg));
            }
            options.pixel_format = *format;
            break;
        }
        case kAudioRate:
            if (!ParseNumber(arg, 8000, 384000, options.audio_rate)) {
                return fail(std::format("invalid audio rate '{}' (expected 8000-384000)", arg));
            }
            break;
        case kAudioChannels:
            if (!ParseNumber(arg, 1, 64, options.audio_channels)) {
                return fail(std::format("invalid audio channel count '{}' (expected 1-64)", arg));
            }
            break;
        case 's':
            options.sender.name = arg;
            break;
        case 'g':
            options.sender.groups = arg;
            break;
        case kNdiLib:
            options.ndi_library = arg;
            break;
        case kNoPacing:
            options.pacing = false;
            break;
        case kStats:
            options.stats_interval = 5;
            if (optarg && !ParseNumber(arg, 1, 86400, options.stats_interval)) {
                return fail(std::format("invalid statistics interval '{}'", arg));
            }
            break;
        case 'v':
            verbosity++;
            break;
        case 'q':
            verbosity--;
            break;
        case 'h':
            message = Usage(program);
            return ParseOutcome::ExitSuccess;
        case 'V':
            message = std::format("ndistreamer {}\nFFmpeg {} (libavformat {}.{}, libavcodec {}.{})", NDISTREAMER_VERSION,
                                  av_version_info(), LIBAVFORMAT_VERSION_MAJOR, LIBAVFORMAT_VERSION_MINOR,
                                  LIBAVCODEC_VERSION_MAJOR, LIBAVCODEC_VERSION_MINOR);
            return ParseOutcome::ExitSuccess;
        case ':':
            return fail(std::format("option '{}' requires an argument", argv[optind - 1]));
        default:
            return fail(std::format("unrecognized option '{}'", argv[optind - 1]));
        }
    }

    if (optind < argc) return fail(std::format("unexpected argument '{}'", argv[optind]));
    if (options.input.url.empty()) return fail("an input is required (-i INPUT)");
    if (options.sender.name.empty()) return fail("the NDI source name cannot be empty");
    if (options.input.disable_video && options.input.disable_audio) {
        return fail("--no-video and --no-audio cannot be combined");
    }

    int level = std::clamp(static_cast<int>(log::Level::Info) + verbosity, static_cast<int>(log::Level::Error),
                           static_cast<int>(log::Level::Trace));
    options.log_level = static_cast<log::Level>(level);

    return ParseOutcome::Run;
}

} // namespace ndistreamer
