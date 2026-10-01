/**
 * @file test_input.cpp
 * @brief Stream selection and the continuous loop timeline.
 */

#include "av/input.hpp"
#include "support/test_support.hpp"

#include <gtest/gtest.h>

#include <algorithm>

using namespace ndistreamer;

namespace {

av::InputConfig Config(const std::string &media) {
    av::InputConfig config;
    config.url = test::MediaPath(media);
    return config;
}

std::string ErrorFrom(const av::InputConfig &config) {
    try {
        av::MediaInput input(config, std::stop_token{});
    } catch (const std::exception &error) {
        return error.what();
    }
    return {};
}

} // namespace

TEST(MediaInput, SelectsBestAudioAndVideoStreams) {
    REQUIRE_MEDIA("av.mp4");
    av::MediaInput input(Config("av.mp4"), std::stop_token{});
    ASSERT_NE(input.VideoStream(), nullptr);
    ASSERT_NE(input.AudioStream(), nullptr);
    EXPECT_EQ(input.VideoStream()->index, 0);
    EXPECT_EQ(input.AudioStream()->index, 1);
    EXPECT_EQ(av_cmp_q(input.VideoFrameRate(), AVRational{25, 1}), 0);
}

TEST(MediaInput, HandlesSingleStreamInputs) {
    REQUIRE_MEDIA("video_only.mkv");
    REQUIRE_MEDIA("audio_51.mka");

    av::MediaInput video(Config("video_only.mkv"), std::stop_token{});
    EXPECT_NE(video.VideoStream(), nullptr);
    EXPECT_EQ(video.AudioStream(), nullptr);
    EXPECT_EQ(av_cmp_q(video.VideoFrameRate(), AVRational{30000, 1001}), 0);

    av::MediaInput audio(Config("audio_51.mka"), std::stop_token{});
    EXPECT_EQ(audio.VideoStream(), nullptr);
    EXPECT_NE(audio.AudioStream(), nullptr);
}

TEST(MediaInput, StreamsCanBeDisabled) {
    REQUIRE_MEDIA("av.mp4");
    auto config = Config("av.mp4");
    config.disable_audio = true;
    av::MediaInput input(config, std::stop_token{});
    EXPECT_NE(input.VideoStream(), nullptr);
    EXPECT_EQ(input.AudioStream(), nullptr);

    // Only packets of selected streams are returned.
    while (auto packet = input.Read()) EXPECT_EQ(packet->stream_index, 0);
}

TEST(MediaInput, ReportsUnusableSelections) {
    REQUIRE_MEDIA("av.mp4");
    REQUIRE_MEDIA("audio_51.mka");

    auto wrong_type = Config("av.mp4");
    wrong_type.video_stream = 1;
    EXPECT_NE(ErrorFrom(wrong_type).find("stream #1 is not a video stream"), std::string::npos);

    auto missing = Config("av.mp4");
    missing.audio_stream = 7;
    EXPECT_NE(ErrorFrom(missing).find("does not exist"), std::string::npos);

    auto nothing = Config("audio_51.mka");
    nothing.disable_audio = true;
    EXPECT_NE(ErrorFrom(nothing).find("no usable audio or video"), std::string::npos);

    auto format = Config("av.mp4");
    format.format = "no_such_format";
    EXPECT_NE(ErrorFrom(format).find("unknown input format"), std::string::npos);

    EXPECT_NE(ErrorFrom(Config("does-not-exist.mp4")).find("cannot open"), std::string::npos);
}

TEST(MediaInput, RestartContinuesTheTimeline) {
    REQUIRE_MEDIA("video_only.mkv");
    av::MediaInput input(Config("video_only.mkv"), std::stop_token{});
    AVRational time_base = input.VideoStream()->time_base;

    auto read_pass = [&] {
        std::vector<int64_t> pts;
        while (auto packet = input.Read()) pts.push_back(av::ToMicros(packet->pts, time_base));
        std::sort(pts.begin(), pts.end());
        return pts;
    };

    auto first = read_pass();
    ASSERT_TRUE(input.Restart());
    auto second = read_pass();

    ASSERT_EQ(first.size(), second.size());
    ASSERT_FALSE(first.empty());
    int64_t frame_us = av::ToMicros(1, av_inv_q(input.VideoFrameRate()));
    int64_t pass_us = first.back() - first.front() + frame_us;
    for (size_t i = 0; i < first.size(); i++) {
        EXPECT_NEAR(static_cast<double>(second[i] - first[i]), static_cast<double>(pass_us), 1000) << i;
    }
}

TEST(MediaInput, UnknownInputOptionsAreNotFatal) {
    REQUIRE_MEDIA("av.mp4");
    auto config = Config("av.mp4");
    config.options.emplace_back("no_such_option", "1");
    EXPECT_NO_THROW(av::MediaInput(config, std::stop_token{}));
}
