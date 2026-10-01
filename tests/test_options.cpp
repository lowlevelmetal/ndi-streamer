/**
 * @file test_options.cpp
 * @brief Command line parsing and validation.
 */

#include "options.hpp"

#include <gtest/gtest.h>

#include <initializer_list>
#include <string>
#include <vector>

using namespace ndistreamer;

namespace {

struct Parsed {
    ParseOutcome outcome;
    Options options;
    std::string message;
};

Parsed Parse(std::initializer_list<const char *> args) {
    std::vector<std::string> storage{"ndistreamer"};
    storage.insert(storage.end(), args.begin(), args.end());
    std::vector<char *> argv;
    for (auto &arg : storage) argv.push_back(arg.data());
    argv.push_back(nullptr);

    Parsed parsed;
    parsed.outcome = ParseOptions(static_cast<int>(storage.size()), argv.data(), parsed.options, parsed.message);
    return parsed;
}

} // namespace

TEST(Options, AcceptsLegacyCommandLine) {
    auto parsed = Parse({"-i", "movie.mp4", "-s", "Lobby", "-t", "software"});
    ASSERT_EQ(parsed.outcome, ParseOutcome::Run) << parsed.message;
    EXPECT_EQ(parsed.options.input.url, "movie.mp4");
    EXPECT_EQ(parsed.options.sender.name, "Lobby");
    EXPECT_EQ(parsed.options.decoder.hwaccel, "none");

    EXPECT_EQ(Parse({"-i", "a.mp4", "-t", "vaapi"}).options.decoder.hwaccel, "vaapi");
    EXPECT_EQ(Parse({"-i", "a.mp4", "-t", "cuda"}).options.decoder.hwaccel, "cuda");
}

TEST(Options, HasSensibleDefaults) {
    auto parsed = Parse({"-i", "movie.mp4"});
    ASSERT_EQ(parsed.outcome, ParseOutcome::Run);
    const Options &o = parsed.options;
    EXPECT_EQ(o.sender.name, "NDI Source");
    EXPECT_EQ(o.decoder.hwaccel, "none");
    EXPECT_EQ(o.pixel_format, av::OutputPixelFormat::Auto);
    EXPECT_TRUE(o.pacing);
    EXPECT_FALSE(o.loop);
    EXPECT_EQ(o.stats_interval, 0);
    EXPECT_EQ(o.log_level, log::Level::Info);
}

TEST(Options, ParsesLongOptions) {
    auto parsed = Parse({"--input", "rtsp://cam", "--format", "rtsp", "-o", "rtsp_transport=tcp", "-o", "timeout=5",
                         "--loop", "--video-stream", "2", "--audio-stream", "3", "--hwaccel", "auto",
                         "--hwaccel-device", "/dev/dri/renderD128", "--threads", "4", "--vf", "yadif",
                         "--pixel-format", "nv12", "--audio-rate", "48000", "--audio-channels", "2", "--name", "Cam",
                         "--groups", "studio,public", "--ndi-lib", "/opt/ndi/lib", "--no-pacing", "--stats=2"});
    ASSERT_EQ(parsed.outcome, ParseOutcome::Run) << parsed.message;
    const Options &o = parsed.options;
    EXPECT_EQ(o.input.url, "rtsp://cam");
    EXPECT_EQ(o.input.format, "rtsp");
    ASSERT_EQ(o.input.options.size(), 2u);
    EXPECT_EQ(o.input.options[0], (std::pair<std::string, std::string>{"rtsp_transport", "tcp"}));
    EXPECT_EQ(o.input.options[1], (std::pair<std::string, std::string>{"timeout", "5"}));
    EXPECT_TRUE(o.loop);
    EXPECT_EQ(o.input.video_stream, 2);
    EXPECT_EQ(o.input.audio_stream, 3);
    EXPECT_EQ(o.decoder.hwaccel, "auto");
    EXPECT_EQ(o.decoder.hwaccel_device, "/dev/dri/renderD128");
    EXPECT_EQ(o.decoder.threads, 4);
    EXPECT_EQ(o.video_filter, "yadif");
    EXPECT_EQ(o.pixel_format, av::OutputPixelFormat::NV12);
    EXPECT_EQ(o.audio_rate, 48000);
    EXPECT_EQ(o.audio_channels, 2);
    EXPECT_EQ(o.sender.name, "Cam");
    EXPECT_EQ(o.sender.groups, "studio,public");
    EXPECT_EQ(o.ndi_library, "/opt/ndi/lib");
    EXPECT_FALSE(o.pacing);
    EXPECT_EQ(o.stats_interval, 2);
}

TEST(Options, StatsDefaultsToFiveSeconds) {
    EXPECT_EQ(Parse({"-i", "a.mp4", "--stats"}).options.stats_interval, 5);
}

TEST(Options, VerbosityIsClamped) {
    EXPECT_EQ(Parse({"-i", "a", "-v"}).options.log_level, log::Level::Debug);
    EXPECT_EQ(Parse({"-i", "a", "-vvvvv"}).options.log_level, log::Level::Trace);
    EXPECT_EQ(Parse({"-i", "a", "-q"}).options.log_level, log::Level::Warn);
    EXPECT_EQ(Parse({"-i", "a", "-qqqq"}).options.log_level, log::Level::Error);
}

TEST(Options, HelpAndVersionExitSuccessfully) {
    auto help = Parse({"--help"});
    EXPECT_EQ(help.outcome, ParseOutcome::ExitSuccess);
    EXPECT_NE(help.message.find("Usage: ndistreamer -i INPUT"), std::string::npos);

    auto version = Parse({"-V"});
    EXPECT_EQ(version.outcome, ParseOutcome::ExitSuccess);
    EXPECT_NE(version.message.find("ndistreamer 2."), std::string::npos);
    EXPECT_NE(version.message.find("FFmpeg"), std::string::npos);
}

TEST(Options, RejectsInvalidUsage) {
    struct Case {
        std::initializer_list<const char *> args;
        const char *error;
    };
    const Case cases[] = {
        {{}, "an input is required"},
        {{"-i"}, "requires an argument"},
        {{"-i", "a", "--bogus"}, "unrecognized option"},
        {{"-i", "a", "extra"}, "unexpected argument"},
        {{"-i", "a", "-o", "novalue"}, "KEY=VALUE"},
        {{"-i", "a", "-o", "=x"}, "KEY=VALUE"},
        {{"-i", "a", "--threads", "-1"}, "invalid thread count"},
        {{"-i", "a", "--threads", "four"}, "invalid thread count"},
        {{"-i", "a", "--audio-rate", "100"}, "invalid audio rate"},
        {{"-i", "a", "--audio-channels", "0"}, "invalid audio channel count"},
        {{"-i", "a", "--video-stream", "x"}, "invalid video stream"},
        {{"-i", "a", "-p", "yuv"}, "invalid pixel format"},
        {{"-i", "a", "--stats=0"}, "invalid statistics interval"},
        {{"-i", "a", "-s", ""}, "cannot be empty"},
        {{"-i", "a", "--no-video", "--no-audio"}, "cannot be combined"},
    };

    for (const auto &c : cases) {
        auto parsed = Parse(c.args);
        EXPECT_EQ(parsed.outcome, ParseOutcome::ExitError) << c.error;
        EXPECT_NE(parsed.message.find(c.error), std::string::npos) << "expected '" << c.error << "' in: " << parsed.message;
        EXPECT_NE(parsed.message.find("--help"), std::string::npos);
    }
}
