/**
 * @file test_streamer.cpp
 * @brief Whole-pipeline tests: real media files streamed into the fake NDI runtime.
 */

#include "support/test_support.hpp"

#include <gtest/gtest.h>

#include <cmath>

using namespace ndistreamer;
using namespace std::chrono_literals;

namespace {

constexpr int64_t kFrame25 = 400000; // 40 ms in NDI's 100 ns units

// Offset between when something was sent and its timecode. Constant across every frame of both
// streams when pacing is exact and audio/video are aligned.
std::vector<double> SendOffsetsMs(const test::FakeLog &log) {
    std::vector<double> offsets;
    for (const auto &v : log.video) offsets.push_back(v.ns / 1e6 - v.timecode / 1e4);
    for (const auto &a : log.audio) offsets.push_back(a.ns / 1e6 - a.timecode / 1e4);
    return offsets;
}

} // namespace

TEST(Streamer, StreamsAudioAndVideoInRealTime) {
    REQUIRE_MEDIA("av.mp4");
    auto log = test::Stream(test::MakeOptions("av.mp4"));

    EXPECT_EQ(log.create, "test||0|0") << "NDI's own clocking must be disabled";
    ASSERT_EQ(log.video.size(), 50u);
    for (const auto &v : log.video) {
        ASSERT_EQ(v.fourcc, "I420"); // 4:2:0 sources pass through by default
        ASSERT_EQ(v.width, 640);
        ASSERT_EQ(v.height, 360);
        ASSERT_EQ(v.fps_num, 25);
        ASSERT_EQ(v.fps_den, 1);
        ASSERT_EQ(v.format_type, 1); // progressive
    }
    EXPECT_NEAR(static_cast<double>(log.TotalAudioSamples()), 96000.0, 1024.0);
    EXPECT_EQ(log.audio.front().sample_rate, 48000);
    EXPECT_EQ(log.audio.front().channels, 2);
    EXPECT_GT(log.audio[10].rms, 0.05);

    // Two seconds of media take two seconds to send.
    double span_ms = (log.video.back().ns - log.video.front().ns) / 1e6;
    EXPECT_NEAR(span_ms, 1960.0, 60.0);

    // Every frame goes out on schedule, and audio is aligned with video.
    auto offsets = SendOffsetsMs(log);
    double median = test::Median(offsets);
    for (double offset : offsets) ASSERT_NEAR(offset, median, 25.0);

    for (size_t i = 1; i < log.video.size(); i++) ASSERT_EQ(log.video[i].timecode - log.video[i - 1].timecode, kFrame25);

    EXPECT_GE(log.flushes, 1);
    EXPECT_TRUE(log.destroyed);
}

TEST(Streamer, NoPacingSendsAsFastAsPossible) {
    REQUIRE_MEDIA("av.mp4");
    auto options = test::MakeOptions("av.mp4");
    options.pacing = false;

    auto start = std::chrono::steady_clock::now();
    auto log = test::Stream(options);
    EXPECT_LT(std::chrono::steady_clock::now() - start, 1500ms);
    EXPECT_EQ(log.video.size(), 50u);
    EXPECT_NEAR(static_cast<double>(log.TotalAudioSamples()), 96000.0, 1024.0);
}

TEST(Streamer, LoopsWithoutGapsOrOverlaps) {
    REQUIRE_MEDIA("video_only.mkv");
    auto options = test::MakeOptions("video_only.mkv");
    options.loop = true;
    options.pacing = false;

    auto log = test::Stream(options, 1500ms);
    ASSERT_GT(log.video.size(), 45u * 3) << "expected several passes";

    // 30000/1001 fps is 333667 (100 ns units) per frame; Matroska rounds timestamps to 1 ms.
    for (size_t i = 1; i < log.video.size(); i++) {
        int64_t delta = log.video[i].timecode - log.video[i - 1].timecode;
        ASSERT_NEAR(static_cast<double>(delta), 333667.0, 10000.0) << "at frame " << i;
    }
}

TEST(Streamer, LoopKeepsAudioContinuous) {
    REQUIRE_MEDIA("av.mp4");
    auto options = test::MakeOptions("av.mp4");
    options.loop = true;
    options.pacing = false;

    auto log = test::Stream(options, 1000ms);
    ASSERT_GT(log.video.size(), 100u) << "expected more than one pass";

    for (size_t i = 1; i < log.video.size(); i++) {
        int64_t delta = log.video[i].timecode - log.video[i - 1].timecode;
        // At the loop point the shorter stream may wait for the longer one, but never overlap it.
        ASSERT_GE(delta, kFrame25) << "at frame " << i;
        ASSERT_LE(delta, kFrame25 + 250000) << "at frame " << i;
    }
    for (size_t i = 1; i < log.audio.size(); i++) {
        const auto &prev = log.audio[i - 1];
        int64_t prev_end = prev.timecode + static_cast<int64_t>(prev.samples) * 10000000 / prev.sample_rate;
        // Timestamps carry microsecond precision; timecodes are in 100 ns units.
        ASSERT_GE(log.audio[i].timecode, prev_end - 10) << "audio overlaps at block " << i;
        ASSERT_LE(log.audio[i].timecode, prev_end + 250000) << "audio gap at block " << i;
    }
}

TEST(Streamer, VideoOnlyInput) {
    REQUIRE_MEDIA("video_only.mkv");
    auto options = test::MakeOptions("video_only.mkv");
    options.pacing = false;
    auto log = test::Stream(options);

    EXPECT_TRUE(log.audio.empty());
    ASSERT_EQ(log.video.size(), 45u);
    EXPECT_EQ(log.video[0].fps_num, 30000);
    EXPECT_EQ(log.video[0].fps_den, 1001);
}

TEST(Streamer, AudioOnlyInputKeepsAllChannels) {
    REQUIRE_MEDIA("audio_51.mka");
    auto log = test::Stream(test::MakeOptions("audio_51.mka"));

    EXPECT_TRUE(log.video.empty());
    ASSERT_FALSE(log.audio.empty());
    EXPECT_EQ(log.audio[0].channels, 6);
    EXPECT_EQ(log.audio[0].sample_rate, 48000);
    EXPECT_NEAR(static_cast<double>(log.TotalAudioSamples()), 72000.0, 1536.0);

    // Paced in real time even without video.
    double span_ms = (log.audio.back().ns - log.audio.front().ns) / 1e6;
    EXPECT_NEAR(span_ms, 1500.0, 80.0);
}

TEST(Streamer, ConvertsAndResamplesAudio) {
    REQUIRE_MEDIA("audio_s16_mono.wav");
    auto options = test::MakeOptions("audio_s16_mono.wav");
    options.pacing = false;
    options.audio_rate = 48000;
    options.audio_channels = 2;
    auto log = test::Stream(options);

    ASSERT_FALSE(log.audio.empty());
    EXPECT_EQ(log.audio[0].sample_rate, 48000);
    EXPECT_EQ(log.audio[0].channels, 2);
    EXPECT_NEAR(static_cast<double>(log.TotalAudioSamples()), 72000.0, 512.0);
    EXPECT_GT(log.audio[5].rms, 0.03);
}

TEST(Streamer, MarksInterlacedVideo) {
    REQUIRE_MEDIA("interlaced.ts");
    auto options = test::MakeOptions("interlaced.ts");
    options.pacing = false;
    auto log = test::Stream(options);

    ASSERT_EQ(log.video.size(), 25u);
    for (const auto &v : log.video) ASSERT_EQ(v.format_type, 0) << "expected NDI interleaved frames";
}

TEST(Streamer, DelayedAudioKeepsItsOffset) {
    REQUIRE_MEDIA("late_audio.mkv");
    auto log = test::Stream(test::MakeOptions("late_audio.mkv"));
    ASSERT_FALSE(log.video.empty());
    ASSERT_FALSE(log.audio.empty());

    double timecode_ms = (log.audio.front().timecode - log.video.front().timecode) / 1e4;
    double sent_ms = (log.audio.front().ns - log.video.front().ns) / 1e6;
    EXPECT_NEAR(timecode_ms, 500.0, 25.0);
    EXPECT_NEAR(sent_ms, 500.0, 50.0);
}

TEST(Streamer, AutoPixelFormatPassesBgraThrough) {
    REQUIRE_MEDIA("bgra.mkv");
    auto options = test::MakeOptions("bgra.mkv");
    options.pacing = false;
    auto log = test::Stream(options);

    ASSERT_EQ(log.video.size(), 10u);
    EXPECT_EQ(log.video[0].fourcc, "BGRA");
    EXPECT_GE(log.video[0].stride, 160 * 4);
}

TEST(Streamer, HonoursRequestedPixelFormat) {
    REQUIRE_MEDIA("av.mp4");
    for (auto [format, fourcc] : {std::pair{av::OutputPixelFormat::UYVY, "UYVY"},
                                  std::pair{av::OutputPixelFormat::NV12, "NV12"},
                                  std::pair{av::OutputPixelFormat::I420, "I420"},
                                  std::pair{av::OutputPixelFormat::BGRA, "BGRA"}}) {
        auto options = test::MakeOptions("av.mp4");
        options.pacing = false;
        options.input.disable_audio = true;
        options.pixel_format = format;
        auto log = test::Stream(options);
        ASSERT_EQ(log.video.size(), 50u) << fourcc;
        EXPECT_EQ(log.video[0].fourcc, fourcc);
    }
}

TEST(Streamer, AppliesVideoFilters) {
    REQUIRE_MEDIA("av.mp4");
    auto options = test::MakeOptions("av.mp4");
    options.pacing = false;
    options.video_filter = "scale=320:180,fps=50";
    auto log = test::Stream(options);

    ASSERT_GE(log.video.size(), 99u);
    EXPECT_EQ(log.video[0].width, 320);
    EXPECT_EQ(log.video[0].height, 180);
    EXPECT_EQ(log.video[0].fps_num, 50);
}

TEST(Streamer, TimesStreamsWithoutContainerTimestamps) {
    REQUIRE_MEDIA("raw.m4v");
    auto options = test::MakeOptions("raw.m4v");
    options.pacing = false;
    auto log = test::Stream(options);

    ASSERT_EQ(log.video.size(), 25u);
    for (size_t i = 1; i < log.video.size(); i++) ASSERT_EQ(log.video[i].timecode - log.video[i - 1].timecode, kFrame25);
}

TEST(Streamer, AutomaticHardwareDecodingFallsBackToSoftware) {
    REQUIRE_MEDIA("av.mp4");
    auto options = test::MakeOptions("av.mp4");
    options.pacing = false;
    options.decoder.hwaccel = "auto";
    auto log = test::Stream(options);
    EXPECT_EQ(log.video.size(), 50u);
}

TEST(Streamer, StopsPromptlyWhenAsked) {
    REQUIRE_MEDIA("av.mp4");
    auto options = test::MakeOptions("av.mp4");
    options.loop = true;

    auto start = std::chrono::steady_clock::now();
    auto log = test::Stream(options, 400ms);
    EXPECT_LT(std::chrono::steady_clock::now() - start, 1500ms);
    EXPECT_FALSE(log.video.empty());
    EXPECT_TRUE(log.destroyed);
}

TEST(Streamer, ReportsSetupErrors) {
    REQUIRE_MEDIA("av.mp4");
    auto expect_error = [](const Options &options, const std::string &text) {
        try {
            test::Stream(options);
            ADD_FAILURE() << "expected an error containing '" << text << "'";
        } catch (const std::exception &error) {
            EXPECT_NE(std::string(error.what()).find(text), std::string::npos) << error.what();
        }
    };

    auto hwaccel = test::MakeOptions("av.mp4");
    hwaccel.decoder.hwaccel = "bogus";
    expect_error(hwaccel, "unknown hardware acceleration 'bogus'");

    expect_error(test::MakeOptions("missing.mp4"), "cannot open");

    auto filter = test::MakeOptions("av.mp4");
    filter.video_filter = "nonsense_filter";
    expect_error(filter, "parsing video filter");

    auto runtime = test::MakeOptions("av.mp4");
    runtime.ndi_library = "/nonexistent/libndi.so.6";
    expect_error(runtime, "cannot load the NDI runtime");
}

TEST(Streamer, JoinsStreamsMidGop) {
    REQUIRE_MEDIA("midgop.ts");
    auto options = test::MakeOptions("midgop.ts");
    options.pacing = false;

    // Hundreds of undecodable packets precede the first keyframe; that is not a reason to give up.
    auto log = test::Stream(options);
    EXPECT_GE(log.video.size(), 40u);
}

TEST(Streamer, HonoursLongGapsInFiles) {
    REQUIRE_MEDIA("slideshow.mkv");
    auto log = test::Stream(test::MakeOptions("slideshow.mkv"));

    ASSERT_EQ(log.video.size(), 3u);
    double gap_s = (log.video[2].ns - log.video[1].ns) / 1e9;
    EXPECT_NEAR(gap_s, 5.25, 0.15) << "a pause in a file must not be treated as a discontinuity";
}

TEST(Streamer, SlowVideoDoesNotStarveAudio) {
    REQUIRE_MEDIA("av_60fps.mp4");
    auto options = test::MakeOptions("av_60fps.mp4");
    // Far slower than real time on any machine (about 4 fps on a 32-core desktop).
    options.video_filter = "scale=1280:720:flags=lanczos,nlmeans,scale=320:180";

    auto log = test::Stream(options);
    ASSERT_FALSE(log.video.empty());
    EXPECT_NEAR(static_cast<double>(log.TotalAudioSamples()), 240000.0, 2048.0);

    // Audio is still delivered in real time, on a single uninterrupted schedule.
    double span_s = (log.audio.back().ns - log.audio.front().ns) / 1e9;
    EXPECT_NEAR(span_s, 4.98, 0.3);
    std::vector<double> offsets;
    for (const auto &a : log.audio) offsets.push_back(a.ns / 1e6 - a.timecode / 1e4);
    double median = test::Median(offsets);
    for (double offset : offsets) ASSERT_NEAR(offset, median, 60.0);
}

TEST(Streamer, LoopsMp3WithoutGaps) {
    REQUIRE_MEDIA("loop.mp3");
    auto options = test::MakeOptions("loop.mp3");
    options.loop = true;
    options.pacing = false;

    auto log = test::Stream(options, 500ms);
    ASSERT_GT(log.TotalAudioSamples(), 44100 * 3) << "expected several passes";
    for (size_t i = 1; i < log.audio.size(); i++) {
        const auto &prev = log.audio[i - 1];
        int64_t prev_end = prev.timecode + static_cast<int64_t>(prev.samples) * 10000000 / prev.sample_rate;
        // Encoder delay and padding are trimmed by the decoder and must not leave holes at the loop point.
        ASSERT_NEAR(static_cast<double>(log.audio[i].timecode - prev_end), 0.0, 20.0) << "at block " << i;
    }
}
